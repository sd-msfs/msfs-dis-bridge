#pragma once

#include <crow.h>
#include <string>
#include <chrono>

namespace DISBridge::Utils
{

    class JsonResponse
    {
    public:
        // Success responses
        static crow::response success(const std::string &data, int status_code = 200);
        static crow::response success(crow::json::wvalue data, int status_code = 200);

        // Error responses
        static crow::response error(const std::string &message, int status_code = 500);
        static crow::response validation_error(const std::string &field, const std::string &message);
        static crow::response not_found(const std::string &resource = "Resource");
        static crow::response unauthorized(const std::string &message = "Unauthorized access");
        static crow::response rate_limited(int retry_after_seconds = 60);

        // Specialized responses
        static crow::response paginated(crow::json::wvalue data,
                                        int page, int per_page, int total_count);

        // Response builders
        struct ResponseBuilder
        {
            crow::json::wvalue data;
            int status_code = 200;

            ResponseBuilder &withTimestamp();
            ResponseBuilder &withStatus(int code);
            ResponseBuilder &withData(const std::string &key, crow::json::wvalue value);
            ResponseBuilder &withMessage(const std::string &message);
            ResponseBuilder &withError(const std::string &error);
            ResponseBuilder &withMetadata(const std::string &key, const std::string &value);

            crow::response build();
        };

        static ResponseBuilder builder();

    private:
        static crow::json::wvalue createBaseResponse(bool success, int status_code);
        static std::string getCurrentTimestamp();
        static crow::response createResponse(const crow::json::wvalue &json, int status_code);

        // CORS headers for web dashboard compatibility
        static void addCorsHeaders(crow::response &response);
    };

}