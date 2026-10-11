#include "Integrity.h"
#include "Config.h"
#include "Network.h"
#include "Utils.h"
#include "Crypto.h"
#include <json.h>
#include <shlobj.h>

namespace Integrity
{

std::string GetCurrentLoaderHash()
{
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return Crypto::ComputeFileSHA256(path);
}

CheckResult VerifyLoader(const std::string& token)
{
    CheckResult result;
    result.CurrentHash = GetCurrentLoaderHash();
    
    if (result.CurrentHash.empty())
    {
        result.Message = "Failed to compute loader hash";
        return result;
    }
    
    std::string url = std::string(Config::GetApiBase()) + "/loader/integrity?token=" + token;
    std::wstring wurl = Utils::Utf8ToWide(url);
    std::string response;
    std::string body;
    
    if (!Network::HttpRequest(wurl, "GET", body, response))
    {
        result.Message = "Network error";
        return result;
    }
    
    json_value* root = json_parse(response.c_str(), response.size());
    if (!root || root->type != json_object)
    {
        if (root) json_value_free(root);
        result.Message = "Invalid response";
        return result;
    }
    
    json_value* dataObj = nullptr;
    for (unsigned int i = 0; i < root->u.object.length; ++i)
    {
        if (strcmp(root->u.object.values[i].name, "data") == 0)
            dataObj = root->u.object.values[i].value;
    }
    
    if (!dataObj)
    {
        json_value_free(root);
        result.Message = "No data";
        return result;
    }
    
    for (unsigned int i = 0; i < dataObj->u.object.length; ++i)
    {
        const auto& entry = dataObj->u.object.values[i];
        if (strcmp(entry.name, "hash") == 0 && entry.value->type == json_string)
            result.ExpectedHash = entry.value->u.string.ptr;
        else if (strcmp(entry.name, "update_url") == 0 && entry.value->type == json_string)
            result.UpdateUrl = entry.value->u.string.ptr;
    }
    
    json_value_free(root);
    
    if (result.ExpectedHash.empty())
    {
        result.Message = "No expected hash from server";
        return result;
    }
    
    if (result.CurrentHash == result.ExpectedHash)
    {
        result.Valid = true;
        result.Message = "OK";
    }
    else
    {
        result.Valid = false;
        result.NeedsUpdate = true;
        result.Message = "Loader hash mismatch";
    }
    
    return result;
}

bool DownloadAndReplace(const std::string& url, const std::string& token)
{
    wchar_t currentPath[MAX_PATH];
    GetModuleFileNameW(nullptr, currentPath, MAX_PATH);
    
    std::wstring currentPathW = currentPath;
    std::wstring backupPath = currentPathW + L".old";
    std::wstring newPath = currentPathW + L".new";
    
    std::wstring downloadUrl = Utils::Utf8ToWide(url + "?token=" + token);
    
    long long written = 0, total = 0;
    if (!Network::HttpDownloadToFile(downloadUrl, newPath, written, total, nullptr))
        return false;
    
    if (written < 1024 * 100)
    {
        DeleteFileW(newPath.c_str());
        return false;
    }
    
    DeleteFileW(backupPath.c_str());
    
    if (!MoveFileW(currentPath, backupPath.c_str()))
    {
        DeleteFileW(newPath.c_str());
        return false;
    }
    
    if (!MoveFileW(newPath.c_str(), currentPath))
    {
        MoveFileW(backupPath.c_str(), currentPath);
        return false;
    }
    
    STARTUPINFOW si = {};
    PROCESS_INFORMATION pi = {};
    si.cb = sizeof(si);
    
    if (CreateProcessW(currentPath, nullptr, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi))
    {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    
    ExitProcess(0);
    return true;
}

}