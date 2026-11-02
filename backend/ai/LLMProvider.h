// LLMProvider: the abstract "brain" interface.
//
// OOP ideas used here:
//   * Abstraction & Polymorphism - the Agent talks to "some LLMProvider". Today that is
//     OpenAI, tomorrow it can be a local Ollama model, and in our unit tests it is a
//     fake MockProvider that needs no internet. The Agent code never changes.
//     (This idea is called "programming to an interface, not an implementation".)
//   * Pure virtual functions define the contract: name(), isConfigured(), complete().
#ifndef VELORA_AI_LLMPROVIDER_H
#define VELORA_AI_LLMPROVIDER_H

#include <nlohmann/json.hpp>

#include <functional>
#include <string>
#include <vector>

// The model asked us to run a tool.
struct ToolCall {
    std::string id;
    std::string name;
    std::string arguments;  // JSON text, e.g. {"query":"weather in Pune"}
};

// What one request to the model produced.
struct Completion {
    std::string content;              // text the model wrote (may be empty if it only called tools)
    std::vector<ToolCall> toolCalls;  // tools the model wants to use
    bool cancelled = false;           // true if the browser went away while we were streaming
};

class LLMProvider {
public:
    // Called for every new piece of text. Return false to say "the user pressed Stop".
    using DeltaCallback = std::function<bool(const std::string&)>;

    virtual ~LLMProvider() = default;

    virtual std::string modelName() const = 0;
    virtual bool isConfigured() const = 0;  // do we have everything needed (e.g. an API key)?

    // Send the conversation to the model and stream the answer through onDelta.
    // Throws ApiError / NetworkError when something goes wrong.
    //   messages : JSON array of {role, content, ...}
    //   tools    : JSON array of tool definitions (empty array = no tools allowed)
    virtual Completion complete(const nlohmann::json& messages, const nlohmann::json& tools,
                                const DeltaCallback& onDelta) const = 0;
};

#endif  // VELORA_AI_LLMPROVIDER_H
