#pragma once

#include <string>
#include <functional>
#include <atomic>
#include <thread>

namespace Heartbeat
{
    enum class Action
    {
        Continue,
        UpdateRequired,
        Terminate
    };
    
    struct HeartbeatResult
    {
        bool Success = false;
        Action ServerAction = Action::Continue;
        std::string Message;
        std::string NewLoaderUrl;
    };
    
    void Start(const std::string& token, const std::string& hwid, 
               std::function<void(HeartbeatResult)> onResult);
    void Stop();
    bool IsRunning();
    HeartbeatResult SendOnce(const std::string& token, const std::string& hwid);
}