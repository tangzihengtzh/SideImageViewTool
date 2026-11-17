#include "app.h"
#include <fstream>
#include <windowsx.h>

namespace sd {
int dockBackgroundTest(HINSTANCE instance,const fs::path& directory) {
    struct ComScope {
        HRESULT status=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
        ~ComScope(){if(SUCCEEDED(status))CoUninitialize();}
    } com;
    std::ofstream report;
    try {
        std::error_code ec;fs::create_directories(directory,ec);if(ec)return 2;
        report.open(directory/L"dock-background-report.txt");if(!report)return 2;
        int count=0,failures=0;
        auto check=[&](bool good,const char* label){++count;report<<(good?"PASS ":"FAIL ")<<label<<'\n';report.flush();if(!good)++failures;return good;};
        if(!check(SUCCEEDED(com.status),"background COM initialization"))return 1;
        App app(instance);
        const auto testData=directory/(L"hidden-dock-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()));
        if(!check(app.init(testData,true),"isolated hidden dock initializes without tray or hotkey"))return 1;
        auto& dock=*app.dock;HWND window=dock.hwnd;
        check(!IsWindowVisible(window),"dock starts invisible");
        // Move only the invisible test window; never move the user's pointer.
        auto position=[&](bool inside){POINT cursor{};if(!GetCursorPos(&cursor))return false;return SetWindowPos(window,nullptr,cursor.x+(inside?-500:100000),cursor.y+(inside?-500:100000),1000,1000,SWP_NOZORDER|SWP_NOACTIVATE)!=FALSE;};
        auto outsideLeave=[&](){if(!position(false))return false;SendMessageW(window,WM_MOUSELEAVE,0,0);return true;};
        SendMessageW(window,WM_MOUSEMOVE,0,MAKELPARAM(1,1));
        check(dock.expanded,"first mouse-entry message expands immediately");
        check(outsideLeave() && !dock.expanded,"leave message collapses synchronously without a timer or wait");
        SendMessageW(window,WM_MOUSEMOVE,0,MAKELPARAM(1,1));
        check(dock.expanded,"reentry expands immediately");
        check(outsideLeave() && !dock.expanded,"rapid repeated leave collapses synchronously");
        dock.expand(true);dock.menuOpen=true;
        check(outsideLeave() && dock.expanded,"active menu protects expanded dock");
        dock.menuOpen=false;dock.collapseIfOutside();
        check(!dock.expanded,"menu completion collapses outside dock without delay");
        dock.expand(true);app.capturing=true;
        check(outsideLeave() && dock.expanded,"active capture protects dock from leave handling");
        app.capturing=false;dock.collapseIfOutside();
        check(!dock.expanded,"outside check collapses once capture protection ends");
        dock.expand(true);
        if(check(position(true),"invisible test rectangle follows current pointer without moving it")) {
            dock.collapseIfOutside();
            check(dock.expanded,"menu completion keeps dock expanded when pointer is inside");
            check(outsideLeave() && !dock.expanded,"subsequent leave still collapses immediately");
        }
        check(!IsWindowVisible(window) && GetForegroundWindow()!=window,"test dock remains invisible and never becomes foreground");
        app.shutdown();check(!IsWindow(window),"hidden test window is released");
        report<<count<<" checks, "<<failures<<" failures\n";
        return failures?1:0;
    } catch(...) {if(report)report<<"FAIL unexpected background-test error\n";return 1;}
}
}
