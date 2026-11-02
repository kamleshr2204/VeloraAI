#include "ai/OpenAICompatibleProvider.h"

#include "core/Exceptions.h"

#include "httplib.h"

#include <algorithm>
#include <map>

using json = nlohmann::json;

OpenAICompatibleProvider::OpenAICompatibleProvider(const Config& config)
    : baseUrl_(config.baseUrl()),
      basePath_(config.basePath()),
      apiKey_(config.apiKey()),
      model_(config.model()) {}

std::unique_ptr<LLMProvider> createProvider(const Config& config) {
    if (config.isOpenAI()) return std::make_unique<OpenAICompatibleProvider>(config);
    return std::make_unique<LocalServerProvider>(config);
}

Completion OpenAICompatibleProvider::complete(const json& messages, const json& tools,
                                              const DeltaCallback& onDelta) const {
    json body = {{"model", model_}, {"messages", messages}, {"stream", true}};
    if (!tools.empty()) body["tools"] = tools;

    Completion result;
    int status = 0;
    std::string lineBuffer, errorBody, streamError;
    std::map<int, ToolCall> pending;  // tool calls arrive in small pieces, keyed by index
    bool sawDone = false;

    // Handles ONE line of the server-sent-events stream:  data: {...json...}
    auto handleLine = [&](const std::string& line) {
        if (line.rfind("data:", 0) != 0) return true;
        std::string payload = line.substr(5);
        if (!payload.empty() && payload[0] == ' ') payload.erase(0, 1);
        if (payload == "[DONE]") { sawDone = true; return true; }

        json j;
        try { j = json::parse(payload); } catch (...) { return true; }

        if (j.contains("error")) {
            streamError = j["error"].is_object() ? j["error"].value("message", "Unknown error")
                                                 : j["error"].dump();
            return true;
        }
        if (!j.contains("choices") || !j["choices"].is_array() || j["choices"].empty()) return true;
        const json& delta = j["choices"][0].value("delta", json::object());

        if (delta.contains("content") && delta["content"].is_string()) {
            std::string piece = delta["content"];
            if (!piece.empty()) {
                result.content += piece;
                if (!onDelta(piece)) { result.cancelled = true; return false; }
            }
        }
        if (delta.contains("tool_calls") && delta["tool_calls"].is_array()) {
            for (const auto& tc : delta["tool_calls"]) {
                ToolCall& call = pending[tc.value("index", 0)];
                if (tc.contains("id") && tc["id"].is_string()) call.id = tc["id"];
                if (tc.contains("function") && tc["function"].is_object()) {
                    const json& f = tc["function"];
                    if (f.contains("name") && f["name"].is_string()) call.name += f["name"].get<std::string>();
                    if (f.contains("arguments") && f["arguments"].is_string())
                        call.arguments += f["arguments"].get<std::string>();
                }
            }
        }
        return true;
    };

    httplib::Client client(baseUrl_);
    client.set_connection_timeout(10, 0);
    client.set_read_timeout(120, 0);
    client.set_write_timeout(30, 0);

    httplib::Request request;
    request.method = "POST";
    request.path = basePath_ + "/chat/completions";
    request.set_header("Content-Type", "application/json");
    request.set_header("Accept", "text/event-stream");
    if (!apiKey_.empty()) request.set_header("Authorization", "Bearer " + apiKey_);
    request.body = body.dump();

    request.response_handler = [&](const httplib::Response& res) {
        status = res.status;
        return true;
    };
    request.content_receiver = [&](const char* data, size_t len, size_t, size_t) {
        if (status != 200) {  // an error message, not a stream
            if (errorBody.size() < 4000) errorBody.append(data, std::min(len, (size_t)4000));
            return true;
        }
        lineBuffer.append(data, len);
        size_t nl;
        while ((nl = lineBuffer.find('\n')) != std::string::npos) {
            std::string line = lineBuffer.substr(0, nl);
            lineBuffer.erase(0, nl + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (!handleLine(line)) return false;
        }
        return true;
    };

    httplib::Response response;
    httplib::Error error = httplib::Error::Success;
    bool sent = client.send(request, response, error);

    if (result.cancelled) return result;  // user pressed Stop: not an error

    if (!sent && status == 0)
        throw NetworkError("Could not reach the AI service. Check your internet connection.");
    if (status != 200) throw ApiError::fromResponse(status, errorBody);
    if (!streamError.empty()) throw ApiError(status, streamError);
    if (!sent && !sawDone && result.content.empty() && pending.empty())
        throw NetworkError("The connection to the AI service was interrupted. Please try again.");

    if (!lineBuffer.empty()) handleLine(lineBuffer);  // last line without a newline

    int round = 0;
    for (auto& [index, call] : pending) {
        if (call.name.empty()) continue;
        if (call.id.empty()) call.id = "call_" + std::to_string(round) + "_" + std::to_string(index);
        result.toolCalls.push_back(call);
        ++round;
    }
    return result;
}
