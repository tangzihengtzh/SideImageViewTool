#include "startup.h"
#include <utility>
#include <vector>

namespace sd {
namespace {
struct RegistryKey {
    HKEY value=nullptr;
    ~RegistryKey(){if(value)RegCloseKey(value);}
};
bool missing(LSTATUS status){return status==ERROR_FILE_NOT_FOUND || status==ERROR_PATH_NOT_FOUND;}
std::wstring quoteArgument(const std::wstring& value) {
    std::wstring result=L"\"";
    size_t slashes=0;
    for(wchar_t c:value) {
        if(c==L'\\'){++slashes;continue;}
        result.append(c==L'"'?slashes*2+1:slashes,L'\\');
        result+=c;slashes=0;
    }
    result.append(slashes*2,L'\\');result+=L'"';return result;
}
}

std::wstring startupCommand(const std::filesystem::path& executable,
                           const std::filesystem::path& dataDirectory) {
    return quoteArgument(executable.wstring())+L" --startup --data-dir "+quoteArgument(dataDirectory.wstring());
}
StartupRegistration::StartupRegistration(HKEY root,std::wstring key):root_(root),key_(std::move(key)){}

LSTATUS StartupRegistration::query(bool& enabled) const {
    enabled=false;if(!root_)return ERROR_INVALID_HANDLE;
    RegistryKey key;
    LSTATUS status=RegOpenKeyExW(root_,key_.c_str(),0,KEY_QUERY_VALUE,&key.value);
    if(missing(status))return ERROR_SUCCESS;if(status!=ERROR_SUCCESS)return status;
    DWORD type=0,bytes=0;
    status=RegQueryValueExW(key.value,L"ScreenDock",nullptr,&type,nullptr,&bytes);
    if(missing(status))return ERROR_SUCCESS;if(status!=ERROR_SUCCESS)return status;
    if(type!=REG_SZ)return ERROR_INVALID_DATATYPE;
    if(bytes<2*sizeof(wchar_t) || bytes>261*sizeof(wchar_t) || bytes%sizeof(wchar_t))return ERROR_INVALID_DATA;
    std::vector<wchar_t> command(bytes/sizeof(wchar_t));
    status=RegQueryValueExW(key.value,L"ScreenDock",nullptr,&type,reinterpret_cast<BYTE*>(command.data()),&bytes);
    if(status!=ERROR_SUCCESS)return status;
    if(type!=REG_SZ || bytes<2*sizeof(wchar_t) || bytes%sizeof(wchar_t))return ERROR_INVALID_DATA;
    const size_t length=bytes/sizeof(wchar_t);
    if(command[length-1]!=0 || command[0]==0)return ERROR_INVALID_DATA;
    for(size_t i=0;i+1<length;++i)if(command[i]==0)return ERROR_INVALID_DATA;
    enabled=true;return ERROR_SUCCESS;
}
LSTATUS StartupRegistration::setEnabled(bool enabled,const std::wstring& command) const {
    if(!root_)return ERROR_INVALID_HANDLE;
    if(enabled && (command.empty() || command.size()>260 || command.find(L'\0')!=std::wstring::npos))return ERROR_INVALID_PARAMETER;
    RegistryKey key;
    if(enabled) {
        LSTATUS status=RegCreateKeyExW(root_,key_.c_str(),0,nullptr,REG_OPTION_NON_VOLATILE,KEY_SET_VALUE,nullptr,&key.value,nullptr);
        if(status!=ERROR_SUCCESS)return status;
        return RegSetValueExW(key.value,L"ScreenDock",0,REG_SZ,reinterpret_cast<const BYTE*>(command.c_str()),DWORD((command.size()+1)*sizeof(wchar_t)));
    }
    LSTATUS status=RegOpenKeyExW(root_,key_.c_str(),0,KEY_SET_VALUE,&key.value);
    if(missing(status))return ERROR_SUCCESS;if(status!=ERROR_SUCCESS)return status;
    status=RegDeleteValueW(key.value,L"ScreenDock");return missing(status)?ERROR_SUCCESS:status;
}
}
