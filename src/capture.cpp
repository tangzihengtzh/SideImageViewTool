#include "app.h"
#include <windowsx.h>
namespace sd {
Capture::~Capture(){if(hwnd)DestroyWindow(hwnd);}
LRESULT CALLBACK Capture::proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {auto* self=reinterpret_cast<Capture*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));if(msg==WM_NCCREATE){self=static_cast<Capture*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);self->hwnd=hwnd;SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}return self?self->message(msg,wp,lp):DefWindowProcW(hwnd,msg,wp,lp);}
bool Capture::create() {
    WNDCLASSEXW c{};c.cbSize=sizeof(c);c.lpfnWndProc=proc;c.hInstance=app_.instance;c.lpszClassName=L"ScreenDock.Capture";c.hCursor=LoadCursorW(nullptr,IDC_CROSS);RegisterClassExW(&c);
    hwnd=CreateWindowExW(WS_EX_TOPMOST|WS_EX_TOOLWINDOW,c.lpszClassName,L"ScreenDock · 框选截图",WS_POPUP,virtual_.left,virtual_.top,virtual_.right-virtual_.left,virtual_.bottom-virtual_.top,nullptr,nullptr,app_.instance,this);
    if(!hwnd)return false;
    // This frozen virtual-desktop surface uses physical pixels across mixed-DPI monitors.
    SetWindowDisplayAffinity(hwnd,0x11);if(!app_.testMode){ShowWindow(hwnd,SW_SHOW);SetForegroundWindow(hwnd);SetFocus(hwnd);}return true;
}
void Capture::paint() {
    PAINTSTRUCT ps;BeginPaint(hwnd,&ps);if(!graphics_.begin(hwnd)){EndPaint(hwnd,&ps);return;}
    float w=float(desktop_->w),h=float(desktop_->h);graphics_.image(desktop_,{0,0,w,h});graphics_.fill({0,0,w,h},0x07101D,0.52f);
    RECT area=intersect(normalized(start_,end_),{0,0,LONG(desktop_->w),LONG(desktop_->h)});
    if(selecting_ && area.right>area.left && area.bottom>area.top) {
        D2D1_RECT_F r{float(area.left),float(area.top),float(area.right),float(area.bottom)};graphics_.image(desktop_,r,1,true,&r);
        graphics_.line(r.left,r.top,r.right,r.top,Accent,2);graphics_.line(r.right,r.top,r.right,r.bottom,Accent,2);graphics_.line(r.right,r.bottom,r.left,r.bottom,Accent,2);graphics_.line(r.left,r.bottom,r.left,r.top,Accent,2);
        std::wstring label=std::to_wstring(area.right-area.left)+L" × "+std::to_wstring(area.bottom-area.top)+L" px";
        float x=std::clamp(r.left,0.f,std::max(0.f,w-190)),y=r.top>=40?r.top-36:std::min(h-36,r.bottom+8);
        graphics_.fill({x,y,x+180,y+28},Background,0.96f,5);graphics_.text(label,{x+10,y,x+170,y+28},13,Accent,true);
    } else {
        POINT cursor;GetCursorPos(&cursor);MONITORINFO info{sizeof(info)};GetMonitorInfoW(MonitorFromPoint(cursor,MONITOR_DEFAULTTONEAREST),&info);
        float x=float(info.rcWork.left-virtual_.left)+24,y=float(info.rcWork.top-virtual_.top)+24;
        graphics_.fill({x,y,x+390,y+70},Background,0.96f,10);graphics_.text(L"拖动框选截图",{x+18,y+8,x+370,y+34},18,Ink,true);graphics_.text(L"松开鼠标保存 PNG · Esc / 右键取消",{x+18,y+37,x+370,y+61},13,Muted);
    }
    graphics_.end();EndPaint(hwnd,&ps);
}
void Capture::finish(bool accepted) {
    if(finished_)return;finished_=true;ReleaseCapture();Picture selected;
    if(accepted)selected=crop(*desktop_,normalized(start_,end_));
    ShowWindow(hwnd,SW_HIDE);auto* result=new Picture(selected);if(!PostMessageW(app_.dock->hwnd,CaptureDone,0,reinterpret_cast<LPARAM>(result)))delete result;
}
LRESULT Capture::message(UINT msg,WPARAM wp,LPARAM lp) {
    switch(msg) {
    case WM_PAINT:paint();return 0;
    case WM_ERASEBKGND:return 1;
    case WM_LBUTTONDOWN:selecting_=true;start_=end_={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};SetCapture(hwnd);InvalidateRect(hwnd,nullptr,FALSE);return 0;
    case WM_MOUSEMOVE:if(selecting_){end_={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};InvalidateRect(hwnd,nullptr,FALSE);}return 0;
    case WM_LBUTTONUP:if(selecting_){end_={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};RECT r=normalized(start_,end_);finish(r.right-r.left>=2 && r.bottom-r.top>=2);}return 0;
    case WM_RBUTTONDOWN:finish(false);return 0;
    case WM_KEYDOWN:if(wp==VK_ESCAPE)finish(false);return 0;
    case WM_CLOSE:finish(false);return 0;
    case WM_DPICHANGED:return 0;
    case WM_NCDESTROY:{HWND old=hwnd;hwnd=nullptr;return DefWindowProcW(old,msg,wp,lp);}
    default:return DefWindowProcW(hwnd,msg,wp,lp);
    }
}
}
