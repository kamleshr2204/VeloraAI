// Agent: runs one conversation turn.
//
//   "Ask the model -> it wants a tool -> run the tool -> show the model the result ->
//    ask again ..." until the model writes its final answer.
//
// OOP ideas used here:
//   * Composition  - an Agent HAS-A LLMProvider, HAS-A ToolRegistry and HAS-A PromptBuilder.
//     It is built from smaller objects instead of doing everything itself.
//   * Dependency injection - the Agent does not create its provider; the caller hands it in.
//     That is why a test can inject a fake provider.
//   * Ownership with unique_ptr - the constructor takes the provider "by value" and the
//     caller must std::move() it: the Agent now OWNS it and deletes it automatically.
//   * const member function - chat() does not change the Agent, so many requests (threads)
//     can safely use ONE Agent at the same time.
//   * Polymorphism - provider_->complete() and tool->execute() are virtual calls.
#ifndef VELORA_AI_AGENT_H
#define VELORA_AI_AGENT_H

#include "ai/LLMProvider.h"
#include "ai/PromptBuilder.h"
#include "core/Types.h"
#include "tools/ToolRegistry.h"

#include <functional>
#include <memory>
#include <vector>

class Agent {
public:
    // Called with UI events: {"type":"status"|"sources"|"delta"|"error", ...}.
    // Return false when the browser has gone away, so work can stop early.
    using Emit = std::function<bool(const nlohmann::json&)>;

    Agent(std::unique_ptr<LLMProvider> provider, ToolRegistry tools, PromptBuilder prompt = {});

    // Answer a conversation. Never throws; problems are sent as "error" events.
    void chat(std::vector<ChatMessage> conversation, const Emit& emit) const;

    bool isConfigured() const { return provider_->isConfigured(); }
    std::string modelName() const { return provider_->modelName(); }

    // Limits (public constants so tests and docs can refer to them).
    static constexpr int kMaxToolRounds = 6;
    static constexpr std::size_t kMaxMessages = 40;
    static constexpr std::size_t kMaxHistoryChars = 60000;

private:
    // Keep only the most recent part of the conversation (the bot's "memory").
    static void trimHistory(std::vector<ChatMessage>& conversation);

    std::unique_ptr<LLMProvider> provider_;
    ToolRegistry tools_;
    PromptBuilder prompt_;
};

#endif  // VELORA_AI_AGENT_H
