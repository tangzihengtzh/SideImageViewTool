#include "startup.h"
#include <shellapi.h>
#include <fstream>
#include <iostream>
#include <vector>

namespace {
struct TestRegistry {
    std::wstring path=L"Software\\ScreenDock\\Tests\\Startup-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64());
    HKEY root=nullptr;
    LSTATUS create(){return RegCreateKeyExW(HKEY_CURRENT_USER,path.c_str(),0,nullptr,REG_OPTION_NON_VOLATILE,KEY_ALL_ACCESS,nullptr,&root,nullptr);}
    LSTATUS cleanup(){if(root){RegCloseKey(root);root=nullptr;}return RegDeleteTreeW(HKEY_CURRENT_USER,path.c_str());}
    ~TestRegistry(){if(root)cleanup();}
};
std::vector<std::wstring> arguments(const std::wstring& command) {
    int count=0;auto values=CommandLineToArgvW(command.c_str(),&count);std::vector<std::wstring> result;
    if(values){for(int i=0;i<count;++i)result.emplace_back(values[i]);LocalFree(values);}return result;
}
}

int wmain(int argc,wchar_t** argv) {
    std::ofstream report;
    if(argc==2){std::filesystem::path file=argv[1];std::error_code ec;std::filesystem::create_directories(file.parent_path(),ec);report.open(file);if(ec || !report)return 2;}
    int checks=0,failures=0;
    auto check=[&](bool success,const char* message){++checks;if(!success)++failures;std::cout<<(success?"PASS ":"FAIL ")<<message<<'\n';if(report)report<<(success?"PASS ":"FAIL ")<<message<<'\n';return success;};
    const std::filesystem::path exe=L"C:\\Tools\\屏幕 便签\\ScreenDock.exe",data=L"D:\\工作 截图\\data";
    auto command=sd::startupCommand(exe,data);auto parsed=arguments(command);
    check(parsed==std::vector<std::wstring>{exe.wstring(),L"--startup",L"--data-dir",data.wstring()},"Unicode and space paths round-trip through Windows argument parser");
    parsed=arguments(sd::startupCommand(exe,L"D:\\"));
    check(parsed.size()==4 && parsed[3]==L"D:\\","drive-root trailing slash is quoted correctly");
    parsed=arguments(sd::startupCommand(exe,L"D:\\work folder\\"));
    check(parsed.size()==4 && parsed[3]==L"D:\\work folder\\","folder trailing slash is quoted correctly");

    TestRegistry fixture;
    if(!check(fixture.create()==ERROR_SUCCESS,"isolated current-user test registry tree created"))return 1;
    sd::StartupRegistration registration(fixture.root,L"Run");bool enabled=true;
    check(registration.query(enabled)==ERROR_SUCCESS && !enabled,"missing startup key defaults to disabled");
    check(registration.setEnabled(false)==ERROR_SUCCESS,"disabling absent key is idempotent");
    HKEY key=nullptr;LSTATUS status=RegOpenKeyExW(fixture.root,L"Run",0,KEY_READ,&key);if(key)RegCloseKey(key);
    check(status==ERROR_FILE_NOT_FOUND,"query and disable never create missing keys");
    check(registration.setEnabled(true,command)==ERROR_SUCCESS,"enable writes startup registration");
    sd::StartupRegistration reopened(fixture.root,L"Run");
    check(reopened.query(enabled)==ERROR_SUCCESS && enabled,"enabled setting survives a new registration object");
    status=RegOpenKeyExW(fixture.root,L"Run",0,KEY_ALL_ACCESS,&key);
    if(!check(status==ERROR_SUCCESS,"test Run key opens for integration checks"))return 1;
    wchar_t stored[512]{};DWORD type=0,bytes=sizeof(stored);
    status=RegQueryValueExW(key,L"ScreenDock",nullptr,&type,reinterpret_cast<BYTE*>(stored),&bytes);
    check(status==ERROR_SUCCESS && type==REG_SZ && command==stored && bytes==(command.size()+1)*sizeof(wchar_t),"actual REG_SZ stores exact quoted command and terminator");
    const DWORD sentinel=0x12345678;
    check(RegSetValueExW(key,L"OtherApplication",0,REG_DWORD,reinterpret_cast<const BYTE*>(&sentinel),sizeof(sentinel))==ERROR_SUCCESS,"unrelated startup value fixture written");
    auto movedCommand=sd::startupCommand(L"E:\\Portable\\ScreenDock.exe",data);
    check(reopened.setEnabled(true,movedCommand)==ERROR_SUCCESS,"enabling again updates executable location");
    bytes=sizeof(stored);status=RegQueryValueExW(key,L"ScreenDock",nullptr,&type,reinterpret_cast<BYTE*>(stored),&bytes);
    check(status==ERROR_SUCCESS && movedCommand==stored,"updated executable command is persisted");
    check(reopened.setEnabled(true,L"")==ERROR_INVALID_PARAMETER && reopened.setEnabled(true,std::wstring(261,L'x'))==ERROR_INVALID_PARAMETER && reopened.setEnabled(true,std::wstring(L"ab\0cd",5))==ERROR_INVALID_PARAMETER,"empty, oversized and embedded-NUL commands are rejected");
    bytes=sizeof(stored);status=RegQueryValueExW(key,L"ScreenDock",nullptr,&type,reinterpret_cast<BYTE*>(stored),&bytes);
    check(status==ERROR_SUCCESS && movedCommand==stored,"invalid enable leaves existing command untouched");
    check(reopened.setEnabled(false)==ERROR_SUCCESS && reopened.query(enabled)==ERROR_SUCCESS && !enabled,"disable removes only own registration");
    bytes=sizeof(stored);check(RegQueryValueExW(key,L"ScreenDock",nullptr,&type,reinterpret_cast<BYTE*>(stored),&bytes)==ERROR_FILE_NOT_FOUND,"own Run value is absent after disable");
    DWORD untouched=0;bytes=sizeof(untouched);status=RegQueryValueExW(key,L"OtherApplication",nullptr,&type,reinterpret_cast<BYTE*>(&untouched),&bytes);
    check(status==ERROR_SUCCESS && type==REG_DWORD && untouched==sentinel,"other startup entries remain unchanged");
    check(reopened.setEnabled(false)==ERROR_SUCCESS,"repeated disable succeeds");
    status=RegSetValueExW(key,L"ScreenDock",0,REG_DWORD,reinterpret_cast<const BYTE*>(&sentinel),sizeof(sentinel));
    check(status==ERROR_SUCCESS && reopened.query(enabled)==ERROR_INVALID_DATATYPE && !enabled,"malformed registry type reports error without claiming enabled");
    const wchar_t unterminated[]={L'a',L'b'};
    status=RegSetValueExW(key,L"ScreenDock",0,REG_SZ,reinterpret_cast<const BYTE*>(unterminated),sizeof(unterminated));
    check(status==ERROR_SUCCESS && reopened.query(enabled)==ERROR_INVALID_DATA && !enabled,"unterminated registry string is rejected");
    check(reopened.setEnabled(false)==ERROR_SUCCESS,"disable can remove malformed own value");
    RegCloseKey(key);
    sd::StartupRegistration invalid(nullptr,L"Run");enabled=true;
    check(invalid.query(enabled)==ERROR_INVALID_HANDLE && !enabled && invalid.setEnabled(true,command)==ERROR_INVALID_HANDLE && invalid.setEnabled(false)==ERROR_INVALID_HANDLE,"registry access failures propagate instead of claiming success");
    check(fixture.cleanup()==ERROR_SUCCESS,"only isolated test registry tree removed");
    std::cout<<checks<<" checks, "<<failures<<" failures\n";if(report)report<<checks<<" checks, "<<failures<<" failures\n";
    return failures?1:0;
}
