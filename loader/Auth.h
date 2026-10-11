#pragma once

#include <string>

namespace Auth
{
    struct AuthState
    {
        bool HasValidSession = false;
        bool SubscriptionActive = false;
        std::string Username;
        std::string Token;
        std::string SubscriptionUntil;
        std::string Status;
    };

    AuthState ValidateToken(const std::string &token);

    AuthState Login(const std::string &username, const std::string &password, 
                    const std::string &hwid);

    bool WriteSessionFile(const std::string &token, const std::string &subscriptionUntil);
}