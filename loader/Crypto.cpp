#include "Crypto.h"
#include "AntiDebug.h"
#include <wincrypt.h>
#include <bcrypt.h>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "crypt32.lib")

namespace Crypto
{

static std::string BytesToHex(const BYTE* data, size_t len)
{
    std::ostringstream ss;
    ss << std::hex << std::setfill('0');
    for (size_t i = 0; i < len; i++)
        ss << std::setw(2) << static_cast<int>(data[i]);
    return ss.str();
}

std::string ComputeHMAC(const std::string& key, const std::string& data)
{
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    std::string result;
    
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 
                                     BCRYPT_ALG_HANDLE_HMAC_FLAG) != 0)
        return "";
    
    DWORD hashLen = 0, resultLen = 0;
    BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PUCHAR)&hashLen, sizeof(hashLen), &resultLen, 0);
    
    std::vector<BYTE> hashValue(hashLen);
    
    if (BCryptCreateHash(hAlg, &hHash, nullptr, 0, 
                         (PUCHAR)key.data(), (ULONG)key.size(), 0) == 0)
    {
        if (BCryptHashData(hHash, (PUCHAR)data.data(), (ULONG)data.size(), 0) == 0)
        {
            if (BCryptFinishHash(hHash, hashValue.data(), hashLen, 0) == 0)
            {
                result = BytesToHex(hashValue.data(), hashLen);
            }
        }
        BCryptDestroyHash(hHash);
    }
    
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return result;
}

std::string Base64Decode(const std::string& input)
{
    DWORD outLen = 0;
    CryptStringToBinaryA(input.c_str(), (DWORD)input.size(), 
                         CRYPT_STRING_BASE64, nullptr, &outLen, nullptr, nullptr);
    
    std::vector<BYTE> buffer(outLen);
    CryptStringToBinaryA(input.c_str(), (DWORD)input.size(), 
                         CRYPT_STRING_BASE64, buffer.data(), &outLen, nullptr, nullptr);
    
    return std::string(buffer.begin(), buffer.end());
}

std::string Base64Encode(const std::vector<BYTE>& input)
{
    DWORD outLen = 0;
    CryptBinaryToStringA(input.data(), (DWORD)input.size(), 
                         CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &outLen);
    
    std::vector<char> buffer(outLen);
    CryptBinaryToStringA(input.data(), (DWORD)input.size(), 
                         CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, buffer.data(), &outLen);
    
    return std::string(buffer.data());
}

std::string SolveChallenge(const std::string& challengeBase64, const std::string& challengeSecret)
{
    std::string decoded = Base64Decode(challengeBase64);
    std::vector<BYTE> data(decoded.begin(), decoded.end());
    
    std::string secret(challengeSecret);
    data.insert(data.end(), secret.begin(), secret.end());
    
    AntiDebug::ContributeToKey(data.data(), data.size() > 16 ? 16 : data.size());
    
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    std::string result;
    
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0)
        return "";
    
    DWORD hashLen = 32;
    std::vector<BYTE> hashValue(hashLen);
    
    if (BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0) == 0)
    {
        if (BCryptHashData(hHash, data.data(), (ULONG)data.size(), 0) == 0)
        {
            if (BCryptFinishHash(hHash, hashValue.data(), hashLen, 0) == 0)
            {
                result = BytesToHex(hashValue.data(), hashLen);
            }
        }
        BCryptDestroyHash(hHash);
    }
    
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return result;
}

std::vector<BYTE> DecryptAES256GCM(const std::vector<BYTE>& key, const std::vector<BYTE>& data)
{
    if (data.size() < 28)
        return {};
    
    std::vector<BYTE> modifiedKey = key;
    AntiDebug::ContributeToKey(modifiedKey.data(), modifiedKey.size());
    
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;
    std::vector<BYTE> result;
    
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0) != 0)
        return {};
    
    if (BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_GCM, 
                          sizeof(BCRYPT_CHAIN_MODE_GCM), 0) != 0)
    {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return {};
    }
    
    if (BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0, 
                                    modifiedKey.data(), (ULONG)modifiedKey.size(), 0) != 0)
    {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return {};
    }
    
    std::vector<BYTE> iv(data.begin(), data.begin() + 12);
    std::vector<BYTE> tag(data.begin() + 12, data.begin() + 28);
    std::vector<BYTE> ciphertext(data.begin() + 28, data.end());
    
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
    BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
    authInfo.pbNonce = iv.data();
    authInfo.cbNonce = 12;
    authInfo.pbTag = tag.data();
    authInfo.cbTag = 16;
    
    DWORD outLen = 0;
    result.resize(ciphertext.size());
    
    NTSTATUS status = BCryptDecrypt(hKey, ciphertext.data(), (ULONG)ciphertext.size(),
                                     &authInfo, nullptr, 0, result.data(), 
                                     (ULONG)result.size(), &outLen, 0);
    
    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    
    if (status != 0)
        return {};
    
    result.resize(outLen);
    return result;
}

std::string ComputeSHA256(const std::vector<BYTE>& data)
{
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    std::string result;
    
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0)
        return "";
    
    DWORD hashLen = 32;
    std::vector<BYTE> hashValue(hashLen);
    
    if (BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0) == 0)
    {
        if (BCryptHashData(hHash, (PUCHAR)data.data(), (ULONG)data.size(), 0) == 0)
        {
            if (BCryptFinishHash(hHash, hashValue.data(), hashLen, 0) == 0)
            {
                result = BytesToHex(hashValue.data(), hashLen);
            }
        }
        BCryptDestroyHash(hHash);
    }
    
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return result;
}

std::string ComputeSHA256(const std::string& data)
{
    std::vector<BYTE> bytes(data.begin(), data.end());
    return ComputeSHA256(bytes);
}

std::string ComputeFileSHA256(const std::wstring& filePath)
{
    HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    
    if (hFile == INVALID_HANDLE_VALUE)
        return "";
    
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    std::string result;
    
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0)
    {
        CloseHandle(hFile);
        return "";
    }
    
    if (BCryptCreateHash(hAlg, &hHash, nullptr, 0, nullptr, 0, 0) != 0)
    {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        CloseHandle(hFile);
        return "";
    }
    
    BYTE buffer[65536];
    DWORD bytesRead = 0;
    
    while (ReadFile(hFile, buffer, sizeof(buffer), &bytesRead, nullptr) && bytesRead > 0)
    {
        if (BCryptHashData(hHash, buffer, bytesRead, 0) != 0)
        {
            BCryptDestroyHash(hHash);
            BCryptCloseAlgorithmProvider(hAlg, 0);
            CloseHandle(hFile);
            return "";
        }
    }
    
    DWORD hashLen = 32;
    std::vector<BYTE> hashValue(hashLen);
    
    if (BCryptFinishHash(hHash, hashValue.data(), hashLen, 0) == 0)
    {
        result = BytesToHex(hashValue.data(), hashLen);
    }
    
    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    CloseHandle(hFile);
    
    return result;
}

std::vector<BYTE> EncryptAES256GCM(const std::vector<BYTE>& key, const std::vector<BYTE>& data)
{
    if (key.size() != 32)
        return {};
    
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_KEY_HANDLE hKey = nullptr;
    std::vector<BYTE> result;
    
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, nullptr, 0) != 0)
        return {};
    
    if (BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_GCM, 
                          sizeof(BCRYPT_CHAIN_MODE_GCM), 0) != 0)
    {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return {};
    }
    
    if (BCryptGenerateSymmetricKey(hAlg, &hKey, nullptr, 0, 
                                    (PUCHAR)key.data(), (ULONG)key.size(), 0) != 0)
    {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return {};
    }
    
    std::vector<BYTE> iv(12);
    BCryptGenRandom(nullptr, iv.data(), 12, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    
    std::vector<BYTE> tag(16);
    
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
    BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
    authInfo.pbNonce = iv.data();
    authInfo.cbNonce = 12;
    authInfo.pbTag = tag.data();
    authInfo.cbTag = 16;
    
    DWORD outLen = 0;
    std::vector<BYTE> ciphertext(data.size() + 16);
    
    NTSTATUS status = BCryptEncrypt(hKey, (PUCHAR)data.data(), (ULONG)data.size(),
                                     &authInfo, nullptr, 0, ciphertext.data(), 
                                     (ULONG)ciphertext.size(), &outLen, 0);
    
    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    
    if (status != 0)
        return {};
    
    ciphertext.resize(outLen);
    
    result.reserve(12 + 16 + ciphertext.size());
    result.insert(result.end(), iv.begin(), iv.end());
    result.insert(result.end(), tag.begin(), tag.end());
    result.insert(result.end(), ciphertext.begin(), ciphertext.end());
    
    return result;
}

std::string GenerateNonce()
{
    std::vector<BYTE> bytes(16);
    BCryptGenRandom(nullptr, bytes.data(), 16, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    return BytesToHex(bytes.data(), bytes.size());
}

}