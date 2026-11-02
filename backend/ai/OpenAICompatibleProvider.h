#ifndef VELORA_AI_OPENAICOMPATIBLEPROVIDER_H
#define VELORA_AI_OPENAICOMPATIBLEPROVIDER_H

#include "ai/LLMProvider.h"
#include "core/Config.h"

#include <memory>

// Child of LLMProvider that speaks the "OpenAI chat completions" protocol.
// Many services use this same protocol (OpenAI, Ollama, LM Studio, Groq ...).
//
// OOP ideas used here:
//   * Inheritance   - IS-A LLMProvider.
//   * Constructor copies the few settings it needs from Config, so the provider does not
//     depend on the Config object staying alive.
//   * protected members - so a grandchild class (LocalServerProvider) can reuse/override them.
class OpenAICompatibleProvider : public LLMProvider {
public:
    explicit OpenAICompatibleProvider(const Config& config);

    std::string modelName() const override { return model_; }
    bool isConfigured() const override { return !apiKey_.empty(); }

    Completion complete(const nlohmann::json& messages, const nlohmann::json& tools,
                        const DeltaCallback& onDelta) const override;

protected:
    std::string baseUrl_;   // https://api.openai.com
    std::string basePath_;  // /v1
    std::string apiKey_;
    std::string model_;
};

// Grandchild: a model running on your own machine (Ollama, LM Studio).
// It is exactly the same protocol; the only difference is that no API key is needed.
// So we inherit EVERYTHING and override one tiny function.
class LocalServerProvider : public OpenAICompatibleProvider {
public:
    using OpenAICompatibleProvider::OpenAICompatibleProvider;  // reuse the parent's constructor

    bool isConfigured() const override { return true; }
};

// Factory function: picks the right child class for the current settings.
// Returns a pointer to the BASE class - the caller doesn't need to know which child it got.
std::unique_ptr<LLMProvider> createProvider(const Config& config);

#endif  // VELORA_AI_OPENAICOMPATIBLEPROVIDER_H
