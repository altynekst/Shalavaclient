#pragma once

#include <string>
#include <functional>

namespace Client
{
    std::string FetchArchiveVersion(const std::string &token);

    bool DownloadClientArchive(const std::string &token, const std::wstring &outputPath,
                               long long &bytesWritten, long long &totalBytes,
                               std::function<void(long long)> onProgress = nullptr);

    bool ExtractArchive(const std::wstring &archivePath, const std::wstring &outputDir,
                        std::string &error);

    bool LaunchClient(const std::wstring &clientDir, std::string &error);

    std::wstring Find7Zip();
}