#include "app.h"
#include <windowsx.h>
namespace sd {
Pin::~Pin() {if(hwnd)DestroyWindow(hwnd);}
LRESULT CALLBACK Pin::proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp) {
    auto* self=reinterpret_cast<Pin*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));if(msg==WM_NCCREATE){self=static_cast<Pin*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);self->hwnd=hwnd;SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
    return self?self->message(msg,wp,lp):DefWindowProcW(hwnd,msg,wp,lp);
}
bool Pin::create(int index) {
    WNDCLASSEXW c{};c.cbSize=sizeof(c);c.style=CS_DBLCLKS;c.lpfnWndProc=proc;c.hInstance=app_.instance;c.lpszClassName=L"ScreenDock.Pin";c.hCursor=LoadCursorW(nullptr,IDC_SIZEALL);c.hIcon=app_.icon();RegisterClassExW(&c);
    RECT work=app_.displays[app_.monitor].work;float s=GetDpiForWindow(app_.dock->hwnd)/96.f;
    zoom_=std::min({1.f,480*s/image_->w,360*s/image_->h});zoom_=std::max(0.08f,zoom_);
    int w=int(std::round(image_->w*zoom_))+4,h=int(std::round(image_->h*zoom_))+int(36*s)+4;
    w=std::min(w,int(work.right-work.left));h=std::min(h,int(work.bottom-work.top));
    int x=work.right-w-int(24*s),y=work.top+int(60*s)+(index%7)*int(34*s);
    if(app_.edge==Edge::Left)x=work.left+int(24*s);
    if(app_.edge==Edge::Bottom)y=work.bottom-h-int(24*s)-(index%7)*int(34*s);
    x=std::clamp(x,int(work.left),int(work.right)-w);y=std::clamp(y,int(work.top),int(work.bottom)-h);
    hwnd=CreateWindowExW(WS_EX_TOPMOST|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,c.lpszClassName,L"ScreenDock · 置顶图片",WS_POPUP,x,y,w,h,nullptr,nullptr,app_.instance,this);
    if(!hwnd)return false;prepareWindow(hwnd);record_->width=image_->w;record_->height=image_->h;if(!app_.testMode)ShowWindow(hwnd,SW_SHOWNOACTIVATE);return true;
}
void Pin::show(bool visible) {ShowWindow(hwnd,visible?SW_SHOWNOACTIVATE:SW_HIDE);}
void Pin::clampPan() {RECT r;GetClientRect(hwnd,&r);float title=36*scaleFor(hwnd);panX_=std::clamp(panX_,std::min(0.f,r.right-4-image_->w*zoom_),0.f);panY_=std::clamp(panY_,std::min(0.f,r.bottom-title-4-image_->h*zoom_),0.f);}
void Pin::resize(float zoom,POINT anchor) {
    zoom=std::clamp(zoom,0.08f,8.f);RECT old;GetWindowRect(hwnd,&old);MONITORINFO info{sizeof(info)};GetMonitorInfoW(MonitorFromPoint(anchor,MONITOR_DEFAULTTONEAREST),&info);auto work=info.rcWork;
    float s=scaleFor(hwnd);float title=36*s;
    float pixelX=(anchor.x-old.left-2-panX_)/zoom_,pixelY=(anchor.y-old.top-title-2-panY_)/zoom_;
    float rx=(anchor.x-old.left-2)/std::max(1.f,float(old.right-old.left-4)),ry=(anchor.y-old.top-title-2)/std::max(1.f,float(old.bottom-old.top-int(title)-4));rx=std::clamp(rx,0.f,1.f);ry=std::clamp(ry,0.f,1.f);
    int w=std::max(8,int(std::round(image_->w*zoom))+4),h=std::max(int(title)+8,int(std::round(image_->h*zoom))+int(title)+4);
    w=std::min(w,int(work.right-work.left));h=std::min(h,int(work.bottom-work.top));
    int x=int(anchor.x-2-rx*(w-4)),y=int(anchor.y-title-2-ry*(h-title-4));
    x=std::clamp(x,int(work.left),int(work.right)-w);y=std::clamp(y,int(work.top),int(work.bottom)-h);zoom_=zoom;
    SetWindowPos(hwnd,HWND_TOPMOST,x,y,w,h,SWP_NOACTIVATE);panX_=anchor.x-x-2-pixelX*zoom_;panY_=anchor.y-y-title-2-pixelY*zoom_;clampPan();InvalidateRect(hwnd,nullptr,FALSE);
}
void Pin::paint() {
    PAINTSTRUCT ps;BeginPaint(hwnd,&ps);if(!graphics_.begin(hwnd)){EndPaint(hwnd,&ps);return;}RECT cr;GetClientRect(hwnd,&cr);float w=float(cr.right),h=float(cr.bottom),s=scaleFor(hwnd),title=36*s;
    graphics_.fill({0,0,w,title},hovering_?0x26384D:Card);
    std::wstring label=std::to_wstring(int(std::round(zoom_*100)))+L"% · "+std::to_wstring(image_->w)+L"×"+std::to_wstring(image_->h);
    graphics_.text(label,{10*s,0,w-111*s,title},11*s,hovering_?Ink:Muted);
    actual_={w-108*s,4*s,w-73*s,title-4*s};copy_={w-70*s,4*s,w-35*s,title-4*s};close_={w-32*s,4*s,w-4*s,title-4*s};
    if(w>180*s) {graphics_.button(actual_,L"1:1",inside(actual_,mouse_));graphics_.button(copy_,L"复制",inside(copy_,mouse_));}
    if(w>44*s)graphics_.button(close_,L"×",inside(close_,mouse_));
    graphics_.fill({2,title+2,w-2,h-2},0xFAFAFA);
    graphics_.rt->PushAxisAlignedClip({2,title+2,w-2,h-2},D2D1_ANTIALIAS_MODE_ALIASED);
    graphics_.image(image_,{2+panX_,title+2+panY_,2+panX_+image_->w*zoom_,title+2+panY_+image_->h*zoom_},1,nearest_ || std::abs(zoom_-1)<0.001f);
    graphics_.rt->PopAxisAlignedClip();
    graphics_.end();EndPaint(hwnd,&ps);
}
void Pin::menu(POINT p) {
    HMENU m=CreatePopupMenu();AppendMenuW(m,MF_STRING,1,L"复制原图");AppendMenuW(m,MF_STRING,2,L"恢复 100%");AppendMenuW(m,MF_STRING,3,L"适合屏幕");AppendMenuW(m,MF_STRING|(nearest_?MF_CHECKED:0),4,L"像素清晰模式（最近邻缩放）");AppendMenuW(m,MF_STRING,5,L"定位历史文件");AppendMenuW(m,MF_STRING,6,L"恢复不透明");AppendMenuW(m,MF_STRING|MF_DISABLED,0,L"大图查看：Ctrl + 拖动 / 中键拖动");AppendMenuW(m,MF_SEPARATOR,0,nullptr);AppendMenuW(m,MF_STRING,7,L"关闭便签（保留原图）");
    SetForegroundWindow(hwnd);UINT c=TrackPopupMenu(m,TPM_RETURNCMD|TPM_RIGHTBUTTON,p.x,p.y,0,hwnd,nullptr);DestroyMenu(m);PostMessageW(hwnd,WM_NULL,0,0);
    if(c==1){if(!copyPicture(hwnd,*image_))app_.notify(L"复制失败，剪贴板可能被其他程序占用。",true);}else if(c==2)resize(1,p);else if(c==3){MONITORINFO info{sizeof(info)};GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&info);float fit=std::min(float(info.rcWork.right-info.rcWork.left-4)/image_->w,(info.rcWork.bottom-info.rcWork.top-36*scaleFor(hwnd)-4)/image_->h);resize(fit,p);}else if(c==4){nearest_=!nearest_;InvalidateRect(hwnd,nullptr,FALSE);}else if(c==5)app_.openRecord(record_->path);else if(c==6){opacity_=255;SetLayeredWindowAttributes(hwnd,0,255,LWA_ALPHA);}else if(c==7)PostMessageW(hwnd,WM_CLOSE,0,0);
}
LRESULT Pin::message(UINT msg,WPARAM wp,LPARAM lp) {
    switch(msg) {
    case WM_PAINT:paint();return 0;
    case WM_ERASEBKGND:return 1;
    case WM_MOUSEACTIVATE:return MA_NOACTIVATE;
    case WM_MOUSEMOVE:
        mouse_={GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};
        if(!hovering_){hovering_=true;TRACKMOUSEEVENT event{sizeof(event),TME_LEAVE,hwnd,0};TrackMouseEvent(&event);}
        if(dragging_){POINT p;GetCursorPos(&p);if(panning_){panX_=dragPanX_+p.x-dragOrigin_.x;panY_=dragPanY_+p.y-dragOrigin_.y;clampPan();}else SetWindowPos(hwnd,HWND_TOPMOST,dragWindow_.left+p.x-dragOrigin_.x,dragWindow_.top+p.y-dragOrigin_.y,0,0,SWP_NOSIZE|SWP_NOACTIVATE);}
        InvalidateRect(hwnd,nullptr,FALSE);return 0;
    case WM_MOUSELEAVE:hovering_=false;mouse_={-1,-1};InvalidateRect(hwnd,nullptr,FALSE);return 0;
    case WM_MBUTTONDOWN:
    case WM_LBUTTONDOWN:{POINT p{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};RECT r;GetClientRect(hwnd,&r);float s=scaleFor(hwnd);
        if(r.right>44*s && inside(close_,p)){PostMessageW(hwnd,WM_CLOSE,0,0);return 0;}
        if(r.right>180*s && inside(copy_,p)){if(!copyPicture(hwnd,*image_))app_.notify(L"复制失败，请稍后重试。",true);return 0;}
        if(r.right>180*s && inside(actual_,p)){GetCursorPos(&p);resize(1,p);return 0;}
        dragging_=true;panning_=msg==WM_MBUTTONDOWN || (wp&MK_CONTROL);dragPanX_=panX_;dragPanY_=panY_;GetCursorPos(&dragOrigin_);GetWindowRect(hwnd,&dragWindow_);SetCapture(hwnd);return 0;}
    case WM_MBUTTONUP:
    case WM_LBUTTONUP:
        if(dragging_){dragging_=false;ReleaseCapture();if(!panning_){RECT r;GetWindowRect(hwnd,&r);MONITORINFO info{sizeof(info)};GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&info);auto m=info.rcWork;int w=r.right-r.left,h=r.bottom-r.top,threshold=int(16*scaleFor(hwnd));int x=std::clamp(int(r.left),int(m.left),std::max(int(m.left),int(m.right)-w)),y=std::clamp(int(r.top),int(m.top),std::max(int(m.top),int(m.bottom)-h));if(x-m.left<threshold)x=m.left;else if(m.right-x-w<threshold)x=m.right-w;if(y-m.top<threshold)y=m.top;else if(m.bottom-y-h<threshold)y=m.bottom-h;SetWindowPos(hwnd,HWND_TOPMOST,x,y,0,0,SWP_NOSIZE|SWP_NOACTIVATE);}}return 0;
    case WM_CAPTURECHANGED:dragging_=false;return 0;
    case WM_LBUTTONDBLCLK:{POINT p;GetCursorPos(&p);resize(1,p);return 0;}
    case WM_MOUSEWHEEL:{POINT p{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};int delta=GET_WHEEL_DELTA_WPARAM(wp);if(wp&MK_SHIFT){opacity_=std::clamp(opacity_+(delta>0?16:-16),64,255);SetWindowLongPtrW(hwnd,GWL_EXSTYLE,GetWindowLongPtrW(hwnd,GWL_EXSTYLE)|WS_EX_LAYERED);SetLayeredWindowAttributes(hwnd,0,BYTE(opacity_),LWA_ALPHA);}else resize(zoomValue(zoom_,delta),p);return 0;}
    case WM_CONTEXTMENU:{POINT p{GET_X_LPARAM(lp),GET_Y_LPARAM(lp)};if(p.x==-1)GetCursorPos(&p);menu(p);return 0;}
    case WM_DPICHANGED:{auto r=reinterpret_cast<RECT*>(lp);SetWindowPos(hwnd,HWND_TOPMOST,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOACTIVATE);graphics_.reset();POINT p{r->left,r->top};resize(zoom_,p);return 0;}
    case WM_SIZE:InvalidateRect(hwnd,nullptr,FALSE);return 0;
    case WM_CLOSE:ShowWindow(hwnd,SW_HIDE);PostMessageW(app_.dock->hwnd,RemovePin,0,reinterpret_cast<LPARAM>(this));return 0;
    case WM_NCDESTROY:{HWND old=hwnd;hwnd=nullptr;return DefWindowProcW(old,msg,wp,lp);}
    default:return DefWindowProcW(hwnd,msg,wp,lp);
    }
}
}
