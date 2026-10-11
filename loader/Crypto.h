#pragma once

#include <windows.h>
#include <string>
#include <vector>

namespace Crypto
{
    std::string ComputeHMAC(const std::string& key, const std::string& data);
    std::string Base64Decode(const std::string& input);
    std::string Base64Encode(const std::vector<BYTE>& input);
    std::string SolveChallenge(const std::string& challengeBase64, const std::string& challengeSecret);
    std::vector<BYTE> DecryptAES256GCM(const std::vector<BYTE>& key, const std::vector<BYTE>& data);
    std::vector<BYTE> EncryptAES256GCM(const std::vector<BYTE>& key, const std::vector<BYTE>& data);
    std::string ComputeSHA256(const std::vector<BYTE>& data);
    std::string ComputeSHA256(const std::string& data);
    std::string ComputeFileSHA256(const std::wstring& filePath);
    std::string GenerateNonce();
}