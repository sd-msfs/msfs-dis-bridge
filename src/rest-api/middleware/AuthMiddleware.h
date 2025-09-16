#pragma once

#include <crow.h>
#include <string>
#include <unordered_set>
#include <chrono>
#include <mutex>

namespace DISBridge::Middleware
{

    // Simple JWT-like token for API authentication
    struct AuthToken
    {
        std::string token;
        std::string user_id;
        std::chrono::steady_clock::time_point created_at;
        std::chrono::steady_clock::time_point expires_at;
        std::unordered_set<std::string> permissions;

        bool isValid() const
        {
            return std::chrono::steady_clock::now() < expires_at;
        }

        bool hasPermission(const std::string &permission) const
        {
            return permissions.find(permission) != permissions.end() ||
                   permissions.find("*") != permissions.end();
        }
    };

    class AuthMiddleware
    {
    public:
        AuthMiddleware();

        // Middleware structure for Crow
        struct context
        {
            std::string user_id;
            std::unordered_set<std::string> permissions;
            bool is_authenticated = false;
        };

        void before_handle(crow::request &req, crow::response &res, context &ctx);
        void after_handle(crow::request &req, crow::response &res, context &ctx);

        // Token management
        std::string generateToken(const std::string &user_id,
                                  const std::unordered_set<std::string> &permissions,
                                  std::chrono::hours duration = std::chrono::hours(24));

        bool validateToken(const std::string &token, AuthToken &out_token) const;
        void revokeToken(const std::string &token);
        void cleanupExpiredTokens();

        // Configuration
        void setSecretKey(const std::string &key) { secret_key_ = key; }
        void addPublicEndpoint(const std::string &endpoint) { public_endpoints_.insert(endpoint); }
        void setRequiredPermission(const std::string &endpoint, const std::string &permission);

    private:
        std::string secret_key_;
        mutable std::mutex tokens_mutex_;
        std::unordered_map<std::string, AuthToken> active_tokens_;
        std::unordered_set<std::string> public_endpoints_;
        std::unordered_map<std::string, std::string> endpoint_permissions_;

        // Helper methods
        std::string extractTokenFromRequest(const crow::request &req) const;
        bool isPublicEndpoint(const std::string &path) const;
        std::string getRequiredPermission(const std::string &path) const;
        std::string hashToken(const std::string &token) const;

        // Rate limiting (simple implementation)
        struct RateLimitInfo
        {
            std::chrono::steady_clock::time_point last_request;
            int request_count;
        };
        mutable std::mutex rate_limit_mutex_;
        std::unordered_map<std::string, RateLimitInfo> rate_limits_;
        static constexpr int MAX_REQUESTS_PER_MINUTE = 100;

        bool checkRateLimit(const std::string &client_id);
    };

}   