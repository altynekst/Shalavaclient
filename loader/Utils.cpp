#include "Utils.h"
#include <shlobj.h>
#include <fstream>
#include <sstream>
#include <functional>

namespace Utils
{

std::wstring Utf8ToWide(const std::string &value)
{
    if(value.empty())
        return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
    std::wstring out(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, out.data(), len);
    out.resize(wcslen(out.c_str()));
    return out;
}

std::string WideToUtf8(const std::wstring &value)
{
    if(value.empty())
        return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string out(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, out.data(), len, nullptr, nullptr);
    out.resize(strlen(out.c_str()));
    return out;
}

std::wstring GetAppDataPath()
{
    PWSTR path = nullptr;
    if(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &path)))
    {
        std::wstring result(path);
        CoTaskMemFree(path);
        result += L"\\Sh";
        CreateDirectoryW(result.c_str(), nullptr);
        return result;
    }
    return L".";
}

std::wstring GetExeDir()
{
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    std::wstring full(path);
    auto pos = full.find_last_of(L"\\/");
    return pos == std::wstring::npos ? L"." : full.substr(0, pos);
}

std::wstring JoinPath(const std::wstring &base, const std::wstring &file)
{
    if(base.empty())
        return file;
    if(base.back() == L'\\')
        return base + file;
    return base + L"\\" + file;
}

bool WriteTextFile(const std::wstring &path, const std::string &data)
{
    std::ofstream out(path, std::ios::binary);
    if(!out)
        return false;
    out.write(data.data(), data.size());
    return true;
}

std::string ReadTextFile(const std::wstring &path)
{
    std::ifstream in(path, std::ios::binary);
    if(!in)
        return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

bool FileExists(const std::wstring &path)
{
    return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool CreateDirsForFile(const std::wstring &path)
{
    auto pos = path.find_last_of(L"\\/");
    if(pos == std::wstring::npos)
        return true;
    std::wstring dir = path.substr(0, pos);
    return SHCreateDirectoryExW(nullptr, dir.c_str(), nullptr) == ERROR_SUCCESS || 
           GetLastError() == ERROR_ALREADY_EXISTS;
}

std::string GetMachineGuid()
{
    HKEY hKey = nullptr;
    if(RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Cryptography", 
                     0, KEY_READ | KEY_WOW64_64KEY, &hKey) != ERROR_SUCCESS)
        return {};
    wchar_t value[256] = {};
    DWORD valueSize = sizeof(value);
    if(RegGetValueW(hKey, nullptr, L"MachineGuid", RRF_RT_REG_SZ, nullptr, value, &valueSize) != ERROR_SUCCESS)
    {
        RegCloseKey(hKey);
        return {};
    }
    RegCloseKey(hKey);
    return WideToUtf8(value);
}

std::string GetHwid()
{
    std::string guid = GetMachineGuid();
    if(guid.empty())
        return "unknown";
    std::hash<std::string> hasher;
    std::ostringstream ss;
    ss << std::hex << hasher(guid);
    return ss.str();
}

}