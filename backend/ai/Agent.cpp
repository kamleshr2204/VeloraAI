#include "ai/Agent.h"

#include "core/Exceptions.h"

#include <algorithm>

using json = nlohmann::json;

Agent::Agent(std::unique_ptr<LLMProvider> provider, ToolRegistry tools, PromptBuilder prompt)
    : provider_(std::move(provider)), tools_(std::move(tools)), prompt_(std::move(prompt)) {
    if (!provider_) throw ConfigError("Agent needs an AI provider.");
}

void Agent::trimHistory(std::vector<ChatMessage>& conversation) {
    if (conversation.size() > kMaxMessages)
        conversation.erase(conversation.begin(), conversation.end() - kMaxMessages);

    std::size_t total = 0, keepFrom = conversation.size();
    while (keepFrom > 0) {
        std::size_t len = conversation[keepFrom - 1].content.size();
        if (total + len > kMaxHistoryChars && keepFrom < conversation.size()) break;
        total += len;
        --keepFrom;
    }
    conversation.erase(conversation.begin(), conversation.begin() + keepFrom);

    // The model expects the conversation to start with the user.
    while (!conversation.empty() && conversation.front().role != "user")
        conversation.erase(conversation.begin());
}

void Agent::chat(std::vector<ChatMessage> conversation, const Emit& emit) const {
    auto fail = [&](const std::string& message) { emit({{"type", "error"}, {"text", message}}); };

    if (!provider_->isConfigured()) {
        fail("OPENAI_API_KEY is not set. Set it in the terminal before starting VeloraAI "
             "(see README), then restart the backend.");
        return;
    }

    trimHistory(conversation);
    if (conversation.empty()) { fail("Nothing to answer."); return; }

    json messages = json::array();
    messages.push_back({{"role", "system"}, {"content", prompt_.build()}});
    for (const auto& m : conversation) messages.push_back({{"role", m.role}, {"content", m.content}});

    const json toolDefinitions = tools_.definitions();
    std::vector<Source> alreadySent;  // so the same link is not shown twice

    for (int round = 0; round < kMaxToolRounds; ++round) {
        // On the very last round we forbid tools, so the model MUST write an answer.
        const bool allowTools = round < kMaxToolRounds - 1;

        Completion completion;
        try {
            completion = provider_->complete(
                messages, allowTools ? toolDefinitions : json::array(),
                [&](const std::string& piece) { return emit({{"type", "delta"}, {"text", piece}}); });
        } catch (const VeloraError& e) {  // ApiError, NetworkError ... (inheritance at work!)
            fail(e.what());
            return;
        }

        if (completion.cancelled) return;  // user pressed Stop

        if (completion.toolCalls.empty()) {  // no tool wanted: that was the final answer
            if (completion.content.empty()) fail("The model returned an empty answer. Please try again.");
            return;
        }

        // Remember that the assistant asked for tools (the model needs to see this next round).
        json assistantMessage = {{"role", "assistant"}, {"tool_calls", json::array()}};
        assistantMessage["content"] = completion.content.empty() ? json(nullptr) : json(completion.content);
        for (const auto& call : completion.toolCalls)
            assistantMessage["tool_calls"].push_back(
                {{"id", call.id},
                 {"type", "function"},
                 {"function",
                  {{"name", call.name}, {"arguments", call.arguments.empty() ? "{}" : call.arguments}}}});
        messages.push_back(assistantMessage);

        // Run every requested tool.
        for (const auto& call : completion.toolCalls) {
            json args = json::object();
            try {
                if (!call.arguments.empty()) args = json::parse(call.arguments);
            } catch (...) {
                // Bad JSON from the model: run the tool with no arguments; it will report the problem.
            }

            const Tool* tool = tools_.find(call.name);  // base-class pointer to ANY kind of tool
            const std::string statusText = tool ? tool->describe(args) : "Using " + call.name;
            if (!emit({{"type", "status"}, {"text", statusText}})) return;

            ToolResult result = tool ? tool->execute(args)  // polymorphism: child's run() executes
                                     : ToolResult{"Unknown tool: " + call.name, {}};

            json fresh = json::array();
            int shown = 0;
            for (const auto& s : result.sources) {
                if (shown++ >= 5) break;
                if (std::find(alreadySent.begin(), alreadySent.end(), s) == alreadySent.end()) {  // uses our operator==
                    alreadySent.push_back(s);
                    fresh.push_back({{"title", s.title}, {"url", s.url}});
                }
            }
            if (!fresh.empty() && !emit({{"type", "sources"}, {"sources", fresh}})) return;

            messages.push_back({{"role", "tool"}, {"tool_call_id", call.id}, {"content", result.text}});
        }
        // loop: the model now reads the tool results and either answers or uses more tools
    }
    fail("VeloraAI needed too many steps to answer that. Try asking in a more specific way.");
}
