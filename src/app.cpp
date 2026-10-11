#include "app.h"
#include <shlobj.h>
#include <dwmapi.h>
#include <fstream>
namespace sd {
static HICON makeIcon() {
    HDC dc=GetDC(nullptr),mem=CreateCompatibleDC(dc);HBITMAP bmp=CreateCompatibleBitmap(dc,32,32),mask=CreateBitmap(32,32,1,1,nullptr);auto old=SelectObject(mem,bmp);
    RECT r{0,0,32,32};auto bg=CreateSolidBrush(RGB(19,28,42));FillRect(mem,&r,bg);DeleteObject(bg);
    auto pen=CreatePen(PS_SOLID,3,RGB(117,226,194));auto op=SelectObject(mem,pen);auto ob=SelectObject(mem,GetStockObject(NULL_BRUSH));RoundRect(mem,5,5,27,25,5,5);MoveToEx(mem,10,28,nullptr);LineTo(mem,15,25);SelectObject(mem,ob);SelectObject(mem,op);DeleteObject(pen);SelectObject(mem,old);
    ICONINFO info{TRUE,0,0,mask,bmp};HICON icon=CreateIconIndirect(&info);DeleteObject(mask);DeleteObject(bmp);DeleteDC(mem);ReleaseDC(nullptr,dc);return icon;
}
App::~App() { shutdown(); }
bool App::init(fs::path directory,bool testing) {
    testMode=testing;data=std::move(directory);std::error_code ec;fs::create_directories(data/L"history",ec);
    if(ec) { if(testing)return false;PWSTR local=nullptr;if(FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&local)))return false;data=fs::path(local)/L"ScreenDock";CoTaskMemFree(local);ec.clear();fs::create_directories(data/L"history",ec);if(ec)return false; }
    history=data/L"history";config=data/L"settings.ini";
    if(!historyIndex_.load(data/L"history-index.txt"))return false;
    edge=Edge(std::clamp(GetPrivateProfileIntW(L"dock",L"edge",1,config.c_str()),0u,3u));
    monitor=GetPrivateProfileIntW(L"dock",L"monitor",0,config.c_str());autoPin=GetPrivateProfileIntW(L"dock",L"autoPin",1,config.c_str())!=0;
    updateMonitors();if(displays.empty())return false;
    icon_=makeIcon();dock=std::make_unique<Dock>(*this);if(!dock->create())return false;
    worker_=std::make_unique<Worker>();loadHistory();
    if(!testMode) {
        NOTIFYICONDATAW tray{};tray.cbSize=sizeof(tray);tray.hWnd=dock->hwnd;tray.uID=1;tray.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP;tray.uCallbackMessage=TrayMessage;tray.hIcon=icon_;wcscpy_s(tray.szTip,L"ScreenDock · 截图便签\nCtrl + Alt + A 截图");trayAdded_=Shell_NotifyIconW(NIM_ADD,&tray)!=FALSE;
        hotkeyRegistered=RegisterHotKey(dock->hwnd,HotkeyId,MOD_CONTROL|MOD_ALT|MOD_NOREPEAT,'A')!=FALSE;
        ShowWindow(dock->hwnd,SW_SHOWNOACTIVATE);if(!hotkeyRegistered)notify(L"Ctrl+Alt+A 已被其他程序占用，请使用边栏截图按钮。",true);
    }
    return true;
}
void App::loadHistory() {
    records.clear();std::error_code ec;
    for(fs::directory_iterator it(history,ec),last;!ec && it!=last;it.increment(ec)) {
        if(it->is_regular_file(ec) && it->path().extension()==L".png" && !historyIndex_.contains(it->path())) {auto r=std::make_shared<Record>();r->path=it->path();r->label=friendlyTime(r->path);records.push_back(std::move(r));}
    }
    std::sort(records.begin(),records.end(),[](const auto& a,const auto& b){return a->path.filename()>b->path.filename();});
    if(dock)dock->refresh(true);
}
void App::saveConfig() {auto put=[&](LPCWSTR key,int value){auto s=std::to_wstring(value);WritePrivateProfileStringW(L"dock",key,s.c_str(),config.c_str());};put(L"edge",int(edge));put(L"monitor",int(monitor));put(L"autoPin",autoPin);}
void App::updateMonitors() {displays=monitors();if(monitor>=displays.size())monitor=0;}
void App::notify(const std::wstring& value,bool error) {
    if(testMode){OutputDebugStringW(value.c_str());return;}
    NOTIFYICONDATAW tray{};tray.cbSize=sizeof(tray);tray.hWnd=dock->hwnd;tray.uID=1;tray.uFlags=NIF_INFO;tray.dwInfoFlags=error?NIIF_ERROR:NIIF_INFO;wcscpy_s(tray.szInfoTitle,L"ScreenDock");wcsncpy_s(tray.szInfo,value.c_str(),_TRUNCATE);
    if(!trayAdded_ || !Shell_NotifyIconW(NIM_MODIFY,&tray))MessageBoxW(dock->hwnd,value.c_str(),L"ScreenDock",MB_OK|(error?MB_ICONERROR:MB_ICONINFORMATION));
}
void App::openFolder() {ShellExecuteW(dock->hwnd,L"open",history.c_str(),nullptr,nullptr,SW_SHOWNORMAL);}
void App::openRecord(const fs::path& path) {std::wstring params=L"/select,\""+path.wstring()+L"\"";ShellExecuteW(dock->hwnd,L"open",L"explorer.exe",params.c_str(),nullptr,SW_SHOWNORMAL);}
void App::beginCapture() {if(capturing)return;capturing=true;ShowWindow(dock->hwnd,SW_HIDE);hidePins(true);SetTimer(dock->hwnd,CaptureTimer,140,nullptr);}
void App::captureNow() {
    KillTimer(dock->hwnd,CaptureTimer);DwmFlush();RECT area{GetSystemMetrics(SM_XVIRTUALSCREEN),GetSystemMetrics(SM_YVIRTUALSCREEN),0,0};area.right=area.left+GetSystemMetrics(SM_CXVIRTUALSCREEN);area.bottom=area.top+GetSystemMetrics(SM_CYVIRTUALSCREEN);
    auto pixels=captureDesktop(area);if(!pixels) {finishCapture({});notify(L"无法读取屏幕，请确认当前桌面可用。",true);return;}
    capture=std::make_unique<Capture>(*this,pixels,area);if(!capture->create()){finishCapture({});notify(L"无法创建截图选区。",true);}
}
void App::finishCapture(Picture picture) {
    capture.reset();capturing=false;dock->expand(false);if(!testMode)ShowWindow(dock->hwnd,SW_SHOWNOACTIVATE);hidePins(false);
    if(picture)add(std::move(picture),autoPin);
}
std::shared_ptr<Record> App::add(Picture image,bool pinNow) {
    if(!image || !image->valid())return {};
    auto r=std::make_shared<Record>();r->path=history/(stamp()+L"_"+std::to_wstring(sequence_.fetch_add(1))+L".png");r->label=friendlyTime(r->path);r->width=image->w;r->height=image->h;r->thumb=thumbnail(*image);r->pending=image;r->saving=true;records.insert(records.begin(),r);dock->refresh(true);
    HWND recipient=dock->hwnd;
    worker_->push([r,image,recipient] {bool good=false;try{good=savePng(*image,r->path);}catch(...){good=false;}auto* result=new SaveResult{r,good};if(!PostMessageW(recipient,SaveDone,0,reinterpret_cast<LPARAM>(result)))delete result;});
    if(pinNow)pin(r);return r;
}
void App::saveComplete(SaveResult* raw) {
    std::unique_ptr<SaveResult> result(raw);if(!result)return;auto record=result->record;
    record->saving=false;record->failed=!result->success;
    if(result->success){record->pending.reset();if(!historyIndex_.contains(record->path) && std::none_of(records.begin(),records.end(),[&](const auto& r){return r->path==record->path;})){records.push_back(record);std::sort(records.begin(),records.end(),[](const auto& a,const auto& b){return a->path.filename()>b->path.filename();});}}
    else notify(L"PNG 保存失败，图片仍留在内存中。请检查目录权限和磁盘空间；可在便签中复制原图。",true);
    dock->refresh();
}
void App::requestThumbnail(const std::shared_ptr<Record>& record) {
    if(!worker_ || record->previewLoading || record->previewFailed || record->saving || record->thumb)return;
    record->previewLoading=true;HWND recipient=dock->hwnd;
    worker_->push([record,recipient] {Picture image;try{image=readImage(record->path,640);}catch(...){image={};}auto* result=new ThumbResult{record,image};if(!PostMessageW(recipient,ThumbDone,0,reinterpret_cast<LPARAM>(result)))delete result;});
}
void App::pin(std::shared_ptr<Record> record) {
    for(auto& p:pins)if(p->path()==record->path) {p->show(true);SetWindowPos(p->hwnd,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);return;}
    Picture image=record->pending?record->pending:readImage(record->path);
    if(!image) {notify(L"无法读取这张图片，文件可能损坏或已经移走。",true);return;}
    auto p=std::make_unique<Pin>(*this,record,image);if(p->create(int(pins.size())))pins.push_back(std::move(p));else notify(L"无法创建置顶便签。",true);
}
void App::removePin(Pin* pinToRemove) {pins.erase(std::remove_if(pins.begin(),pins.end(),[&](const auto& p){return p.get()==pinToRemove;}),pins.end());if(dock)dock->refresh();}
bool App::hideRecord(const std::shared_ptr<Record>& record) {
    auto found=std::find(records.begin(),records.end(),record);if(found==records.end())return false;
    if(!historyIndex_.hide(record->path)){notify(L"无法保存列表索引，记录未移出。请检查数据目录权限。",true);return false;}
    records.erase(found);dock->refresh();return true;
}
bool App::restoreHistory() {
    if(!historyIndex_.restoreAll()){notify(L"无法恢复列表索引，请检查数据目录权限。",true);return false;}
    // Preserve records whose background PNG write has not completed yet.
    std::vector<std::shared_ptr<Record>> pending;for(auto& r:records)if(r->saving || r->failed)pending.push_back(r);
    loadHistory();for(auto& r:pending){records.erase(std::remove_if(records.begin(),records.end(),[&](const auto& x){return x->path==r->path;}),records.end());records.push_back(r);}
    std::sort(records.begin(),records.end(),[](const auto& a,const auto& b){return a->path.filename()>b->path.filename();});dock->refresh(true);return true;
}
bool App::copyRecordFile(const std::shared_ptr<Record>& record) {
    if(record->saving){notify(L"图片仍在保存，请稍后再复制文件。");return false;}
    if(record->failed || !copyFile(dock->hwnd,record->path)){notify(L"复制文件失败：原图可能已移走，或剪贴板正被其他程序占用。",true);return false;}
    return true;
}
void App::paste() {auto image=pastePicture(dock->hwnd);if(image)add(image,autoPin);else notify(L"剪贴板没有可读取的图片。先复制图片，再点“粘贴”。");}
void App::setEdge(Edge value) {edge=value;saveConfig();dock->reposition();}
void App::setMonitor(size_t index) {if(index<displays.size()){monitor=index;saveConfig();dock->reposition();}}
void App::hidePins(bool hidden) {for(auto& p:pins)p->show(!hidden && !testMode);}
void App::shutdown() {
    if(shuttingDown)return;shuttingDown=true;
    if(dock && dock->hwnd) {KillTimer(dock->hwnd,CaptureTimer);UnregisterHotKey(dock->hwnd,HotkeyId);}
    worker_.reset(); // Finish all queued PNG writes before exiting.
    if(dock && dock->hwnd) {MSG msg;while(PeekMessageW(&msg,dock->hwnd,SaveDone,SaveDone,PM_REMOVE))saveComplete(reinterpret_cast<SaveResult*>(msg.lParam));while(PeekMessageW(&msg,dock->hwnd,CaptureDone,CaptureDone,PM_REMOVE))delete reinterpret_cast<Picture*>(msg.lParam);while(PeekMessageW(&msg,dock->hwnd,ThumbDone,ThumbDone,PM_REMOVE))delete reinterpret_cast<ThumbResult*>(msg.lParam);}
    capture.reset();pins.clear();
    if(trayAdded_ && dock) {NOTIFYICONDATAW tray{};tray.cbSize=sizeof(tray);tray.hWnd=dock->hwnd;tray.uID=1;Shell_NotifyIconW(NIM_DELETE,&tray);trayAdded_=false;}
    dock.reset();if(icon_){DestroyIcon(icon_);icon_=nullptr;}
}
LRESULT App::controller(UINT msg,WPARAM wp,LPARAM lp,bool& handled) {
    handled=true;
    if(msg==RegisterWindowMessageW(L"TaskbarCreated") && !testMode) {NOTIFYICONDATAW tray{};tray.cbSize=sizeof(tray);tray.hWnd=dock->hwnd;tray.uID=1;tray.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP;tray.uCallbackMessage=TrayMessage;tray.hIcon=icon_;wcscpy_s(tray.szTip,L"ScreenDock · Ctrl+Alt+A 截图");trayAdded_=Shell_NotifyIconW(NIM_ADD,&tray)!=FALSE;return 0;}
    switch(msg) {
    case WM_HOTKEY:if(wp==HotkeyId)beginCapture();return 0;
    case WM_TIMER:if(wp==CaptureTimer){captureNow();return 0;}handled=false;return 0;
    case TrayMessage:if(lp==WM_LBUTTONDBLCLK)beginCapture();else if(lp==WM_LBUTTONUP){dock->expand(true);ShowWindow(dock->hwnd,SW_SHOWNOACTIVATE);}else if(lp==WM_RBUTTONUP){POINT p;GetCursorPos(&p);dock->showMenu(p);}return 0;
    case SaveDone:saveComplete(reinterpret_cast<SaveResult*>(lp));return 0;
    case CaptureDone:{std::unique_ptr<Picture> result(reinterpret_cast<Picture*>(lp));finishCapture(result?*result:Picture{});return 0;}
    case RemovePin:removePin(reinterpret_cast<Pin*>(lp));return 0;
    case ThumbDone:{std::unique_ptr<ThumbResult> result(reinterpret_cast<ThumbResult*>(lp));result->record->previewLoading=false;result->record->previewFailed=!result->image;result->record->thumb=std::move(result->image);dock->refresh();return 0;}
    case WM_DISPLAYCHANGE:updateMonitors();dock->reposition();return 0;
    case WM_QUERYENDSESSION:return TRUE;
    case WM_ENDSESSION:if(wp)PostQuitMessage(0);return 0;
    case WM_CLOSE:PostQuitMessage(0);return 0;
    default:handled=false;return 0;
    }
}
}
