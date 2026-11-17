#pragma once
#include <windows.h>
#include <filesystem>
#include <string>

namespace sd {
std::wstring startupCommand(const std::filesystem::path& executable,
                           const std::filesystem::path& dataDirectory);

class StartupRegistration {
    HKEY root_;
    std::wstring key_;
public:
    explicit StartupRegistration(HKEY root=HKEY_CURRENT_USER,
        std::wstring key=L"Software\\Microsoft\\Windows\\CurrentVersion\\Run");
    LSTATUS query(bool& enabled) const;
    LSTATUS setEnabled(bool enabled,const std::wstring& command={}) const;
};
}
