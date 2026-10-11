#include "core.h"
#include <sstream>
#include <iomanip>
#include <limits>
#include <cmath>
#include <shellapi.h>
#include <shlobj.h>

namespace sd {
bool Pixels::valid() const { return w>0 && h>0 && w<=32768 && h<=32768 && bgra.size()==size_t(w)*h*4; }
RECT normalized(POINT a, POINT b) { return {std::min(a.x,b.x),std::min(a.y,b.y),std::max(a.x,b.x),std::max(a.y,b.y)}; }
RECT intersect(RECT a, RECT b) { RECT r{}; IntersectRect(&r,&a,&b); return r; }
RECT dockRect(RECT work, Edge edge, bool expanded, int dpi) {
    const int u=MulDiv(expanded?310:12,dpi,96), v=MulDiv(expanded?640:112,dpi,96);
    const int width=work.right-work.left, height=work.bottom-work.top;
    int w=std::min(u,width), h=std::min(v,height);
    if (edge==Edge::Top || edge==Edge::Bottom) { w=std::min(v,width); h=std::min(MulDiv(expanded?360:12,dpi,96),height); }
    int x=work.left+(width-w)/2, y=work.top+(height-h)/2;
    if(edge==Edge::Left) x=work.left;
    if(edge==Edge::Right) x=work.right-w;
    if(edge==Edge::Top) y=work.top;
    if(edge==Edge::Bottom) y=work.bottom-h;
    return {x,y,x+w,y+h};
}
float zoomValue(float current, int delta) { return std::clamp(current*std::pow(1.12f,delta/120.0f),0.08f,8.0f); }
Picture crop(const Pixels& source, RECT area) {
    if(!source.valid()) return {};
    area=intersect(area,{0,0,LONG(source.w),LONG(source.h)});
    if(area.right<=area.left || area.bottom<=area.top) return {};
    auto p=std::make_shared<Pixels>(); p->w=area.right-area.left; p->h=area.bottom-area.top;
    p->bgra.resize(size_t(p->w)*p->h*4);
    for(UINT y=0;y<p->h;++y) memcpy(p->bgra.data()+size_t(y)*p->w*4, source.bgra.data()+(size_t(y+area.top)*source.w+area.left)*4,size_t(p->w)*4);
    return p;
}
static Com<IWICImagingFactory> wic() {
    Com<IWICImagingFactory> f; CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&f)); return f;
}
static Picture fromSource(IWICBitmapSource* source, UINT maxSide) {
    if(!source) return {};
    UINT w=0,h=0; if(FAILED(source->GetSize(&w,&h)) || !w || !h || w>32768 || h>32768) return {};
    auto f=wic(); if(!f) return {};
    Com<IWICBitmapSource> src=source;
    if(maxSide && std::max(w,h)>maxSide) {
        float factor=float(maxSide)/std::max(w,h); w=std::max(1u,UINT(w*factor)); h=std::max(1u,UINT(h*factor));
        Com<IWICBitmapScaler> scaler; if(FAILED(f->CreateBitmapScaler(&scaler)) || FAILED(scaler->Initialize(src.Get(),w,h,WICBitmapInterpolationModeFant))) return {};
        src=scaler;
    }
    if(uint64_t(w)*h*4>512ull*1024*1024) return {};
    Com<IWICFormatConverter> convert;
    if(FAILED(f->CreateFormatConverter(&convert)) || FAILED(convert->Initialize(src.Get(),GUID_WICPixelFormat32bppPBGRA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom))) return {};
    auto out=std::make_shared<Pixels>(); out->w=w; out->h=h; out->bgra.resize(size_t(w)*h*4);
    if(FAILED(convert->CopyPixels(nullptr,w*4,UINT(out->bgra.size()),out->bgra.data()))) return {};
    return out;
}
Picture thumbnail(const Pixels& source, UINT maxSide) {
    if(!source.valid()) return {};
    auto f=wic(); if(!f) return {};
    Com<IWICBitmap> bmp;
    if(FAILED(f->CreateBitmapFromMemory(source.w,source.h,GUID_WICPixelFormat32bppPBGRA,source.w*4,UINT(source.bgra.size()),const_cast<BYTE*>(source.bgra.data()),&bmp))) return {};
    return fromSource(bmp.Get(),maxSide);
}
Picture readImage(const fs::path& path, UINT maxSide) {
    auto f=wic(); if(!f) return {};
    Com<IWICBitmapDecoder> decoder; Com<IWICBitmapFrameDecode> frame;
    if(FAILED(f->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnDemand,&decoder)) || FAILED(decoder->GetFrame(0,&frame))) return {};
    return fromSource(frame.Get(),maxSide);
}
bool savePng(const Pixels& image, const fs::path& path) {
    if(!image.valid()) return false;
    auto f=wic(); if(!f) return false;
    const fs::path temp=path.wstring()+L".partial";
    struct Cleanup {fs::path path;~Cleanup(){DeleteFileW(path.c_str());}} cleanup{temp};
    Com<IWICStream> stream; Com<IWICBitmapEncoder> encoder; Com<IWICBitmapFrameEncode> frame;
    if(FAILED(f->CreateStream(&stream)) || FAILED(stream->InitializeFromFilename(temp.c_str(),GENERIC_WRITE)) ||
       FAILED(f->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder)) || FAILED(encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache)) ||
       FAILED(encoder->CreateNewFrame(&frame,nullptr)) || FAILED(frame->Initialize(nullptr)) || FAILED(frame->SetSize(image.w,image.h))) return false;
    WICPixelFormatGUID format=GUID_WICPixelFormat32bppBGRA;
    if(FAILED(frame->SetPixelFormat(&format)) || format!=GUID_WICPixelFormat32bppBGRA) return false;
    // Screenshots are opaque; clipboard sources are converted back from premultiplied BGRA.
    std::vector<BYTE> straight=image.bgra;
    for(size_t n=0;n<straight.size();n+=4) { BYTE a=straight[n+3]; if(a && a!=255) for(int c=0;c<3;++c) straight[n+c]=BYTE(std::min(255,straight[n+c]*255/a)); }
    bool good=SUCCEEDED(frame->WritePixels(image.h,image.w*4,UINT(straight.size()),straight.data())) && SUCCEEDED(frame->Commit()) && SUCCEEDED(encoder->Commit());
    frame.Reset(); encoder.Reset(); stream.Reset();
    if(!good) return false;
    if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_WRITE_THROUGH)) return false;
    return true;
}
Picture captureDesktop(RECT area) {
    const int w=area.right-area.left,h=area.bottom-area.top;
    if(w<=0 || h<=0 || uint64_t(w)*h*4>512ull*1024*1024) return {};
    HDC screen=GetDC(nullptr), mem=CreateCompatibleDC(screen); void* bits=nullptr;
    BITMAPINFO bi{}; bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=w;bi.bmiHeader.biHeight=-h;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
    HBITMAP bmp=CreateDIBSection(screen,&bi,DIB_RGB_COLORS,&bits,nullptr,0);
    Picture out;
    if(bmp && mem) {
        auto old=SelectObject(mem,bmp);
        if(BitBlt(mem,0,0,w,h,screen,area.left,area.top,SRCCOPY|CAPTUREBLT)) { out=std::make_shared<Pixels>();out->w=w;out->h=h;out->bgra.assign(static_cast<BYTE*>(bits),static_cast<BYTE*>(bits)+size_t(w)*h*4);for(size_t i=3;i<out->bgra.size();i+=4) out->bgra[i]=255; }
        SelectObject(mem,old);
    }
    if(bmp) DeleteObject(bmp); if(mem) DeleteDC(mem); if(screen) ReleaseDC(nullptr,screen);
    return out;
}
bool copyPicture(HWND owner, const Pixels& image) {
    if(!image.valid() || !OpenClipboard(owner)) return false;
    size_t bytes=sizeof(BITMAPV5HEADER)+image.bgra.size(); HGLOBAL data=GlobalAlloc(GMEM_MOVEABLE,bytes);
    if(!data) { CloseClipboard(); return false; }
    auto* header=static_cast<BITMAPV5HEADER*>(GlobalLock(data));
    if(!header) { GlobalFree(data);CloseClipboard();return false; }
    *header={};header->bV5Size=sizeof(*header);header->bV5Width=image.w;header->bV5Height=-LONG(image.h);header->bV5Planes=1;header->bV5BitCount=32;header->bV5Compression=BI_BITFIELDS;
    header->bV5RedMask=0x00ff0000;header->bV5GreenMask=0x0000ff00;header->bV5BlueMask=0x000000ff;header->bV5AlphaMask=0xff000000;header->bV5CSType=LCS_sRGB;
    BYTE* output=reinterpret_cast<BYTE*>(header+1);memcpy(output,image.bgra.data(),image.bgra.size());
    for(size_t i=0;i<image.bgra.size();i+=4) { BYTE a=output[i+3];if(a && a!=255) for(int c=0;c<3;++c) output[i+c]=BYTE(std::min(255,output[i+c]*255/a)); }
    GlobalUnlock(data);EmptyClipboard();bool success=SetClipboardData(CF_DIBV5,data)!=nullptr;if(!success)GlobalFree(data);CloseClipboard();return success;
}
HGLOBAL fileDropData(const fs::path& path) {
    std::error_code ec;fs::path absolute=fs::absolute(path,ec);
    if(ec || !fs::is_regular_file(absolute,ec) || ec)return nullptr;
    auto value=absolute.lexically_normal().wstring();
    size_t bytes=sizeof(DROPFILES)+(value.size()+2)*sizeof(wchar_t);
    HGLOBAL data=GlobalAlloc(GMEM_MOVEABLE|GMEM_ZEROINIT,bytes);if(!data)return nullptr;
    auto* drop=static_cast<DROPFILES*>(GlobalLock(data));if(!drop){GlobalFree(data);return nullptr;}
    drop->pFiles=sizeof(DROPFILES);drop->fWide=TRUE;
    memcpy(reinterpret_cast<BYTE*>(drop)+sizeof(DROPFILES),value.c_str(),(value.size()+1)*sizeof(wchar_t));
    GlobalUnlock(data);return data;
}
bool copyFile(HWND owner,const fs::path& path) {
    HGLOBAL files=fileDropData(path);if(!files)return false;
    UINT preferred=RegisterClipboardFormatW(CFSTR_PREFERREDDROPEFFECT);
    HGLOBAL effect=GlobalAlloc(GMEM_MOVEABLE,sizeof(DWORD));
    auto* value=effect?static_cast<DWORD*>(GlobalLock(effect)):nullptr;
    if(!preferred || !value){if(effect)GlobalFree(effect);GlobalFree(files);return false;}
    *value=DROPEFFECT_COPY;GlobalUnlock(effect);
    if(!OpenClipboard(owner)){GlobalFree(files);GlobalFree(effect);return false;}
    if(!EmptyClipboard()){CloseClipboard();GlobalFree(files);GlobalFree(effect);return false;}
    if(!SetClipboardData(CF_HDROP,files)){CloseClipboard();GlobalFree(files);GlobalFree(effect);return false;}
    if(!SetClipboardData(preferred,effect)){EmptyClipboard();CloseClipboard();GlobalFree(effect);return false;}
    CloseClipboard();return true;
}
Picture pastePicture(HWND owner) {
    if(!OpenClipboard(owner)) return {};
    Picture result; HBITMAP bmp=static_cast<HBITMAP>(GetClipboardData(CF_BITMAP));
    if(bmp) { auto f=wic();Com<IWICBitmap> source;if(f && SUCCEEDED(f->CreateBitmapFromHBITMAP(bmp,nullptr,WICBitmapIgnoreAlpha,&source)))result=fromSource(source.Get(),0); }
    CloseClipboard();return result;
}
std::vector<Monitor> monitors() {
    std::vector<Monitor> result;
    EnumDisplayMonitors(nullptr,nullptr,[](HMONITOR mon,HDC,LPRECT,LPARAM arg)->BOOL {MONITORINFOEXW info{};info.cbSize=sizeof(info);GetMonitorInfoW(mon,&info);reinterpret_cast<std::vector<Monitor>*>(arg)->push_back({info.rcWork,info.rcMonitor,info.szDevice});return TRUE;},reinterpret_cast<LPARAM>(&result));return result;
}
std::wstring lastError(DWORD error) { wchar_t* p=nullptr;FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,nullptr,error,0,reinterpret_cast<LPWSTR>(&p),0,nullptr);std::wstring s=p?p:L"未知错误";if(p)LocalFree(p);return s; }
std::wstring stamp() { SYSTEMTIME t;GetLocalTime(&t);wchar_t s[64];swprintf_s(s,L"%04u%02u%02u_%02u%02u%02u_%03u",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,t.wMilliseconds);return s; }
std::wstring friendlyTime(const fs::path& file) { auto s=file.stem().wstring();if(s.size()>=19 && s[8]==L'_')return s.substr(4,2)+L"/"+s.substr(6,2)+L"  "+s.substr(9,2)+L":"+s.substr(11,2)+L":"+s.substr(13,2);return s; }
Worker::Worker() : thread_([this] { CoInitializeEx(nullptr,COINIT_MULTITHREADED);for(;;) {std::function<void()> fn;{std::unique_lock lock(mutex_);cv_.wait(lock,[this]{return stopping_ || !tasks_.empty();});if(tasks_.empty() && stopping_)break;fn=std::move(tasks_.front());tasks_.pop_front();}try{fn();}catch(...){OutputDebugStringW(L"ScreenDock worker task failed\n");}}CoUninitialize();}) {}
Worker::~Worker() { {std::lock_guard lock(mutex_);stopping_=true;}cv_.notify_all();if(thread_.joinable())thread_.join(); }
void Worker::push(std::function<void()> fn) { {std::lock_guard lock(mutex_);tasks_.push_back(std::move(fn));}cv_.notify_one(); }
}
