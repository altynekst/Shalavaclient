#pragma once

#define NOMINMAX
#include <windows.h>
#include <string>
#include <vector>

namespace Utils
{
    std::wstring Utf8ToWide(const std::string &value);
    std::string WideToUtf8(const std::wstring &value);

    std::wstring GetAppDataPath();
    std::wstring GetExeDir();
    std::wstring JoinPath(const std::wstring &base, const std::wstring &file);

    bool WriteTextFile(const std::wstring &path, const std::string &data);
    std::string ReadTextFile(const std::wstring &path);
    bool FileExists(const std::wstring &path);
    bool CreateDirsForFile(const std::wstring &path);

    std::string GetMachineGuid();
    std::string GetHwid();
}