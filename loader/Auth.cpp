#include "Auth.h"
#include "Config.h"
#include "Network.h"
#include "Utils.h"
#include "Crypto.h"
#include "AntiDebug.h"
#include "antifrida.h"
#include "obfuscate.h"
#include <json.h>
#include <sstream>
#include <ctime>

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

namespace Auth
{

static bool GetChallenge(std::string& challengeId, std::string& challengeData)
{
    std::string url = std::string(Config::GetApiBase()) + "/challenge";
    std::wstring wurl = Utils::Utf8ToWide(url);
    std::string response;
    
    if (!Network::HttpRequest(wurl, "POST", "{}", response))
        return false;
    
    json_value* root = json_parse(response.c_str(), response.size());
    if (!root)
        return false;
    
    json_value* dataObj = nullptr;
    for (unsigned int i = 0; i < root->u.object.length; ++i)
    {
        if (strcmp(root->u.object.values[i].name, "data") == 0)
            dataObj = root->u.object.values[i].value;
    }
    
    if (!dataObj)
    {
        json_value_free(root);
        return false;
    }
    
    challengeId = ReadJsonString(dataObj, "challenge_id");
    challengeData = ReadJsonString(dataObj, "challenge");
    
    json_value_free(root);
    return !challengeId.empty() && !challengeData.empty();
}


AuthState ValidateToken(const std::string &token)
{
    AuthState state;
    state.Token = token;
    
    if (AntiFrida::IsDetected())
        AntiFrida::TriggerAntiUnpack();
    
    if (token.empty())
        return state;
    
    std::string url = std::string(Config::GetApiBase()) + "/me?token=" + token;
    std::wstring wurl = Utils::Utf8ToWide(url);
    std::string response;
    std::string body;
    
    if (!Network::HttpRequest(wurl, "GET", body, response))
    {
        state.Status = AY_OBFUSCATE("Server unavailable");
        return state;
    }
    
    json_value *root = json_parse(response.c_str(), response.size());
    if (!root || root->type != json_object)
    {
        if (root)
            json_value_free(root);
        state.Status = AY_OBFUSCATE("Invalid response");
        return state;
    }
    
    json_value *userObj = nullptr;
    for (unsigned int i = 0; i < root->u.object.length; ++i)
    {
        if (strcmp(root->u.object.values[i].name, "user") == 0)
            userObj = root->u.object.values[i].value;
    }
    
    if (userObj && userObj->type == json_object)
    {
        state.Username = ReadJsonString(userObj, "username");
        state.SubscriptionActive = ReadJsonBool(userObj, "subscription_active");
        state.SubscriptionUntil = ReadJsonString(userObj, "subscription_until");
        state.HasValidSession = true;
    }
    
    json_value_free(root);
    return state;
}

AuthState Login(const std::string& username, const std::string& password, const std::string& hwid)
{
    AuthState state;
    
    AntiDebug::PerformAdvancedAntiDebug();
    
    if (AntiFrida::IsDetected())
    {
        AntiFrida::TriggerAntiUnpack();
        state.Status = "security_check_failed";
        return state;
    }
    
    std::string challengeId, challengeData;
    if (!GetChallenge(challengeId, challengeData))
    {
        state.Status = AY_OBFUSCATE("challenge_failed");
        return state;
    }
    
    std::string challengeSecret = AntiDebug::GetDynamicChallengeSecret();
    std::string challengeResponse = Crypto::SolveChallenge(challengeData, challengeSecret);
    if (challengeResponse.empty())
    {
        state.Status = AY_OBFUSCATE("challenge_solve_failed");
        return state;
    }
    
    std::ostringstream ss;
    ss << "{";
    ss << "\"username\":\"" << username << "\",";
    ss << "\"password\":\"" << password << "\",";
    ss << "\"hwid\":\"" << hwid << "\",";
    ss << "\"client\":\"loader\",";
    ss << "\"challenge_id\":\"" << challengeId << "\",";
    ss << "\"challenge_response\":\"" << challengeResponse << "\"";
    ss << "}";
    
    std::string url = std::string(Config::GetApiBase()) + "/login";
    std::wstring wurl = Utils::Utf8ToWide(url);
    std::string response;
    
    if (!Network::HttpRequest(wurl, "POST", ss.str(), response))
    {
        state.Status = AY_OBFUSCATE("Server unavailable");
        return state;
    }
    
    
    json_value* root = json_parse(response.c_str(), response.size());
    if (!root || root->type != json_object)
    {
        if (root)
            json_value_free(root);
        state.Status = AY_OBFUSCATE("Invalid response");
        return state;
    }
    
    json_value* dataObj = nullptr;
    std::string signature;
    int timestamp = 0;
    std::string nonce;
    
    for (unsigned int i = 0; i < root->u.object.length; ++i)
    {
        const auto& entry = root->u.object.values[i];
        if (strcmp(entry.name, "data") == 0)
            dataObj = entry.value;
        else if (strcmp(entry.name, "signature") == 0 && entry.value->type == json_string)
            signature = entry.value->u.string.ptr;
        else if (strcmp(entry.name, "timestamp") == 0 && entry.value->type == json_integer)
            timestamp = (int)entry.value->u.integer;
        else if (strcmp(entry.name, "nonce") == 0 && entry.value->type == json_string)
            nonce = entry.value->u.string.ptr;
    }
    
    if (!dataObj)
    {
        json_value_free(root);
        state.Status = AY_OBFUSCATE("Invalid response structure");
        return state;
    }
    
    if (!ReadJsonBool(dataObj, "ok"))
    {
        std::string error = ReadJsonString(dataObj, "error");
        state.Status = error.empty() ? std::string(AY_OBFUSCATE("Login failed")) : error;
        json_value_free(root);
        return state;
    }
    
    state.Token = ReadJsonString(dataObj, "token");
    
    json_value* userObj = nullptr;
    for (unsigned int i = 0; i < dataObj->u.object.length; ++i)
    {
        if (strcmp(dataObj->u.object.values[i].name, "user") == 0)
            userObj = dataObj->u.object.values[i].value;
    }
    
    if (userObj && userObj->type == json_object)
    {
        state.Username = ReadJsonString(userObj, "username");
        state.SubscriptionActive = ReadJsonBool(userObj, "subscription_active");
        state.SubscriptionUntil = ReadJsonString(userObj, "subscription_until");
        state.HasValidSession = true;
    }
    
    json_value_free(root);
    return state;
}

bool WriteSessionFile(const std::string &token, const std::string &subscriptionUntil)
{
    std::ostringstream ss;
    std::string now = std::to_string(static_cast<long long>(::time(nullptr)));
    ss << "{";
    ss << "\"token\":\"" << token << "\",";
    ss << "\"issued_at\":" << now << ",";
    ss << "\"subscription_until\":\"" << subscriptionUntil << "\",";
    ss << "\"server_url\":\"" << Config::GetApiBase() << "\"";
    ss << "}";
    std::wstring path = Utils::JoinPath(Utils::GetAppDataPath(), 
                                         Utils::Utf8ToWide(Config::GetSessionFileName()));
    return Utils::WriteTextFile(path, ss.str());
}

}