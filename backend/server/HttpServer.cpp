#include "server/HttpServer.h"

#include <fstream>
#include <iostream>
#include <memory>
#include <cstdlib>

using json = nlohmann::json;

HttpServer::HttpServer(const Config& config, const Agent& agent) : config_(config), agent_(agent) {
    server_.set_payload_max_length(2 * 1024 * 1024);
    server_.new_task_queue = [] { return new httplib::ThreadPool(16); };
    setupSecurityHeaders();
    setupRoutes();
    mountFrontend();
}

bool HttpServer::originAllowed(const std::string& origin) {
    if (origin.empty() || origin == "null") return true;

    const char* allowed = std::getenv("VELORA_ALLOWED_ORIGIN");

    if (allowed && origin == allowed) return true;

    for (const char* ok : {"http://localhost", "http://127.0.0.1"}) {
        std::string prefix = ok;
        if (origin.compare(0, prefix.size(), prefix) == 0 &&
            (origin.size() == prefix.size() || origin[prefix.size()] == ':'))
            return true;
    }

    return false;
}

bool HttpServer::parseConversation(const json& body, std::vector<ChatMessage>& out,
                                   std::string& error) {
    json list = json::array();
    if (body.contains("messages") && body["messages"].is_array()) {
        list = body["messages"];
    } else if (body.contains("message") && body["message"].is_string()) {
        if (body.contains("history") && body["history"].is_array()) list = body["history"];
        list.push_back({{"role", "user"}, {"content", body["message"]}});
    } else {
        error = "Please provide a message.";
        return false;
    }

    for (const auto& m : list) {
        if (!m.is_object() || !m.contains("role") || !m.contains("content") ||
            !m["role"].is_string() || !m["content"].is_string())
            continue;
        std::string role = m["role"], content = m["content"];
        if ((role != "user" && role != "assistant") || content.empty()) continue;
        if (content.size() > kMaxMessageChars) content.resize(kMaxMessageChars);
        out.push_back({role, content});
    }
    if (out.empty() || out.back().role != "user") {
        error = "The last message must be from the user.";
        return false;
    }
    return true;
}

void HttpServer::setupSecurityHeaders() {
    // Runs before every request: block foreign origins, add CORS headers.
    server_.set_pre_routing_handler([](const httplib::Request& req, httplib::Response& res) {
        std::string origin = req.get_header_value("Origin");
        if (!originAllowed(origin)) {
            res.status = 403;
            res.set_content(R"({"error":"Origin not allowed."})", "application/json");
            return httplib::Server::HandlerResponse::Handled;
        }
        if (!origin.empty()) {
            res.set_header("Access-Control-Allow-Origin", origin);
            res.set_header("Vary", "Origin");
        }
        res.set_header("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type");
        return httplib::Server::HandlerResponse::Unhandled;
    });
    server_.Options("/chat", [](const httplib::Request&, httplib::Response& res) { res.status = 204; });
}

void HttpServer::setupRoutes() {
    // ---- health: lets the page show model / search status ----
    server_.Get("/health", [this](const httplib::Request&, httplib::Response& res) {
        res.set_content(json({{"ok", true},
                              {"configured", agent_.isConfigured()},
                              {"model", agent_.modelName()},
                              {"search", config_.searchProviderName()}})
                            .dump(),
                        "application/json");
    });

    // ---- chat: streams Server-Sent Events ----
    server_.Post("/chat", [this](const httplib::Request& req, httplib::Response& res) {
        std::vector<ChatMessage> conversation;
        std::string error;
        try {
            json body = json::parse(req.body);
            if (!parseConversation(body, conversation, error)) {
                res.status = 400;
                res.set_content(json({{"error", error}}).dump(), "application/json");
                return;
            }
        } catch (const std::exception&) {
            res.status = 400;
            res.set_content(R"({"error":"Invalid request."})", "application/json");
            return;
        }

        // The streaming callback runs later (after this function returned), so the
        // conversation must live on the heap: shared_ptr keeps it alive until the end.
        auto convo = std::make_shared<std::vector<ChatMessage>>(std::move(conversation));
        res.set_header("Cache-Control", "no-cache");
        res.set_header("X-Accel-Buffering", "no");
        res.set_chunked_content_provider(
            "text/event-stream", [this, convo](size_t, httplib::DataSink& sink) {
                auto emit = [&sink](const json& event) {
                    std::string line =
                        "data: " + event.dump(-1, ' ', false, json::error_handler_t::replace) + "\n\n";
                    return sink.is_writable() && sink.write(line.data(), line.size());
                };
                try {
                    agent_.chat(*convo, emit);
                } catch (const std::exception& e) {
                    emit({{"type", "error"}, {"text", std::string("Unexpected error: ") + e.what()}});
                }
                emit({{"type", "done"}});
                sink.done();
                return true;
            });
    });
}

std::string HttpServer::findFrontendDir() const {
    auto hasIndex = [](const std::string& dir) { return std::ifstream(dir + "/index.html").good(); };
    if (!config_.frontendDir().empty()) return config_.frontendDir();
    for (const char* candidate : {"../frontend", "frontend", "../../frontend"})
        if (hasIndex(candidate)) return candidate;
    return "";
}

void HttpServer::mountFrontend() {
    std::string dir = findFrontendDir();
    if (!dir.empty() && server_.set_mount_point("/", dir))
        std::cout << "Serving the web page from: " << dir << "\n";
    else
        std::cout << "(Frontend folder not found - open frontend/index.html manually.)\n";
}

void HttpServer::printBanner() const {
    std::cout << "\nVeloraAI backend\n"
              << "  Model:   " << agent_.modelName() << "\n"
              << "  Search:  " << config_.searchProviderName() << "\n"
              << "  API key: " << (agent_.isConfigured() ? "found" : "MISSING - set OPENAI_API_KEY") << "\n\n"
              << "Open  http://localhost:" << config_.port() << "  in your browser.\n"
              << "Keep this window open while using VeloraAI.\n"
              << std::flush;  // show it immediately, even when output goes to a log file
}

bool HttpServer::run() {
    printBanner();
    if (!server_.listen(config_.host(), config_.port())) {
        std::cerr << "Could not start server. Port " << config_.port() << " may already be in use.\n";
        return false;
    }
    return true;
}
