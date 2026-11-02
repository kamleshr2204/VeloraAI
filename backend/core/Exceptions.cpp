#include "core/Exceptions.h"

#include <nlohmann/json.hpp>

ApiError ApiError::fromResponse(int status, const std::string& rawBody) {
    std::string detail;
    try {
        nlohmann::json j = nlohmann::json::parse(rawBody);
        if (j.contains("error") && j["error"].is_object())
            detail = j["error"].value("message", "");
        else if (j.contains("error") && j["error"].is_string())
            detail = j["error"];
    } catch (...) {
        // The body was not JSON - we simply have no extra detail.
    }

    std::string head;
    switch (status) {
        case 401: head = "The API key was rejected. Check OPENAI_API_KEY."; break;
        case 403: head = "The API refused this request (no access to this model or region)."; break;
        case 404: head = "Model not found. Check the VELORA_MODEL setting."; break;
        case 429: head = "Rate limit or quota reached. Check your usage/billing, then try again."; break;
        default:
            head = status >= 500
                       ? "The AI service had a problem. Try again in a moment."
                       : "The AI service returned an error (HTTP " + std::to_string(status) + ").";
    }
    return ApiError(status, detail.empty() ? head : head + " (" + detail + ")");
}
