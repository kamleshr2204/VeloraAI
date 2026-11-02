// All settings of the program in ONE place.
//
// OOP ideas used here:
//   * Encapsulation   - the data members are private; the outside world can only
//                       READ them through getters. Nobody can change the API key by accident.
//   * Private constructor + static factory - you cannot write  Config c;  you must
//                       call  Config::fromEnvironment();  which also validates the values.
//   * const-correctness - every getter is marked const: "calling me does not change the object".
//
// Why not a Singleton? Singletons are global state, which makes testing hard.
// Instead main() creates one Config and passes it (by const reference) to
// whoever needs it. This is called "dependency injection".
#ifndef VELORA_CORE_CONFIG_H
#define VELORA_CORE_CONFIG_H

#include <string>

class Config {
public:
    // Reads environment variables (OPENAI_API_KEY, VELORA_MODEL, ...).
    // Throws ConfigError if something is invalid.
    static Config fromEnvironment();

    // ---- getters (read-only access) ----
    const std::string& apiKey() const { return apiKey_; }
    const std::string& baseUrl() const { return baseUrl_; }    // e.g. https://api.openai.com
    const std::string& basePath() const { return basePath_; }  // e.g. /v1
    const std::string& model() const { return model_; }
    const std::string& tavilyKey() const { return tavilyKey_; }
    const std::string& host() const { return host_; }
    const std::string& frontendDir() const { return frontendDir_; }
    int port() const { return port_; }

    bool isOpenAI() const;                  // true when talking to api.openai.com
    std::string searchProviderName() const; // "Tavily" or "DuckDuckGo"

private:
    Config() = default;  // only fromEnvironment() may create a Config

    std::string apiKey_;
    std::string baseUrl_;
    std::string basePath_;
    std::string model_;
    std::string tavilyKey_;
    std::string host_;
    std::string frontendDir_;
    int port_ = 8080;
};

#endif  // VELORA_CORE_CONFIG_H
