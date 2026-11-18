#include "core/Config.h"

#include "core/Exceptions.h"

#include <cstdlib>

namespace {

// Read an environment variable, or use a default when it is missing/empty.
std::string env(const char* name, const std::string& fallback = "") {
    const char* v = std::getenv(name);
    return (v && *v) ? std::string(v) : fallback;
}

}  // namespace

Config Config::fromEnvironment() {
    Config c;  // allowed: we are inside a member function of Config

    std::string base = env("OPENAI_BASE_URL", "https://api.openai.com/v1");
    while (!base.empty() && base.back() == '/') base.pop_back();

    // Split "https://host/v1" into "https://host" and "/v1".
    size_t scheme = base.find("://");
    size_t pathStart = scheme == std::string::npos ? std::string::npos : base.find('/', scheme + 3);
    if (pathStart == std::string::npos) {
        c.baseUrl_ = base;
        c.basePath_ = "/v1";
    } else {
        c.baseUrl_ = base.substr(0, pathStart);
        c.basePath_ = base.substr(pathStart);
    }

    c.apiKey_ = env("OPENAI_API_KEY");
    c.model_ = env("VELORA_MODEL", "gpt-4.1-mini");
    c.tavilyKey_ = env("TAVILY_API_KEY");
    c.host_ = env("VELORA_HOST", "127.0.0.1");
    c.frontendDir_ = env("VELORA_FRONTEND");

    std::string portText = env("VELORA_PORT", "8080");
    try {
        c.port_ = std::stoi(portText);
    } catch (...) {
        throw ConfigError("VELORA_PORT must be a number, but it is '" + portText + "'.");
    }
    if (c.port_ < 1 || c.port_ > 65535)
        throw ConfigError("VELORA_PORT must be between 1 and 65535.");

    return c;
}

bool Config::isOpenAI() const {
    return !apiKey_.empty();
}

std::string Config::searchProviderName() const {
    return tavilyKey_.empty() ? "DuckDuckGo" : "Tavily";
}
