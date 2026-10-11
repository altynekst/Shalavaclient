#pragma once

#include <string>

namespace Integrity
{
    struct CheckResult
    {
        bool Valid = false;
        bool NeedsUpdate = false;
        std::string CurrentHash;
        std::string ExpectedHash;
        std::string UpdateUrl;
        std::string Message;
    };
    
    CheckResult VerifyLoader(const std::string& token);
    bool DownloadAndReplace(const std::string& url, const std::string& token);
    std::string GetCurrentLoaderHash();
}