#pragma once

#include <string>
#include <functional>

namespace Network
{
    bool IsProxyDetected();
    bool HasSnifferProcesses();
    bool IsNetworkSafe();
    
    bool HttpRequest(const std::wstring& url, const std::string& method,
                     const std::string& body, std::string& response);
    
    bool HttpDownloadToFile(const std::wstring& url, const std::wstring& outputPath,
                            long long& bytesWritten, long long& totalBytes,
                            std::function<void(long long)> onProgress = nullptr);
    
    bool DownloadFile(const std::wstring& url, const std::wstring& outputPath);
}