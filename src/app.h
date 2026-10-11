#pragma once
#include "ui.h"
#include "history_index.h"
#include <shellapi.h>
namespace sd {
constexpr UINT TrayMessage=WM_APP+1, SaveDone=WM_APP+2, CaptureDone=WM_APP+3, RemovePin=WM_APP+4, ThumbDone=WM_APP+5;
constexpr UINT HotkeyId=0x5344, CaptureTimer=2, HoverTimer=1;
struct Record {
    fs::path path;std::wstring label;Picture thumb;Picture pending;
    UINT width=0,height=0;bool saving=false,failed=false,previewLoading=false,previewFailed=false;
};
class App;class Pin;class Capture;
class Dock {
    App& app_;Graphics graphics_;POINT mouse_{-1,-1};
    size_t first_=0;int thumbSize_=120;bool tracking_=false;
    struct CardHit {size_t index;D2D1_RECT_F body,copyFile,remove;};
    std::vector<CardHit> cards_;fs::path copiedPath_;ULONGLONG copiedUntil_=0;
    D2D1_RECT_F shot_{},paste_{},folder_{},menu_{},previous_{},next_{};
    void paint();LRESULT message(UINT,WPARAM,LPARAM);
    static LRESULT CALLBACK proc(HWND,UINT,WPARAM,LPARAM);
public:
    HWND hwnd=nullptr;bool expanded=false;bool menuOpen=false;ULONGLONG leaveAt=0;
    explicit Dock(App& app):app_(app){};~Dock();
    bool create();void expand(bool value);void reposition();void refresh(bool newest=false);void showMenu(POINT p);
    int thumbnailSize() const {return thumbSize_;}
    bool recordButtons(const fs::path& path,RECT& copy,RECT& remove) const;
};
class Pin {
    App& app_;Graphics graphics_;std::shared_ptr<Record> record_;Picture image_;
    float zoom_=1,panX_=0,panY_=0,dragPanX_=0,dragPanY_=0;int opacity_=255;POINT mouse_{-1,-1};bool hovering_=false,dragging_=false,panning_=false;
    POINT dragOrigin_{};RECT dragWindow_{};D2D1_RECT_F close_{},copy_{},actual_{};
    bool nearest_=false;void paint();void menu(POINT p);void clampPan();LRESULT message(UINT,WPARAM,LPARAM);
    static LRESULT CALLBACK proc(HWND,UINT,WPARAM,LPARAM);
public:
    HWND hwnd=nullptr;
    Pin(App& app,std::shared_ptr<Record> record,Picture image):app_(app),record_(std::move(record)),image_(std::move(image)){};
    ~Pin();bool create(int index);void resize(float zoom,POINT anchor);void show(bool visible);
    float zoom() const {return zoom_;}const fs::path& path() const {return record_->path;}
};
class Capture {
    App& app_;Graphics graphics_;Picture desktop_;RECT virtual_{};POINT start_{},end_{};bool selecting_=false,finished_=false;
    void paint();void finish(bool accepted);LRESULT message(UINT,WPARAM,LPARAM);
    static LRESULT CALLBACK proc(HWND,UINT,WPARAM,LPARAM);
public:
    HWND hwnd=nullptr;
    Capture(App& app,Picture desktop,RECT area):app_(app),desktop_(std::move(desktop)),virtual_(area){};
    ~Capture();bool create();
};
struct SaveResult {std::shared_ptr<Record> record;bool success;};
struct ThumbResult {std::shared_ptr<Record> record;Picture image;};
class App {
    HICON icon_=nullptr;bool trayAdded_=false;std::unique_ptr<Worker> worker_;
    std::atomic<unsigned> sequence_{0};
    HistoryIndex historyIndex_;
public:
    HINSTANCE instance;fs::path data,history,config;std::unique_ptr<Dock> dock;
    std::vector<std::shared_ptr<Record>> records;std::vector<std::unique_ptr<Pin>> pins;std::unique_ptr<Capture> capture;
    std::vector<Monitor> displays;Edge edge=Edge::Right;size_t monitor=0;
    bool autoPin=true,capturing=false,testMode=false,hotkeyRegistered=false,shuttingDown=false;
    explicit App(HINSTANCE inst):instance(inst){};~App();
    bool init(fs::path directory,bool testing=false);void loadHistory();void saveConfig();void updateMonitors();
    void notify(const std::wstring& value,bool error=false);void openFolder();void openRecord(const fs::path& path);
    void beginCapture();void captureNow();void finishCapture(Picture picture);
    std::shared_ptr<Record> add(Picture image,bool pin=true);void pin(std::shared_ptr<Record> record);void removePin(Pin* pin);
    void paste();void setEdge(Edge value);void setMonitor(size_t index);void hidePins(bool hidden);
    void saveComplete(SaveResult* result);void shutdown();LRESULT controller(UINT,WPARAM,LPARAM,bool& handled);
    void requestThumbnail(const std::shared_ptr<Record>& record);
    bool hideRecord(const std::shared_ptr<Record>& record);bool restoreHistory();bool copyRecordFile(const std::shared_ptr<Record>& record);
    HICON icon() const {return icon_;}
};
int selfTest(HINSTANCE instance,const fs::path& directory);
int clipboardSelfTest(const fs::path& source,const fs::path& destination);
}
