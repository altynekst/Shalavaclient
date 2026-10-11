#include "Config.h"
#include "Utils.h"
#include "obfuscate.h"
#include <json.h>
#include <sstream>

namespace Config
{

const char* GetApiBase() 
{ 
    return AY_OBFUSCATE("https://api.shalawa.xyz/api/login"); 
}

const wchar_t* GetClientArchive() 
{ 
    return L"Shalawa Client.zip";
}

const char* GetArchiveEndpoint() { return AY_OBFUSCATE("/download"); }
const char* GetVersionEndpoint() { return AY_OBFUSCATE("/version"); }
const char* GetSessionFileName() { return AY_OBFUSCATE("session.json"); }
const char* GetConfigFileName() { return AY_OBFUSCATE("loader_config.json"); }

bool Load(LoaderConfig &config)
{
    std::wstring path = Utils::JoinPath(Utils::GetAppDataPath(), 
                                         Utils::Utf8ToWide(GetConfigFileName()));
    std::string data = Utils::ReadTextFile(path);
    if (data.empty())
        return false;
        
    json_value *root = json_parse(data.c_str(), data.size());
    if (!root || root->type != json_object)
    {
        if (root)
            json_value_free(root);
        return false;
    }
    
    for (unsigned int i = 0; i < root->u.object.length; ++i)
    {
        const auto &entry = root->u.object.values[i];
        if (strcmp(entry.name, "remember") == 0 && entry.value->type == json_boolean)
            config.Remember = entry.value->u.boolean;
        if (strcmp(entry.name, "token") == 0 && entry.value->type == json_string)
            config.Token = entry.value->u.string.ptr;
        if (strcmp(entry.name, "username") == 0 && entry.value->type == json_string)
            config.Username = entry.value->u.string.ptr;
        if (strcmp(entry.name, "last_version") == 0 && entry.value->type == json_string)
            config.LastVersion = entry.value->u.string.ptr;
    }
    
    json_value_free(root);
    return true;
}

void Save(const LoaderConfig &config)
{
    std::ostringstream ss;
    ss << "{";
    ss << "\"remember\":" << (config.Remember ? "true" : "false") << ",";
    ss << "\"token\":\"" << config.Token << "\",";
    ss << "\"username\":\"" << config.Username << "\",";
    ss << "\"last_version\":\"" << config.LastVersion << "\"";
    ss << "}";
    std::wstring path = Utils::JoinPath(Utils::GetAppDataPath(), 
                                         Utils::Utf8ToWide(GetConfigFileName()));
    Utils::WriteTextFile(path, ss.str());
}

}