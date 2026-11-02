// HttpServer: the web server. It exposes the Agent to the browser.
//
//   GET  /health   -> {"ok":true,"configured":...,"model":...,"search":...}
//   POST /chat     -> streams the answer as Server-Sent Events
//   GET  /         -> the web page (frontend/)
//
// OOP ideas used here:
//   * Encapsulation - routes, CORS rules and body parsing are private helper functions.
//     The outside world only sees the constructor and run().
//   * Reference members - HttpServer does not own the Config or the Agent; it just
//     "looks at" them. A reference must be set in the constructor's initializer list and
//     can never be null. (main() makes sure they outlive the server.)
//   * Static member functions - originAllowed() and parseConversation() use no object data,
//     so they are static: easy to test, and clearly "pure helpers".
#ifndef VELORA_SERVER_HTTPSERVER_H
#define VELORA_SERVER_HTTPSERVER_H

#include "ai/Agent.h"
#include "core/Config.h"
#include "core/Types.h"

#include "httplib.h"

#include <string>
#include <vector>

class HttpServer {
public:
    HttpServer(const Config& config, const Agent& agent);

    // Starts listening. Blocks until the server stops. Returns false if it could not start.
    bool run();

    // Only pages served by this computer may call the API. Otherwise any website you visit
    // could spend your API credits.
    static bool originAllowed(const std::string& origin);

    // Turns the JSON sent by the browser into a list of ChatMessage. Returns false and sets
    // `error` if the request is invalid.
    static bool parseConversation(const nlohmann::json& body, std::vector<ChatMessage>& out,
                                  std::string& error);

    static constexpr std::size_t kMaxMessageChars = 20000;

private:
    void setupSecurityHeaders();
    void setupRoutes();
    void mountFrontend();
    std::string findFrontendDir() const;
    void printBanner() const;

    const Config& config_;
    const Agent& agent_;
    httplib::Server server_;
};

#endif  // VELORA_SERVER_HTTPSERVER_H
