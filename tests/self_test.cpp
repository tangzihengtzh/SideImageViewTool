#include "app.h"
#include <fstream>
#include <chrono>
#include <psapi.h>
#include <dwmapi.h>
namespace sd {
static void pump() {MSG msg;while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){if(msg.message!=WM_QUIT){TranslateMessage(&msg);DispatchMessageW(&msg);}}}
static bool waitSaved(const std::shared_ptr<Record>& record) {auto until=GetTickCount64()+6000;while(record->saving && GetTickCount64()<until){pump();MsgWaitForMultipleObjects(0,nullptr,FALSE,20,QS_ALLINPUT);}pump();return !record->saving && !record->failed;}
static Picture fixture(int revision) {
    auto image=std::make_shared<Pixels>();image->w=640;image->h=380;image->bgra.resize(size_t(image->w)*image->h*4);
    HDC dc=GetDC(nullptr),mem=CreateCompatibleDC(dc);BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=image->w;info.bmiHeader.biHeight=-LONG(image->h);info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;void* bits=nullptr;HBITMAP bmp=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);auto old=SelectObject(mem,bmp);
    RECT r{0,0,640,380};HBRUSH bg=CreateSolidBrush(RGB(248,250,253));FillRect(mem,&r,bg);DeleteObject(bg);SetBkMode(mem,TRANSPARENT);
    HFONT title=CreateFontW(-24,0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");auto oldFont=SelectObject(mem,title);SetTextColor(mem,RGB(22,41,67));TextOutW(mem,28,24,L"EXPERIMENT / SERIAL OUTPUT",26);SelectObject(mem,oldFont);DeleteObject(title);
    HFONT mono=CreateFontW(-18,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Consolas");SelectObject(mem,mono);
    const wchar_t* rows[]={L"> device connected: COM5 / 115200",L"> firmware: 1.04  |  sample rate: 100 Hz",L"[ OK ] sensor initialized",L"[ OK ] calibration loaded",L"temperature    24.60 C",L"pressure       101.32 kPa",L"samples        2048",L"status         PASS"};
    for(int i=0;i<8;++i){SetTextColor(mem,i==7?RGB(18,130,100):RGB(48,65,89));TextOutW(mem,28,80+i*31,rows[i],int(wcslen(rows[i])));}
    if(revision){SetTextColor(mem,RGB(200,72,65));TextOutW(mem,320,80+5*31,L"(+0.18 kPa)",11);}
    SelectObject(mem,oldFont);DeleteObject(mono);GdiFlush();memcpy(image->bgra.data(),bits,image->bgra.size());for(size_t i=3;i<image->bgra.size();i+=4)image->bgra[i]=255;SelectObject(mem,old);DeleteObject(bmp);DeleteDC(mem);ReleaseDC(nullptr,dc);return image;
}
int selfTest(HINSTANCE instance,const fs::path& directory) {
    std::error_code ec;fs::create_directories(directory,ec);if(ec)return 2;
    std::ofstream report(directory/L"report.txt",std::ios::trunc);if(!report)return 2;
    int failures=0,count=0;auto check=[&](bool condition,const char* label){++count;report<<(condition?"PASS ":"FAIL ")<<label<<"\n";report.flush();if(!condition)++failures;};
    RECT area=normalized({500,300},{40,50});check(area.left==40 && area.top==50 && area.right==500 && area.bottom==300,"reverse-drag rectangle normalization");
    check(zoomValue(1,120)>1 && zoomValue(1,-120)<1,"wheel zoom direction");check(zoomValue(8,120)==8 && zoomValue(.08f,-120)==.08f,"zoom bounds");
    RECT work{-1920,-200,0,880};
    for(int e=0;e<4;++e){RECT r=dockRect(work,Edge(e),false,144);check(r.left>=work.left && r.top>=work.top && r.right<=work.right && r.bottom<=work.bottom,"four-edge bounds at 150% DPI / negative monitor coordinates");}
    auto image=fixture(0),other=fixture(1);check(image && image->valid(),"opaque BGRA fixture");
    auto part=crop(*image,area);check(part && part->w==460 && part->h==250,"crop dimensions");
    check(part && memcmp(part->bgra.data(),image->bgra.data()+(size_t(50)*640+40)*4,460*4)==0,"crop exact pixel row");
    check(!crop(*image,{900,900,950,950}),"reject empty/outside crop");auto thumb=thumbnail(*image,180);check(thumb && thumb->w==180 && thumb->h==106,"WIC thumbnail aspect ratio");
    fs::path fixturePath=directory/(L"roundtrip_"+stamp()+L".png");check(savePng(*image,fixturePath),"PNG encode and atomic rename");auto decoded=readImage(fixturePath);check(decoded && decoded->w==640 && decoded->h==380 && decoded->bgra==image->bgra,"lossless PNG pixel roundtrip");check(!readImage(directory/L"missing.png"),"missing image rejected");
    check(!savePng(*other,fixturePath) && readImage(fixturePath)->bgra==image->bgra,"existing archive is never overwritten");check(!fs::exists(fixturePath.wstring()+L".partial"),"failed save removes temporary file");check(!savePng(*image,directory/L"nonexistent-directory"/L"save.png"),"unwritable target reports failure");
    {std::ofstream bad(directory/L"corrupt.png");bad<<"invalid PNG";}check(!readImage(directory/L"corrupt.png"),"corrupt image rejected");
    auto transparent=std::make_shared<Pixels>();transparent->w=2;transparent->h=1;transparent->bgra={40,60,80,128,0,0,0,0};auto alphaPath=directory/(L"alpha_"+stamp()+L".png");check(savePng(*transparent,alphaPath) && readImage(alphaPath) && readImage(alphaPath)->bgra[3]==128,"alpha PNG preserved");
    size_t archived=0;
    {
        App app(instance);check(app.init(directory/L"app-data",true),"native app initialization");if(!app.dock)return 3;
        app.autoPin=false;check(IsWindow(app.dock->hwnd)!=FALSE,"real dock HWND created");check((GetWindowLongPtrW(app.dock->hwnd,GWL_EXSTYLE)&WS_EX_TOPMOST)!=0,"dock always-on-top style");
        for(int e=0;e<4;++e){app.setEdge(Edge(e));app.dock->expand(true);RECT r;GetWindowRect(app.dock->hwnd,&r);auto m=app.displays[app.monitor].work;bool at=e==0?r.left==m.left:e==1?r.right==m.right:e==2?r.top==m.top:r.bottom==m.bottom;check(at,"native four-edge expanded positioning");app.dock->expand(false);}
        app.setEdge(Edge::Right);app.dock->expand(true);SendMessageW(app.dock->hwnd,WM_MOUSEWHEEL,MAKEWPARAM(0,120),0);check(app.dock->thumbnailSize()==140,"dock wheel enlarges previews");
        auto a=app.add(image,false);check(a && waitSaved(a) && fs::exists(a->path),"asynchronous PNG archive completion");auto b=app.add(other,false);check(b && waitSaved(b),"second independent screenshot archive");
        app.pin(a);app.pin(b);check(app.pins.size()==2,"multiple pinned image windows");app.pin(a);check(app.pins.size()==2,"pin same screenshot reuses existing window");
        if(!app.pins.empty()) {
            auto& pin=app.pins.front();check((GetWindowLongPtrW(pin->hwnd,GWL_EXSTYLE)&WS_EX_TOPMOST)!=0,"pin always-on-top style");RECT r;GetWindowRect(pin->hwnd,&r);POINT center{(r.left+r.right)/2,(r.top+r.bottom)/2};float before=pin->zoom();SendMessageW(pin->hwnd,WM_MOUSEWHEEL,MAKEWPARAM(0,120),MAKELPARAM(center.x,center.y));check(pin->zoom()>before,"pin wheel handler zooms image");
            SendMessageW(pin->hwnd,WM_LBUTTONDBLCLK,0,0);check(std::abs(pin->zoom()-1)<.001f,"double-click restores actual pixels");pin->resize(8,center);check(pin->zoom()==8,"8x zoom retained beyond screen size");GetWindowRect(pin->hwnd,&r);auto m=app.displays[app.monitor].work;check(r.right-r.left<=m.right-m.left && r.bottom-r.top<=m.bottom-m.top,"large image viewport stays within monitor");pin->resize(.65f,center);
            Graphics graphics;check(graphics.begin(pin->hwnd),"Direct2D hardware/native render target");if(graphics.rt){graphics.image(image,{0,0,200,120});graphics.text(L"ScreenDock 渲染测试",{0,120,240,160});check(SUCCEEDED(graphics.end()),"Direct2D bitmap and DirectWrite draw succeeds");graphics.reset();check(graphics.begin(pin->hwnd),"render resources recreate after reset");if(graphics.rt)graphics.end();}
            SendMessageW(pin->hwnd,WM_MOUSEWHEEL,MAKEWPARAM(MK_SHIFT,-120),0);check((GetWindowLongPtrW(pin->hwnd,GWL_EXSTYLE)&WS_EX_LAYERED)!=0,"shift wheel enables transparency");
        }
        app.capturing=true;app.capture=std::make_unique<Capture>(app,image,RECT{0,0,640,380});check(app.capture->create(),"selection overlay window created");
        if(app.capture->hwnd){SendMessageW(app.capture->hwnd,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(500,300));SendMessageW(app.capture->hwnd,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(40,50));SendMessageW(app.capture->hwnd,WM_LBUTTONUP,0,MAKELPARAM(40,50));pump();auto selected=app.records.front();check(waitSaved(selected) && selected->width==460 && selected->height==250,"real reverse-drag selection to archive pipeline");}
        size_t beforeCancel=app.records.size();app.capturing=true;app.capture=std::make_unique<Capture>(app,image,RECT{0,0,640,380});app.capture->create();SendMessageW(app.capture->hwnd,WM_KEYDOWN,VK_ESCAPE,0);pump();check(!app.capturing && !app.capture && app.records.size()==beforeCancel,"Escape cancels without creating history");
        if(!app.pins.empty()){HWND closing=app.pins.front()->hwnd;size_t before=app.pins.size();fs::path original=app.pins.front()->path();SendMessageW(closing,WM_CLOSE,0,0);pump();check(app.pins.size()==before-1 && fs::exists(original),"closing pin frees window and retains original PNG");}
        // Capture only this program's opaque synthetic UI for visual review.
        app.setEdge(Edge::Bottom);app.dock->expand(true);SetWindowDisplayAffinity(app.dock->hwnd,WDA_NONE);ShowWindow(app.dock->hwnd,SW_SHOWNOACTIVATE);UpdateWindow(app.dock->hwnd);DwmFlush();RECT horizontal;GetWindowRect(app.dock->hwnd,&horizontal);horizontal.left+=6;horizontal.top+=6;horizontal.right-=6;horizontal.bottom-=6;auto horizontalShot=captureDesktop(horizontal);check(horizontalShot && savePng(*horizontalShot,directory/(L"horizontal-preview_"+stamp()+L".png")),"native horizontal dock visual preview PNG");ShowWindow(app.dock->hwnd,SW_HIDE);app.setEdge(Edge::Right);
        if(!app.pins.empty()){auto& pin=app.pins.front();SetWindowDisplayAffinity(pin->hwnd,WDA_NONE);pin->show(true);UpdateWindow(pin->hwnd);DwmFlush();RECT r;GetWindowRect(pin->hwnd,&r);r.left+=6;r.top+=6;r.right-=6;r.bottom-=6;auto pinShot=captureDesktop(r);check(pinShot && savePng(*pinShot,directory/(L"pin-preview_"+stamp()+L".png")),"native pinned image visual preview PNG");pin->show(false);}
        app.dock->expand(true);SetWindowDisplayAffinity(app.dock->hwnd,WDA_NONE);ShowWindow(app.dock->hwnd,SW_SHOWNOACTIVATE);UpdateWindow(app.dock->hwnd);DwmFlush();RECT ui;GetWindowRect(app.dock->hwnd,&ui);ui.left+=6;ui.top+=6;ui.right-=6;ui.bottom-=6;auto shot=captureDesktop(ui);check(shot && savePng(*shot,directory/(L"dock-preview_"+stamp()+L".png")),"native dock visual preview PNG");ShowWindow(app.dock->hwnd,SW_HIDE);
        PROCESS_MEMORY_COUNTERS memory{};if(GetProcessMemoryInfo(GetCurrentProcess(),&memory,sizeof(memory)))report<<"Working set with synthetic history and pins: "<<memory.WorkingSetSize/(1024*1024)<<" MiB\n";
        archived=app.records.size();app.saveConfig();app.shutdown();check(fs::exists(a->path) && fs::exists(b->path),"shutdown waits for save and preserves history");
    }
    {App reopened(instance);check(reopened.init(directory/L"app-data",true),"restart initialization");check(reopened.records.size()>=archived,"PNG history survives restart");check(reopened.edge==Edge::Right,"dock preference survives restart");if(!reopened.records.empty()){auto record=reopened.records.front();reopened.requestThumbnail(record);auto until=GetTickCount64()+6000;while(record->previewLoading && GetTickCount64()<until){pump();MsgWaitForMultipleObjects(0,nullptr,FALSE,20,QS_ALLINPUT);}check(record->thumb && !record->previewFailed && !record->previewLoading,"history thumbnail asynchronously decodes without blocking UI");}}
    report<<"\n"<<count<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
}
