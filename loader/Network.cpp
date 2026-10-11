#include "Network.h"
#include "Utils.h"
#include "antifrida.h"
#include "obfuscate.h"
#include <winhttp.h>
#include <wincrypt.h>
#include "AntiDebug.h"
#include <vector>
#include <tlhelp32.h>
#include <algorithm>
#include <random>
#include <chrono>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "crypt32.lib")

namespace Network
{

bool IsProxyDetected()
{
    WINHTTP_CURRENT_USER_IE_PROXY_CONFIG proxyConfig = {};
    if (WinHttpGetIEProxyConfigForCurrentUser(&proxyConfig))
    {
        bool hasProxy = false;
        
        if (proxyConfig.lpszProxy != nullptr && wcslen(proxyConfig.lpszProxy) > 0)
            hasProxy = true;
        
        if (proxyConfig.lpszProxy)
            GlobalFree(proxyConfig.lpszProxy);
        if (proxyConfig.lpszProxyBypass)
            GlobalFree(proxyConfig.lpszProxyBypass);
        if (proxyConfig.lpszAutoConfigUrl)
            GlobalFree(proxyConfig.lpszAutoConfigUrl);
        
        if (hasProxy)
            return true;
    }

    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, 
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings",
        0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        DWORD proxyEnable = 0;
        DWORD size = sizeof(proxyEnable);
        if (RegQueryValueExW(hKey, L"ProxyEnable", nullptr, nullptr, 
            (LPBYTE)&proxyEnable, &size) == ERROR_SUCCESS)
        {
            if (proxyEnable != 0)
            {
                RegCloseKey(hKey);
                return true;
            }
        }
        RegCloseKey(hKey);
    }

    const wchar_t* proxyEnvVars[] = {
        L"HTTP_PROXY", L"HTTPS_PROXY", L"http_proxy", L"https_proxy"
    };
    
    for (int i = 0; i < 4; i++)
    {
        wchar_t buffer[512] = {};
        if (GetEnvironmentVariableW(proxyEnvVars[i], buffer, 512) > 0)
            return true;
    }
    
    return false;
}

bool HasSnifferProcesses()
{
    const char* sniffers[] = {
        "fiddler",
        "wireshark",
        "charles",
        "burp",
        "mitmproxy",
        "proxifier",
        "httpdebuggerpro",
        "httpdebugger",
        "telerik",
        "reqable",
        "proxyman",
    };
    
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE)
        return false;

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);

    if (Process32FirstW(hSnapshot, &pe))
    {
        do
        {
            char procName[MAX_PATH];
            WideCharToMultiByte(CP_UTF8, 0, pe.szExeFile, -1, 
                procName, MAX_PATH, nullptr, nullptr);
            
            for (int i = 0; procName[i] != '\0'; i++)
            {
                if (procName[i] >= 'A' && procName[i] <= 'Z')
                    procName[i] = procName[i] + 32;
            }

            for (int i = 0; i < 11; i++)
            {
                if (strstr(procName, sniffers[i]) != nullptr)
                {
                    CloseHandle(hSnapshot);
                    return true;
                }
            }
        } while (Process32NextW(hSnapshot, &pe));
    }

    CloseHandle(hSnapshot);
    return false;
}

static bool CheckCertificate(HINTERNET hRequest)
{
    DWORD certInfoSize = sizeof(WINHTTP_CERTIFICATE_INFO);
    WINHTTP_CERTIFICATE_INFO certInfo = {};
    
    if (!WinHttpQueryOption(hRequest, WINHTTP_OPTION_SECURITY_CERTIFICATE_STRUCT, 
        &certInfo, &certInfoSize))
    {
        return true;
    }
    
    bool valid = true;
    
    if (certInfo.lpszSubjectInfo)
    {
        std::wstring subject(certInfo.lpszSubjectInfo);
        std::transform(subject.begin(), subject.end(), subject.begin(), ::towlower);
        
        const wchar_t* badSubjects[] = {
            L"fiddler", L"charles", L"burp", L"mitmproxy", 
            L"do_not_trust", L"proxy", L"debug"
        };
        
        for (const wchar_t* bad : badSubjects)
        {
            if (subject.find(bad) != std::wstring::npos)
            {
                valid = false;
                break;
            }
        }
        LocalFree(certInfo.lpszSubjectInfo);
    }
    
    if (certInfo.lpszIssuerInfo)
    {
        std::wstring issuer(certInfo.lpszIssuerInfo);
        std::transform(issuer.begin(), issuer.end(), issuer.begin(), ::towlower);
        
        const wchar_t* badIssuers[] = {
            L"fiddler", L"charles", L"burp", L"mitmproxy", 
            L"do_not_trust", L"debug"
        };
        
        for (const wchar_t* bad : badIssuers)
        {
            if (issuer.find(bad) != std::wstring::npos)
            {
                valid = false;
                break;
            }
        }
        LocalFree(certInfo.lpszIssuerInfo);
    }
    
    return valid;
}

bool IsNetworkSafe()
{
    if (AntiFrida::IsDetected())
        return false;
    
    if (IsProxyDetected())
        return false;
    
    if (HasSnifferProcesses())
        return false;
    
    return true;
}

static void AddAntiCacheHeaders(HINTERNET hRequest)
{
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    std::mt19937_64 rng(static_cast<unsigned>(now));
    
    wchar_t nonce[64];
    swprintf_s(nonce, L"X-Request-Nonce: %llu\r\n", rng());
    WinHttpAddRequestHeaders(hRequest, nonce, (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);
    
    wchar_t timestamp[64];
    swprintf_s(timestamp, L"X-Request-Time: %lld\r\n", 
        std::chrono::system_clock::now().time_since_epoch().count());
    WinHttpAddRequestHeaders(hRequest, timestamp, (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);
    
    WinHttpAddRequestHeaders(hRequest, 
        L"Cache-Control: no-cache, no-store, must-revalidate\r\n", 
        (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);
    WinHttpAddRequestHeaders(hRequest, 
        L"Pragma: no-cache\r\n", 
        (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);
}

static std::wstring GetRandomUserAgent()
{
    const wchar_t* agents[] = {
        L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36",
        L"Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:109.0) Gecko/20100101",
        L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Chrome/120.0.0.0",
    };
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    std::mt19937_64 rng(static_cast<unsigned>(now));
    return agents[rng() % 3];
}

bool HttpRequest(const std::wstring &url, const std::string &method, 
                 const std::string &body, std::string &response)
{
    if (!IsNetworkSafe())
    {
        AntiFrida::TriggerAntiUnpack();
        return false;
    }
    if (!AntiDebug::IsClean())
    {
        AntiFrida::TriggerAntiUnpack();
        return false;
    }

    URL_COMPONENTS components = {};
    components.dwStructSize = sizeof(components);
    components.dwSchemeLength = static_cast<DWORD>(-1);
    components.dwHostNameLength = static_cast<DWORD>(-1);
    components.dwUrlPathLength = static_cast<DWORD>(-1);
    components.dwExtraInfoLength = static_cast<DWORD>(-1);

    std::wstring urlCopy = url;
    if (!WinHttpCrackUrl(urlCopy.data(), 0, 0, &components))
        return false;

    std::wstring host(components.lpszHostName, components.dwHostNameLength);
    std::wstring path(components.lpszUrlPath, components.dwUrlPathLength);
    std::wstring extra(components.lpszExtraInfo, components.dwExtraInfoLength);
    std::wstring fullPath = path + extra;

    std::wstring userAgent = GetRandomUserAgent();

    HINTERNET hSession = WinHttpOpen(userAgent.c_str(), 
        WINHTTP_ACCESS_TYPE_NO_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession)
        return false;

    DWORD timeout = 30000;
    WinHttpSetOption(hSession, WINHTTP_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
    WinHttpSetOption(hSession, WINHTTP_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));
    WinHttpSetOption(hSession, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), components.nPort, 0);
    if (!hConnect)
    {
        WinHttpCloseHandle(hSession);
        return false;
    }

    DWORD flags = (components.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    std::wstring methodW = Utils::Utf8ToWide(method);
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, methodW.c_str(), fullPath.c_str(),
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest)
    {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    AddAntiCacheHeaders(hRequest);

    std::wstring headers = L"Content-Type: application/json\r\n";
    headers += L"Accept: application/json\r\n";
    headers += L"Accept-Encoding: identity\r\n";
    
    BOOL sent = WinHttpSendRequest(hRequest, headers.c_str(), static_cast<DWORD>(headers.size()),
        (LPVOID)body.data(), static_cast<DWORD>(body.size()), static_cast<DWORD>(body.size()), 0);
    
    if (!sent || !WinHttpReceiveResponse(hRequest, nullptr))
    {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, 
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeSize, 
        WINHTTP_NO_HEADER_INDEX);
    
    if (statusCode >= 400)
    {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }
    
    if (components.nScheme == INTERNET_SCHEME_HTTPS)
    {
        if (!CheckCertificate(hRequest))
        {
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            AntiFrida::TriggerAntiUnpack();
            return false;
        }
    }

    response.clear();
    DWORD size = 0;
    do
    {
        DWORD downloaded = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &size))
            break;
        if (size == 0)
            break;
        std::vector<char> buffer(size);
        if (!WinHttpReadData(hRequest, buffer.data(), size, &downloaded))
            break;
        response.append(buffer.data(), buffer.data() + downloaded);
    } while (size > 0);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    
    return !response.empty();
}

bool HttpDownloadToFile(const std::wstring &url, const std::wstring &outputPath,
                        long long &bytesWritten, long long &totalBytes,
                        std::function<void(long long)> onProgress)
{
    if (!IsNetworkSafe())
    {
        AntiFrida::TriggerAntiUnpack();
        return false;
    }

    URL_COMPONENTS components = {};
    components.dwStructSize = sizeof(components);
    components.dwSchemeLength = static_cast<DWORD>(-1);
    components.dwHostNameLength = static_cast<DWORD>(-1);
    components.dwUrlPathLength = static_cast<DWORD>(-1);
    components.dwExtraInfoLength = static_cast<DWORD>(-1);

    if (!AntiDebug::IsClean())
    {
        AntiFrida::TriggerAntiUnpack();
        return false;
    }

    std::wstring urlCopy = url;
    if (!WinHttpCrackUrl(urlCopy.data(), 0, 0, &components))
        return false;

    std::wstring host(components.lpszHostName, components.dwHostNameLength);
    std::wstring path(components.lpszUrlPath, components.dwUrlPathLength);
    std::wstring extra(components.lpszExtraInfo, components.dwExtraInfoLength);
    std::wstring fullPath = path + extra;

    std::wstring userAgent = GetRandomUserAgent();

    HINTERNET hSession = WinHttpOpen(userAgent.c_str(), 
        WINHTTP_ACCESS_TYPE_NO_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession)
        return false;

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), components.nPort, 0);
    if (!hConnect)
    {
        WinHttpCloseHandle(hSession);
        return false;
    }

    DWORD flags = (components.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", fullPath.c_str(),
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest)
    {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    AddAntiCacheHeaders(hRequest);

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, nullptr, 0, 0, 0) ||
       !WinHttpReceiveResponse(hRequest, nullptr))
    {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, 
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusCodeSize, 
        WINHTTP_NO_HEADER_INDEX);
    
    if (statusCode >= 400)
    {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    if (components.nScheme == INTERNET_SCHEME_HTTPS)
    {
        if (!CheckCertificate(hRequest))
        {
            WinHttpCloseHandle(hRequest);
            WinHttpCloseHandle(hConnect);
            WinHttpCloseHandle(hSession);
            AntiFrida::TriggerAntiUnpack();
            return false;
        }
    }

    totalBytes = 0;
    DWORD headerSize = sizeof(totalBytes);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &totalBytes, &headerSize, WINHTTP_NO_HEADER_INDEX);

    if (!Utils::CreateDirsForFile(outputPath))
    {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    HANDLE file = CreateFileW(outputPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, 
                              nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    bytesWritten = 0;
    DWORD size = 0;
    do
    {
        DWORD downloaded = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &size))
            break;
        if (size == 0)
            break;
        std::vector<char> buffer(size);
        if (!WinHttpReadData(hRequest, buffer.data(), size, &downloaded))
            break;
        DWORD written = 0;
        if (!WriteFile(file, buffer.data(), downloaded, &written, nullptr))
            break;
        bytesWritten += written;
        if (onProgress)
            onProgress(bytesWritten);
    } while (size > 0);

    CloseHandle(file);
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return true;
}

bool DownloadFile(const std::wstring &url, const std::wstring &outputPath)
{
    std::string response;
    if (!HttpRequest(url, "GET", "", response))
        return false;
    return Utils::WriteTextFile(outputPath, response);
}

}