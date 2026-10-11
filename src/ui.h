#pragma once
#include "core.h"
#include <d2d1.h>
#include <dwrite.h>
#include <cmath>
#include <unordered_map>
namespace sd {
inline D2D1_COLOR_F color(unsigned rgb, float alpha=1) { return D2D1::ColorF((rgb>>16&255)/255.f,(rgb>>8&255)/255.f,(rgb&255)/255.f,alpha); }
constexpr unsigned Ink=0xEAF0F7, Muted=0x92A1B6, Background=0x131C2A, Card=0x202D40, Accent=0x75E2C2;
struct Graphics {
    HWND hwnd=nullptr;Com<ID2D1HwndRenderTarget> rt;Com<ID2D1SolidColorBrush> brush;
    std::unordered_map<const Pixels*,Com<ID2D1Bitmap>> bitmaps;
    bool begin(HWND window); HRESULT end(); void reset();
    void fill(D2D1_RECT_F r, unsigned rgb, float alpha=1, float radius=0);
    void line(float x1,float y1,float x2,float y2,unsigned rgb,float thickness=1);
    void text(const std::wstring& text,D2D1_RECT_F r,float size=14,unsigned rgb=Ink,bool bold=false,DWRITE_TEXT_ALIGNMENT align=DWRITE_TEXT_ALIGNMENT_LEADING);
    void image(const Picture& pixels,D2D1_RECT_F destination,float opacity=1,bool nearest=false,const D2D1_RECT_F* source=nullptr);
    void button(D2D1_RECT_F r,const std::wstring& label,bool hovered=false,bool primary=false);
};
bool inside(D2D1_RECT_F r,POINT p);
float scaleFor(HWND hwnd);
void prepareWindow(HWND hwnd);
}
