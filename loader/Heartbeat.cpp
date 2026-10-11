#include "Heartbeat.h"
#include "Config.h"
#include "Network.h"
#include "Utils.h"
#include "Crypto.h"
#include "AntiDebug.h"
#include "antifrida.h"
#include <json.h>
#include <sstream>
#include <ctime>

namespace Heartbeat
{

static std::atomic<bool> g_Running{false};
static std::thread g_Thread;
static std::string g_LoaderHash;

static std::string GetLoaderHash()
{
    if (g_LoaderHash.empty())
    {
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        g_LoaderHash = Crypto::ComputeFileSHA256(path);
    }
    return g_LoaderHash;
}

static std::string BuildHeartbeatPayload(const std::string& token, const std::string& hwid)
{
    std::string nonce = Crypto::GenerateNonce();
    long long timestamp = static_cast<long long>(std::time(nullptr));
    std::string loaderHash = GetLoaderHash();
    
    std::ostringstream dataStream;
    dataStream << token << "|" << hwid << "|" << timestamp << "|" << nonce << "|" << loaderHash;
    std::string dataToSign = dataStream.str();
    
    std::string signature = Crypto::ComputeHMAC(hwid + token, dataToSign);
    
    std::ostringstream ss;
    ss << "{";
    ss << "\"token\":\"" << token << "\",";
    ss << "\"hwid\":\"" << hwid << "\",";
    ss << "\"timestamp\":" << timestamp << ",";
    ss << "\"nonce\":\"" << nonce << "\",";
    ss << "\"loader_hash\":\"" << loaderHash << "\",";
    ss << "\"signature\":\"" << signature << "\"";
    ss << "}";
    
    return ss.str();
}

static HeartbeatResult ParseResponse(const std::string& response)
{
    HeartbeatResult result;
    
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
        result.Message = "No data in response";
        return result;
    }
    
    bool ok = false;
    std::string action;
    std::string message;
    std::string newUrl;
    
    for (unsigned int i = 0; i < dataObj->u.object.length; ++i)
    {
        const auto& entry = dataObj->u.object.values[i];
        if (strcmp(entry.name, "ok") == 0 && entry.value->type == json_boolean)
            ok = entry.value->u.boolean != 0;
        else if (strcmp(entry.name, "action") == 0 && entry.value->type == json_string)
            action = entry.value->u.string.ptr;
        else if (strcmp(entry.name, "message") == 0 && entry.value->type == json_string)
            message = entry.value->u.string.ptr;
        else if (strcmp(entry.name, "loader_url") == 0 && entry.value->type == json_string)
            newUrl = entry.value->u.string.ptr;
    }
    
    json_value_free(root);
    
    result.Success = ok;
    result.Message = message;
    result.NewLoaderUrl = newUrl;
    
    if (action == "continue")
        result.ServerAction = Action::Continue;
    else if (action == "update_required")
        result.ServerAction = Action::UpdateRequired;
    else if (action == "terminate")
        result.ServerAction = Action::Terminate;
    
    return result;
}

HeartbeatResult SendOnce(const std::string& token, const std::string& hwid)
{
    HeartbeatResult result;
    
    if (AntiFrida::IsDetected())
    {
        result.ServerAction = Action::Terminate;
        result.Message = "Security check failed";
        return result;
    }
    
    if (!AntiDebug::IsClean())
    {
        result.ServerAction = Action::Terminate;
        result.Message = "Environment check failed";
        return result;
    }
    
    std::string payload = BuildHeartbeatPayload(token, hwid);
    std::string url = std::string(Config::GetApiBase()) + "/heartbeat";
    std::wstring wurl = Utils::Utf8ToWide(url);
    std::string response;
    
    if (!Network::HttpRequest(wurl, "POST", payload, response))
    {
        result.Message = "Network error";
        return result;
    }
    
    return ParseResponse(response);
}

static void HeartbeatLoop(std::string token, std::string hwid, 
                          std::function<void(HeartbeatResult)> onResult)
{
    while (g_Running)
    {
        HeartbeatResult result = SendOnce(token, hwid);
        
        if (onResult)
            onResult(result);
        
        if (result.ServerAction == Action::Terminate)
        {
            g_Running = false;
            break;
        }
        
        for (int i = 0; i < 50 && g_Running; i++)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

void Start(const std::string& token, const std::string& hwid, 
           std::function<void(HeartbeatResult)> onResult)
{
    if (g_Running)
        return;
    
    g_Running = true;
    g_Thread = std::thread(HeartbeatLoop, token, hwid, onResult);
}

void Stop()
{
    g_Running = false;
    if (g_Thread.joinable())
        g_Thread.join();
}

bool IsRunning()
{
    return g_Running;
}

}