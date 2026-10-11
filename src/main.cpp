#include "app.h"
#include <exception>
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,LPWSTR,int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    HRESULT com=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);if(FAILED(com))return 1;
    int argc=0;LPWSTR* argv=CommandLineToArgvW(GetCommandLineW(),&argc);int result=0;
    try {
        wchar_t exe[32768];DWORD count=GetModuleFileNameW(nullptr,exe,32768);if(!count || count>=32768)throw std::runtime_error("Executable path unavailable");
        sd::fs::path directory=sd::fs::path(exe).parent_path()/L"data";
        bool testing=false;sd::fs::path testDirectory;
        for(int i=1;i<argc;++i) {
            std::wstring arg=argv[i];
            if(arg==L"--self-test" && i+1<argc){testing=true;testDirectory=sd::fs::absolute(argv[++i]);}
            else if(arg==L"--data-dir" && i+1<argc)directory=sd::fs::absolute(argv[++i]);
            else {MessageBoxW(nullptr,L"用法：ScreenDock.exe [--data-dir 文件夹]\n测试：ScreenDock.exe --self-test 输出文件夹",L"ScreenDock",MB_OK);LocalFree(argv);CoUninitialize();return 2;}
        }
        if(testing)result=sd::selfTest(instance,testDirectory);
        else {
            HANDLE single=CreateMutexW(nullptr,TRUE,L"Local\\ScreenDock.User.Instance.v1");
            if(single && GetLastError()==ERROR_ALREADY_EXISTS) {HWND existing=FindWindowW(L"ScreenDock.Edge",nullptr);if(existing){PostMessageW(existing,sd::TrayMessage,0,WM_LBUTTONUP);}CloseHandle(single);LocalFree(argv);CoUninitialize();return 0;}
            {
                sd::App app(instance);
                if(!app.init(directory)) {MessageBoxW(nullptr,L"启动失败：无法创建数据目录或窗口。",L"ScreenDock",MB_ICONERROR);result=1;}
                else {MSG msg;BOOL status;while((status=GetMessageW(&msg,nullptr,0,0))>0){TranslateMessage(&msg);DispatchMessageW(&msg);}if(status<0)result=1;}
            }
            if(single){ReleaseMutex(single);CloseHandle(single);}
        }
    } catch(const std::exception&) {MessageBoxW(nullptr,L"出现无法继续的错误（可能内存不足）。已归档的截图仍在本地数据目录。",L"ScreenDock",MB_ICONERROR);result=1;}
    if(argv)LocalFree(argv);CoUninitialize();return result;
}
