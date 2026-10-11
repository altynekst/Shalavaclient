#pragma once
#include <string>

namespace Config
{
    const char* GetApiBase();
    const wchar_t* GetClientArchive();
    const char* GetArchiveEndpoint();
    const char* GetVersionEndpoint();
    const char* GetSessionFileName();
    const char* GetConfigFileName();

    struct LoaderConfig
    {
        bool Remember = false;
        std::string Token;
        std::string Username;
        std::string LastVersion;
    };

    bool Load(LoaderConfig& config);
    void Save(const LoaderConfig& config);
}