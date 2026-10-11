#include "Client.h"
#include "Config.h"
#include "Network.h"
#include "Utils.h"
#include "antifrida.h"
#include "obfuscate.h"
#include <json.h>
#include <shlobj.h>
#include <algorithm>

namespace
{

std::string ReadJsonString(json_value *object, const char *name)
{
    if (!object || object->type != json_object)
        return {};
    for (unsigned int i = 0; i < object->u.object.length; ++i)
    {
        const auto &entry = object->u.object.values[i];
        if (strcmp(entry.name, name) == 0 && entry.value->type == json_string)
            return entry.value->u.string.ptr;
    }
    return {};
}

bool ReadJsonBool(json_value *object, const char *name, bool fallback = false)
{
    if (!object || object->type != json_object)
        return fallback;
    for (unsigned int i = 0; i < object->u.object.length; ++i)
    {
        const auto &entry = object->u.object.values[i];
        if (strcmp(entry.name, name) == 0 && entry.value->type == json_boolean)
            return entry.value->u.boolean != 0;
    }
    return fallback;
}

}

namespace Client
{

std::wstring Find7Zip()
{
    std::wstring local = Utils::JoinPath(Utils::GetExeDir(), L"7z.exe");
    if (Utils::FileExists(local))
        return local;
        
    wchar_t path[MAX_PATH] = {};
    DWORD len = SearchPathW(nullptr, L"7z.exe", nullptr, MAX_PATH, path, nullptr);
    if (len > 0)
        return path;
        
    wchar_t programFiles[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PROGRAM_FILES, nullptr, 
                                   SHGFP_TYPE_CURRENT, programFiles)))
    {
        std::wstring candidate = std::wstring(programFiles) + L"\\7-Zip\\7z.exe";
        if (Utils::FileExists(candidate))
            return candidate;
    }
    return L"";
}

std::string FetchArchiveVersion(const std::string &token)
{
    if (AntiFrida::IsDetected())
        AntiFrida::TriggerAntiUnpack();
    
    std::string url = std::string(Config::GetApiBase()) + 
                      Config::GetVersionEndpoint() + 
                      "?token=" + token;
    std::wstring wurl = Utils::Utf8ToWide(url);
    std::string response;
    std::string body;
    
    if (!Network::HttpRequest(wurl, "GET", body, response))
        return {};
        
    json_value *root = json_parse(response.c_str(), response.size());
    if (!root || root->type != json_object)
    {
        if (root)
            json_value_free(root);
        return {};
    }
    
    if (!ReadJsonBool(root, "ok"))
    {
        json_value_free(root);
        return {};
    }
    
    std::string version = ReadJsonString(root, "version");
    json_value_free(root);
    return version;
}

bool DownloadClientArchive(const std::string &token, const std::wstring &outputPath,
                           long long &bytesWritten, long long &totalBytes,
                           std::function<void(long long)> onProgress)
{
    if (AntiFrida::IsDetected())
        AntiFrida::TriggerAntiUnpack();
    
    std::string url = std::string(Config::GetApiBase()) + 
                      Config::GetArchiveEndpoint() + 
                      "?token=" + token;
    std::wstring wurl = Utils::Utf8ToWide(url);
    return Network::HttpDownloadToFile(wurl, outputPath, bytesWritten, totalBytes, onProgress);
}

bool ExtractArchive(const std::wstring &archivePath, const std::wstring &outputDir,
                    std::string &error)
{
    auto extPos = archivePath.find_last_of(L'.');
    std::wstring ext = (extPos == std::wstring::npos) ? L"" : archivePath.substr(extPos);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);

    std::wstring command;
    if (ext == L".zip")
    {
        command = L"powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \"Expand-Archive -Path '"
            + archivePath + L"' -DestinationPath '" + outputDir + L"' -Force\"";
    }
    else
    {
        std::wstring sevenZip = Find7Zip();
        if (sevenZip.empty())
        {
            error = AY_OBFUSCATE("7-Zip not found");
            return false;
        }
        command = L"\"" + sevenZip + L"\" x -y -o\"" + outputDir + L"\" \"" + archivePath + L"\"";
    }

    STARTUPINFOW si = {};
    PROCESS_INFORMATION pi = {};
    si.cb = sizeof(si);
    std::wstring mutableCmd = command;
    if (!CreateProcessW(nullptr, mutableCmd.data(), nullptr, nullptr, FALSE, 
                       CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
    {
        error = AY_OBFUSCATE("Failed to start extractor");
        return false;
    }
    
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    if (exitCode != 0)
    {
        error = AY_OBFUSCATE("Extract failed");
        return false;
    }
    return true;
}

bool LaunchClient(const std::wstring &clientDir, std::string &error)
{
    if (AntiFrida::IsDetected())
        AntiFrida::TriggerAntiUnpack();
    
    std::wstring exePath = Utils::JoinPath(clientDir, L"DDNet.exe");
    if (!Utils::FileExists(exePath))
    {
        error = AY_OBFUSCATE("DDNet.exe not found");
        return false;
    }
    
    HINSTANCE result = ShellExecuteW(nullptr, L"open", exePath.c_str(), 
                                      nullptr, clientDir.c_str(), SW_SHOWNORMAL);
    if (reinterpret_cast<intptr_t>(result) <= 32)
    {
        error = AY_OBFUSCATE("Failed to launch client");
        return false;
    }
    return true;
}

}