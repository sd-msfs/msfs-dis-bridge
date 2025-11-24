#include "JsonResponse.h"
#include <chrono>
#include <iomanip>
#include <sstream>

namespace DISBridge::Utils
{

    // ========================================================================
    // Success Responses
    // ========================================================================

    crow::response JsonResponse::success(const std::string& data, int status_code)
    {
        crow::json::wvalue json = createBaseResponse(true, status_code);
        json["data"] = data;
        return createResponse(json, status_code);
    }

    crow::response JsonResponse::success(crow::json::wvalue data, int status_code)
    {
        crow::json::wvalue json = createBaseResponse(true, status_code);
        json["data"] = std::move(data);
        return createResponse(json, status_code);
    }

    // ========================================================================
    // Error Responses
    // ========================================================================

    crow::response JsonResponse::error(const std::string& message, int status_code)
    {
        crow::json::wvalue json = createBaseResponse(false, status_code);
        json["error"]["message"] = message;
        return createResponse(json, status_code);
    }

    crow::response JsonResponse::validation_error(const std::string& field, const std::string& message)
    {
        crow::json::wvalue json = createBaseResponse(false, 400);
        json["error"]["message"] = "Validation error";
        json["error"]["field"] = field;
        json["error"]["details"] = message;
        return createResponse(json, 400);
    }

    crow::response JsonResponse::not_found(const std::string& resource)
    {
        crow::json::wvalue json = createBaseResponse(false, 404);
        json["error"]["message"] = resource + " not found";
        return createResponse(json, 404);
    }

    crow::response JsonResponse::unauthorized(const std::string& message)
    {
        crow::json::wvalue json = createBaseResponse(false, 401);
        json["error"]["message"] = message;
        return createResponse(json, 401);
    }

    crow::response JsonResponse::rate_limited(int retry_after_seconds)
    {
        crow::json::wvalue json = createBaseResponse(false, 429);
        json["error"]["message"] = "Too many requests";
        json["error"]["retry_after_seconds"] = retry_after_seconds;

        crow::response response = createResponse(json, 429);
        response.add_header("Retry-After", std::to_string(retry_after_seconds));
        return response;
    }

    // ========================================================================
    // Specialized Responses
    // ========================================================================

    crow::response JsonResponse::paginated(crow::json::wvalue data,
                                           int page, int per_page, int total_count)
    {
        crow::json::wvalue json = createBaseResponse(true, 200);
        json["data"] = std::move(data);

        crow::json::wvalue pagination;
        pagination["page"] = page;
        pagination["per_page"] = per_page;
        pagination["total_count"] = total_count;
        pagination["total_pages"] = (total_count + per_page - 1) / per_page; // Ceiling division

        json["pagination"] = std::move(pagination);
        return createResponse(json, 200);
    }

    // ========================================================================
    // Response Builder Implementation
    // ========================================================================

    JsonResponse::ResponseBuilder& JsonResponse::ResponseBuilder::withTimestamp()
    {
        data["timestamp"] = getCurrentTimestamp();
        return *this;
    }

    JsonResponse::ResponseBuilder& JsonResponse::ResponseBuilder::withStatus(int code)
    {
        status_code = code;
        return *this;
    }

    JsonResponse::ResponseBuilder& JsonResponse::ResponseBuilder::withData(
        const std::string& key, crow::json::wvalue value)
    {
        data["data"][key] = std::move(value);
        return *this;
    }

    JsonResponse::ResponseBuilder& JsonResponse::ResponseBuilder::withMessage(const std::string& message)
    {
        data["message"] = message;
        return *this;
    }

    JsonResponse::ResponseBuilder& JsonResponse::ResponseBuilder::withError(const std::string& error)
    {
        data["error"]["message"] = error;
        data["success"] = false;
        return *this;
    }

    JsonResponse::ResponseBuilder& JsonResponse::ResponseBuilder::withMetadata(
        const std::string& key, const std::string& value)
    {
        data["metadata"][key] = value;
        return *this;
    }

    crow::response JsonResponse::ResponseBuilder::build()
    {
        if (data.size() == 0)
        {
            data = createBaseResponse(true, status_code);
        }

        return createResponse(data, status_code);
    }

    JsonResponse::ResponseBuilder JsonResponse::builder()
    {
        ResponseBuilder builder;
        builder.data = createBaseResponse(true, 200);
        builder.withTimestamp();
        return builder;
    }

    // ========================================================================
    // Private Helper Methods
    // ========================================================================

    crow::json::wvalue JsonResponse::createBaseResponse(bool success, int status_code)
    {
        crow::json::wvalue json;
        json["success"] = success;
        json["status_code"] = status_code;
        json["timestamp"] = getCurrentTimestamp();
        return json;
    }

    std::string JsonResponse::getCurrentTimestamp()
    {
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);

        std::tm tm_now;
#ifdef _WIN32
        gmtime_s(&tm_now, &time_t_now);
#else
        gmtime_r(&time_t_now, &tm_now);
#endif

        std::ostringstream oss;
        oss << std::put_time(&tm_now, "%Y-%m-%dT%H:%M:%SZ");
        return oss.str();
    }

    crow::response JsonResponse::createResponse(const crow::json::wvalue& json, int status_code)
    {
        crow::response response;
        response.code = status_code;
        response.body = json.dump();
        response.add_header("Content-Type", "application/json");
        addCorsHeaders(response);
        return response;
    }

    void JsonResponse::addCorsHeaders(crow::response& response)
    {
        // Enable CORS for web dashboard compatibility
        response.add_header("Access-Control-Allow-Origin", "*");
        response.add_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        response.add_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
        response.add_header("Access-Control-Max-Age", "3600");
    }

}
