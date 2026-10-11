#include "app.h"
#include <windowsx.h>
namespace sd {
Dock::~Dock() {if(hwnd)DestroyWindow(hwnd);}
LRESULT CALLBACK Dock::proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    auto* self=reinterpret_cast<Dock*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
    if(msg==WM_NCCREATE){self=static_cast<Dock*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);self->hwnd=hwnd;SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
    if(!self)return DefWindowProcW(hwnd,msg,wp,lp);
    return self->message(msg,wp,lp);
}
bool Dock::create() {
    WNDCLASSEXW c{};c.cbSize=sizeof(c);c.lpfnWndProc=proc;c.hInstance=app_.instance;c.lpszClassName=L"ScreenDock.Edge";c.hCursor=LoadCursorW(nullptr,IDC_ARROW);c.hIcon=app_.icon();RegisterClassExW(&c);
    hwnd=CreateWindowExW(WS_EX_TOPMOST|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,c.lpszClassName,L"ScreenDock · 截图便签",WS_POPUP,0,0,12,112,nullptr,nullptr,app_.instance,this);
    if(!hwnd)return false;prepareWindow(hwnd);reposition();return true;
}
void Dock::reposition() {
    if(app_.displays.empty())return;
    auto work=app_.displays[app_.monitor].work;
    // Move to the destination monitor first so the following DPI query is accurate.
    SetWindowPos(hwnd,HWND_TOPMOST,work.left+(work.right-work.left)/2,work.top+(work.bottom-work.top)/2,0,0,SWP_NOSIZE|SWP_NOACTIVATE);
    auto r=dockRect(work,app_.edge,expanded,int(GetDpiForWindow(hwnd)));
    SetWindowPos(hwnd,HWND_TOPMOST,r.left,r.top,r.right-r.left,r.bottom-r.top,SWP_NOACTIVATE);graphics_.reset();InvalidateRect(hwnd,nullptr,FALSE);
}
void Dock::expand(bool value) {
    if(expanded==value)return;expanded=value;leaveAt=0;mouse_={-1,-1};
    if(value)SetTimer(hwnd,HoverTimer,120,nullptr);else KillTimer(hwnd,HoverTimer);
    reposition();
}
void Dock::refresh(bool newest) {if(newest)first_=0;graphics_.bitmaps.clear();InvalidateRect(hwnd,nullptr,FALSE);}
bool Dock::recordButtons(const fs::path& path,RECT& copy,RECT& remove) const {
    auto rectangle=[](D2D1_RECT_F r){return RECT{LONG(r.left),LONG(r.top),LONG(r.right),LONG(r.bottom)};};
    for(const auto& hit:cards_)if(hit.index<app_.records.size() && app_.records[hit.index]->path==path){copy=rectangle(hit.copyFile);remove=rectangle(hit.remove);return true;}return false;
}
void Dock::paint() {
    PAINTSTRUCT ps;BeginPaint(hwnd,&ps);
    if(!graphics_.begin(hwnd)){EndPaint(hwnd,&ps);return;}
    RECT cr{};GetClientRect(hwnd,&cr);float w=float(cr.right),h=float(cr.bottom),s=scaleFor(hwnd);
    const bool horizontal=app_.edge==Edge::Top || app_.edge==Edge::Bottom;
    if(!expanded) {
        graphics_.fill({0,0,w,h},0x203A46,1,5*s);
        if(horizontal){graphics_.fill({w/2-23*s,h/2-1.5f*s,w/2+23*s,h/2+1.5f*s},Accent,1,1.5f*s);}
        else graphics_.fill({w/2-1.5f*s,h/2-23*s,w/2+1.5f*s,h/2+23*s},Accent,1,1.5f*s);
        graphics_.end();EndPaint(hwnd,&ps);return;
    }
    graphics_.text(L"SCREEN DOCK " SCREENDOCK_VERSION,{18*s,12*s,w-54*s,42*s},14*s,Ink,true);
    menu_={w-45*s,12*s,w-15*s,42*s};graphics_.button(menu_,L"···",inside(menu_,mouse_));
    graphics_.text(L"让对比留在眼前",{18*s,41*s,w-18*s,62*s},11*s,Muted);
    shot_={18*s,74*s,horizontal?186*s:w-100*s,112*s};paste_={shot_.right+8*s,74*s,horizontal?274*s:w-18*s,112*s};
    graphics_.button(shot_,L"＋  截图",inside(shot_,mouse_),true);graphics_.button(paste_,L"粘贴",inside(paste_,mouse_));
    if(horizontal)graphics_.text(L"Ctrl + Alt + A",{294*s,74*s,w-18*s,112*s},12*s,Muted,false,DWRITE_TEXT_ALIGNMENT_TRAILING);
    const float top=horizontal?122*s:134*s,bottom=h-66*s;
    graphics_.line(18*s,top-6*s,w-18*s,top-6*s,0x2D3A4D);
    cards_.clear();
    if(first_>=app_.records.size())first_=app_.records.empty()?0:app_.records.size()-1;
    const float cardH=horizontal?bottom-top:float(thumbSize_+74)*s;
    const float cardW=horizontal?float(thumbSize_+60)*s:w-36*s;
    const size_t capacity=std::max<size_t>(1,size_t(horizontal?(w-26*s)/(cardW+10*s):(bottom-top+10*s)/(cardH+10*s)));
    if(app_.records.empty()) {
        graphics_.text(L"还没有截图",{18*s,top+24*s,w-18*s,top+56*s},17*s,Ink,true,DWRITE_TEXT_ALIGNMENT_CENTER);
        graphics_.text(L"拖动框选 → 自动保存 → 点击置顶",{18*s,top+62*s,w-18*s,top+92*s},12*s,Muted,false,DWRITE_TEXT_ALIGNMENT_CENTER);
        if(!horizontal){graphics_.text(L"便签可拖动，滚轮缩放",{18*s,top+98*s,w-18*s,top+126*s},12*s,Muted,false,DWRITE_TEXT_ALIGNMENT_CENTER);graphics_.text(L"右键边栏切换停靠边缘",{18*s,top+132*s,w-18*s,top+160*s},12*s,Muted,false,DWRITE_TEXT_ALIGNMENT_CENTER);}
    }
    for(size_t k=0;k<capacity && first_+k<app_.records.size();++k) {
        const size_t index=first_+k;auto& record=app_.records[index];
        float x=18*s+(horizontal?k*(cardW+10*s):0),y=top+(horizontal?0:k*(cardH+10*s));
        D2D1_RECT_F card{x,y,x+cardW,std::min(y+cardH,bottom)};
        float middle=(card.left+card.right)/2;
        D2D1_RECT_F fileButton{card.left+8*s,card.bottom-30*s,middle-4*s,card.bottom-6*s},removeButton{middle+4*s,card.bottom-30*s,card.right-8*s,card.bottom-6*s};
        cards_.push_back({index,card,fileButton,removeButton});
        graphics_.fill(card,inside(card,mouse_)?0x2C3D53:Card,1,9*s);
        if(!record->thumb && !record->failed)app_.requestThumbnail(record);
        const float labelH=64*s;D2D1_RECT_F viewport{card.left+7*s,card.top+7*s,card.right-7*s,card.bottom-labelH};
        graphics_.fill(viewport,0x0E1622,1,5*s);
        if(record->thumb) {
            float factor=std::min((viewport.right-viewport.left)/record->thumb->w,(viewport.bottom-viewport.top)/record->thumb->h);
            float iw=record->thumb->w*factor,ih=record->thumb->h*factor,cx=(viewport.left+viewport.right)/2,cy=(viewport.top+viewport.bottom)/2;
            graphics_.image(record->thumb,{cx-iw/2,cy-ih/2,cx+iw/2,cy+ih/2});
        } else graphics_.text(record->previewLoading?L"正在读取…":L"预览不可用",viewport,12*s,Muted,false,DWRITE_TEXT_ALIGNMENT_CENTER);
        bool pinned=false;for(auto& p:app_.pins)if(p->path()==record->path)pinned=true;
        std::wstring status=record->failed?L"保存失败":record->saving?L"保存中":pinned?L"已置顶":L"点击置顶";
        graphics_.text(record->label,{card.left+10*s,card.bottom-58*s,card.right-64*s,card.bottom-34*s},11*s,Muted);
        graphics_.text(status,{card.right-74*s,card.bottom-58*s,card.right-8*s,card.bottom-34*s},10*s,pinned?Accent:Muted,false,DWRITE_TEXT_ALIGNMENT_TRAILING);
        const bool copied=copiedPath_==record->path && GetTickCount64()<copiedUntil_;
        graphics_.fill(fileButton,copied?Accent:(inside(fileButton,mouse_)?0x3B506A:0x2C3B50),1,5*s);
        graphics_.text(copied?L"已复制":record->saving?L"保存中…":L"复制文件",fileButton,11*s,copied?Background:Ink,true,DWRITE_TEXT_ALIGNMENT_CENTER);
        graphics_.fill(removeButton,inside(removeButton,mouse_)?0x3B506A:0x2C3B50,1,5*s);graphics_.text(L"删除记录",removeButton,11*s,Ink,true,DWRITE_TEXT_ALIGNMENT_CENTER);
    }
    // Keep at most the nearby preview pages; pinned windows own their original pixels independently.
    for(size_t n=0;n<app_.records.size();++n) if(!app_.records[n]->pending && (n+12<first_ || n>first_+capacity+12)) {graphics_.bitmaps.erase(app_.records[n]->thumb.get());app_.records[n]->thumb.reset();}
    if(graphics_.bitmaps.size()>48)graphics_.bitmaps.clear();
    previous_={18*s,h-57*s,48*s,h-30*s};next_={54*s,h-57*s,84*s,h-30*s};folder_={w-106*s,h-57*s,w-18*s,h-30*s};
    graphics_.button(previous_,L"‹",inside(previous_,mouse_));graphics_.button(next_,L"›",inside(next_,mouse_));graphics_.button(folder_,L"历史文件",inside(folder_,mouse_));
    auto total=app_.records.size();std::wstring count=total?std::to_wstring(first_+1)+L" / "+std::to_wstring(total):L"0 张";
    graphics_.text(count,{92*s,h-57*s,w-112*s,h-30*s},10*s,Muted);
    graphics_.text(L"滚轮调预览大小 · Shift 滚轮翻页",{18*s,h-27*s,w-18*s,h-7*s},10*s,Muted,false,DWRITE_TEXT_ALIGNMENT_CENTER);
    graphics_.end();EndPaint(hwnd,&ps);
}
void Dock::showMenu(POINT p) {
    menuOpen=true;HMENU m=CreatePopupMenu(),edges=CreatePopupMenu(),screens=CreatePopupMenu();
    AppendMenuW(m,MF_STRING,1,L"区域截图\tCtrl+Alt+A");AppendMenuW(m,MF_STRING,2,L"粘贴剪贴板图片");AppendMenuW(m,MF_STRING,3,L"打开截图历史");AppendMenuW(m,MF_SEPARATOR,0,nullptr);
    const wchar_t* names[]={L"左侧",L"右侧",L"顶部",L"底部"};for(int i=0;i<4;++i)AppendMenuW(edges,MF_STRING|(int(app_.edge)==i?MF_CHECKED:0),10+i,names[i]);AppendMenuW(m,MF_POPUP,reinterpret_cast<UINT_PTR>(edges),L"停靠位置");
    for(size_t i=0;i<app_.displays.size();++i){auto label=L"显示器 "+std::to_wstring(i+1)+L"  "+app_.displays[i].name;AppendMenuW(screens,MF_STRING|(app_.monitor==i?MF_CHECKED:0),100+i,label.c_str());}AppendMenuW(m,MF_POPUP,reinterpret_cast<UINT_PTR>(screens),L"停靠显示器");
    AppendMenuW(m,MF_STRING|(app_.autoPin?MF_CHECKED:0),30,L"新截图自动置顶");AppendMenuW(m,MF_STRING,31,L"关闭全部便签（保留图片文件）");AppendMenuW(m,MF_STRING,33,L"恢复已移出的记录");AppendMenuW(m,MF_STRING,32,L"收起边栏");AppendMenuW(m,MF_SEPARATOR,0,nullptr);AppendMenuW(m,MF_STRING,99,L"退出 ScreenDock");
    SetForegroundWindow(hwnd);UINT command=TrackPopupMenu(m,TPM_RETURNCMD|TPM_RIGHTBUTTON,p.x,p.y,0,hwnd,nullptr);DestroyMenu(m);menuOpen=false;PostMessageW(hwnd,WM_NULL,0,0);
    if(command==1)app_.beginCapture();else if(command==2)app_.paste();else if(command==3)app_.openFolder();else if(command>=10 && command<=13)app_.setEdge(Edge(command-10));else if(command>=100 && command<100+app_.displays.size())app_.setMonitor(command-100);else if(command==30){app_.autoPin=!app_.autoPin;app_.saveConfig();}else if(command==31){app_.pins.clear();refresh();}else if(command==33)app_.restoreHistory();else if(command==32)expand(false);else if(command==99)PostQuitMessage(0);
}
LRESULT Dock::message(UINT msg,WPARAM wp,LPARAM lp) {
    bool handled=false;LRESULT result=app_.controller(msg,wp,lp,handled);if(handled)return result;
    switch(msg) {
    case WM_PAINT:paint();return 0;
    case WM_ERASEBKGND:return 1;
    case WM_MOUSEACTIVATE:return MA_NOACTIVATE;
    case WM_MOUSEMOVE:
        mouse_={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};
        if(!expanded && !app_.capturing){expand(true);return 0;}
        if(!tracking_){TRACKMOUSEEVENT event{sizeof(event),TME_LEAVE,hwnd,0};TrackMouseEvent(&event);tracking_=true;}
        leaveAt=0;InvalidateRect(hwnd,nullptr,FALSE);return 0;
    case WM_MOUSELEAVE:tracking_=false;mouse_={-1,-1};leaveAt=GetTickCount64();InvalidateRect(hwnd,nullptr,FALSE);return 0;
    case WM_TIMER:
        if(copiedUntil_ && GetTickCount64()>=copiedUntil_){copiedUntil_=0;InvalidateRect(hwnd,nullptr,FALSE);}
        if(wp==HoverTimer && expanded && !menuOpen && !app_.capturing) {POINT p;GetCursorPos(&p);RECT r;GetWindowRect(hwnd,&r);if(PtInRect(&r,p))leaveAt=0;else if(!leaveAt)leaveAt=GetTickCount64();else if(GetTickCount64()-leaveAt>650)expand(false);}return 0;
    case WM_LBUTTONUP:{POINT p{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};if(!expanded){expand(true);return 0;}if(inside(shot_,p))app_.beginCapture();else if(inside(paste_,p))app_.paste();else if(inside(folder_,p))app_.openFolder();else if(inside(menu_,p)){GetCursorPos(&p);showMenu(p);}else if(inside(previous_,p)){if(first_>0)first_=first_>cards_.size()?first_-cards_.size():0;refresh();}else if(inside(next_,p)){if(first_+cards_.size()<app_.records.size())first_+=cards_.size();refresh();}else for(const auto& hit:cards_)if(inside(hit.body,p) && hit.index<app_.records.size()){
        auto record=app_.records[hit.index];
        if(inside(hit.remove,p))app_.hideRecord(record);
        else if(inside(hit.copyFile,p)){if(app_.copyRecordFile(record)){copiedPath_=record->path;copiedUntil_=GetTickCount64()+1500;refresh();}}
        else {app_.pin(record);refresh();}break;
    }return 0;}
    case WM_MOUSEWHEEL:
        if(wp&MK_SHIFT) {int delta=GET_WHEEL_DELTA_WPARAM(wp);if(delta<0 && first_+1<app_.records.size())++first_;else if(delta>0 && first_>0)--first_;}
        else thumbSize_=std::clamp(thumbSize_+(GET_WHEEL_DELTA_WPARAM(wp)>0?20:-20),80,200);
        refresh();return 0;
    case WM_CONTEXTMENU:{POINT p{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};if(p.x==-1)GetCursorPos(&p);showMenu(p);return 0;}
    case WM_DPICHANGED:graphics_.reset();InvalidateRect(hwnd,nullptr,FALSE);return 0;
    case WM_SETTINGCHANGE:app_.updateMonitors();reposition();return 0;
    case WM_SIZE:InvalidateRect(hwnd,nullptr,FALSE);return 0;
    case WM_NCDESTROY:{HWND old=hwnd;hwnd=nullptr;return DefWindowProcW(old,msg,wp,lp);}
    default:return DefWindowProcW(hwnd,msg,wp,lp);
    }
}
}
