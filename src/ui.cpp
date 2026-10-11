#include "ui.h"
#include <dwmapi.h>
namespace sd {
static Com<ID2D1Factory> factory;
static Com<IDWriteFactory> textFactory;
static std::unordered_map<uint64_t,Com<IDWriteTextFormat>> formats;
bool Graphics::begin(HWND window) {
    hwnd=window;
    if(!factory && FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,factory.GetAddressOf()))) return false;
    if(!textFactory && FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(textFactory.GetAddressOf()))))return false;
    RECT r{};GetClientRect(hwnd,&r);
    if(!rt) {
        auto props=D2D1::RenderTargetProperties();props.dpiX=props.dpiY=96;
        if(FAILED(factory->CreateHwndRenderTarget(props,D2D1::HwndRenderTargetProperties(hwnd,D2D1::SizeU(r.right,r.bottom)),&rt)))return false;
        if(FAILED(rt->CreateSolidColorBrush(color(Ink),&brush))) {reset();return false;}
    } else {auto size=rt->GetPixelSize();if((size.width!=UINT(r.right) || size.height!=UINT(r.bottom)) && FAILED(rt->Resize(D2D1::SizeU(r.right,r.bottom)))){reset();return false;}}
    rt->BeginDraw();rt->SetTransform(D2D1::Matrix3x2F::Identity());rt->Clear(color(Background));return true;
}
HRESULT Graphics::end() {HRESULT result=rt?rt->EndDraw():E_FAIL;if(result==D2DERR_RECREATE_TARGET)reset();return result;}
void Graphics::reset() {bitmaps.clear();brush.Reset();rt.Reset();}
void Graphics::fill(D2D1_RECT_F r,unsigned rgb,float alpha,float radius) {brush->SetColor(color(rgb,alpha));if(radius)rt->FillRoundedRectangle(D2D1::RoundedRect(r,radius,radius),brush.Get());else rt->FillRectangle(r,brush.Get());}
void Graphics::line(float x1,float y1,float x2,float y2,unsigned rgb,float thickness) {brush->SetColor(color(rgb));rt->DrawLine(D2D1::Point2F(x1,y1),D2D1::Point2F(x2,y2),brush.Get(),thickness);}
void Graphics::text(const std::wstring& value,D2D1_RECT_F r,float size,unsigned rgb,bool bold,DWRITE_TEXT_ALIGNMENT align) {
    uint64_t key=(uint64_t(std::round(size*100))<<8)|(uint64_t(bold)<<4)|uint64_t(align);
    auto& f=formats[key];if(!f) {
        if(FAILED(textFactory->CreateTextFormat(L"Microsoft YaHei UI",nullptr,bold?DWRITE_FONT_WEIGHT_SEMI_BOLD:DWRITE_FONT_WEIGHT_NORMAL,DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,size,L"zh-cn",&f)))return;
        f->SetTextAlignment(align);f->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);f->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        DWRITE_TRIMMING trim{DWRITE_TRIMMING_GRANULARITY_CHARACTER,0,0};Com<IDWriteInlineObject> ellipsis;textFactory->CreateEllipsisTrimmingSign(f.Get(),&ellipsis);f->SetTrimming(&trim,ellipsis.Get());
    }
    brush->SetColor(color(rgb));rt->DrawText(value.c_str(),UINT32(value.size()),f.Get(),r,brush.Get(),D2D1_DRAW_TEXT_OPTIONS_CLIP);
}
void Graphics::image(const Picture& pixels,D2D1_RECT_F dest,float opacity,bool nearest,const D2D1_RECT_F* source) {
    if(!pixels || !pixels->valid())return;
    auto& bmp=bitmaps[pixels.get()];if(!bmp) {
        auto props=D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED),96,96);
        if(FAILED(rt->CreateBitmap(D2D1::SizeU(pixels->w,pixels->h),pixels->bgra.data(),pixels->w*4,props,&bmp)))return;
    }
    rt->DrawBitmap(bmp.Get(),dest,opacity,nearest?D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR:D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,source);
}
void Graphics::button(D2D1_RECT_F r,const std::wstring& label,bool hovered,bool primary) {float s=scaleFor(hwnd);fill(r,primary?(hovered?0x9AEBD4:Accent):(hovered?0x34465F:Card),1,8*s);text(label,r,12*s,primary?Background:Ink,true,DWRITE_TEXT_ALIGNMENT_CENTER);}
bool inside(D2D1_RECT_F r,POINT p) {return p.x>=r.left && p.x<r.right && p.y>=r.top && p.y<r.bottom;}
float scaleFor(HWND hwnd) {return GetDpiForWindow(hwnd)/96.f;}
void prepareWindow(HWND hwnd) {BOOL yes=TRUE;DwmSetWindowAttribute(hwnd,DWMWA_TRANSITIONS_FORCEDISABLED,&yes,sizeof(yes));DWORD corner=2;DwmSetWindowAttribute(hwnd,33,&corner,sizeof(corner));SetWindowDisplayAffinity(hwnd,0x11);}
}
