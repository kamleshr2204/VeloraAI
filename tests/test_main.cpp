// Unit tests for VeloraAI. No framework needed: a tiny CHECK macro is enough.
//
// Run:  ./tests/run_tests.sh      (or the `velora_tests` target in CMake)
//
// Notice MockProvider below: because Agent depends on the abstract LLMProvider (and not on
// OpenAI directly), we can plug in a fake "AI" that returns scripted answers. This is
// polymorphism making code TESTABLE.
#include "ai/Agent.h"
#include "core/Config.h"
#include "core/Exceptions.h"
#include "search/DuckDuckGoSearch.h"
#include "server/HttpServer.h"
#include "tools/CalculatorTool.h"
#include "tools/DateTimeTool.h"
#include "tools/ExpressionParser.h"
#include "tools/ToolRegistry.h"
#include "tools/WebSearchTool.h"
#include "util/Cache.h"
#include "util/StringUtil.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <thread>

#ifdef _WIN32  // Windows has no setenv/unsetenv; give the tests equivalents.
inline int setenv(const char* k, const char* v, int) { return _putenv_s(k, v); }
inline int unsetenv(const char* k) { return _putenv_s(k, ""); }
#endif

using json = nlohmann::json;

static int g_checks = 0, g_failed = 0;

#define CHECK(cond)                                                                   \
    do {                                                                              \
        ++g_checks;                                                                   \
        if (!(cond)) {                                                                \
            ++g_failed;                                                               \
            std::cerr << "  FAILED  " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; \
        }                                                                             \
    } while (0)

#define CHECK_THROWS(statement, ExceptionType)                                        \
    do {                                                                              \
        ++g_checks;                                                                   \
        bool caught = false;                                                          \
        try { statement; } catch (const ExceptionType&) { caught = true; }            \
        if (!caught) {                                                                \
            ++g_failed;                                                               \
            std::cerr << "  FAILED  " << __FILE__ << ":" << __LINE__                  \
                      << "  expected " #ExceptionType " from: " #statement "\n";      \
        }                                                                             \
    } while (0)

static void section(const char* name) { std::cout << "- " << name << "\n"; }

// ---------------------------------------------------------------------------
// A fake AI. It replays a script of Completions and remembers what it was asked.
class MockProvider : public LLMProvider {
public:
    explicit MockProvider(std::vector<Completion> script, bool configured = true)
        : script_(std::move(script)), configured_(configured) {}

    std::string modelName() const override { return "mock-model"; }
    bool isConfigured() const override { return configured_; }

    Completion complete(const json& messages, const json& tools,
                        const DeltaCallback& onDelta) const override {
        seenMessages.push_back(messages);
        seenToolCounts.push_back(tools.size());
        if (next_ >= script_.size()) throw NetworkError("script exhausted");
        Completion c = script_[next_++];
        if (!c.content.empty() && !onDelta(c.content)) c.cancelled = true;
        return c;
    }

    mutable std::vector<json> seenMessages;
    mutable std::vector<std::size_t> seenToolCounts;

private:
    std::vector<Completion> script_;
    bool configured_;
    mutable std::size_t next_ = 0;
};

// A tiny extra tool that exists only in the tests - proves new tools plug in with no changes elsewhere.
class EchoTool final : public Tool {
public:
    std::string name() const override { return "echo"; }
    std::string description() const override { return "Echoes text."; }
    json parameters() const override { return {{"type", "object"}, {"properties", json::object()}}; }
    std::string describe(const json&) const override { return "Echoing"; }

protected:
    ToolResult run(const json& args) const override {
        if (stringArg(args, "text") == "boom") throw std::logic_error("kaboom");
        if (stringArg(args, "text") == "bad") throw ToolError("bad input");
        return {"echo: " + stringArg(args, "text"), {{"Echo page", "https://example.com/echo"}}};
    }
};

// Collects every event the Agent emits.
struct Events {
    std::vector<json> list;
    Agent::Emit emitter() { return [this](const json& e) { list.push_back(e); return true; }; }
    std::string text() const {
        std::string s;
        for (auto& e : list) if (e["type"] == "delta") s += e["text"].get<std::string>();
        return s;
    }
    int count(const std::string& type) const {
        int n = 0;
        for (auto& e : list) if (e["type"] == type) ++n;
        return n;
    }
};

// ---------------------------------------------------------------------------

static void testStringUtil() {
    section("StringUtil");
    CHECK(util::trim("  hi \n") == "hi");
    CHECK(util::lower("AbC") == "abc");
    CHECK(util::startsWith("hello", "he") && !util::startsWith("he", "hello"));
    CHECK(util::endsWith("a.txt", ".txt"));
    CHECK(util::urlEncode("a b&c") == "a%20b%26c");
    CHECK(util::urlDecode("a%20b+c") == "a b c");
    CHECK(util::decodeEntities("Tom &amp; Jerry &#65;&#x42;") == "Tom & Jerry AB");
    CHECK(util::stripTags("<b>Hi</b> <i>there</i>") == "Hi there");
    CHECK(util::truncate("abcdef", 3).rfind("abc", 0) == 0);
    CHECK(util::truncate("short", 100) == "short");
    std::string page = "<html><head><title>T</title></head><body><script>x()</script>"
                       "<h1>Hello</h1><p>World &amp; more</p></body></html>";
    CHECK(util::htmlToText(page) == "Hello\n\nWorld & more" || util::htmlToText(page) == "Hello\nWorld & more");
}

static void testExpressionParser() {
    section("ExpressionParser (calculator engine)");
    auto eval = [](const std::string& s) { return ExpressionParser::evaluateToString(s); };
    CHECK(eval("1+2*3") == "7");
    CHECK(eval("(1+2)*3") == "9");
    CHECK(eval("2^3^2") == "512");          // right-associative
    CHECK(eval("2**10") == "1024");
    CHECK(eval("-3+5") == "2");
    CHECK(eval("10/4") == "2.5");
    CHECK(eval("10%4") == "2");
    CHECK(eval("5!") == "120");
    CHECK(eval("sqrt(144)") == "12");
    CHECK(eval("max(3, 9, 4)") == "9");
    CHECK(eval("round(pi*100)") == "314");
    CHECK(eval("1200*(1+0.07)^5") == "1683.06207684");
    CHECK_THROWS(eval("1/0"), std::runtime_error);
    CHECK_THROWS(eval("2+"), std::runtime_error);
    CHECK_THROWS(eval("(2+3"), std::runtime_error);
    CHECK_THROWS(eval("foo(1)"), std::runtime_error);
    CHECK_THROWS(eval("sqrt(-1)"), std::runtime_error);
}

static void testCache() {
    section("Cache<K,V> template");
    Cache<std::string, int> c(2, std::chrono::seconds(60));
    CHECK(!c.get("a").has_value());
    c.put("a", 1);
    c.put("b", 2);
    CHECK(c.get("a").value() == 1);
    c.put("c", 3);                       // full: oldest entry (a) is evicted
    CHECK(c.size() == 2);
    CHECK(!c.get("a").has_value());
    CHECK(c.get("c").value() == 3);

    Cache<int, std::string> other(5, std::chrono::seconds(0));   // same template, other types
    other.put(1, "x");
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    CHECK(!other.get(1).has_value());    // expired immediately (ttl = 0)
}

static void testTools() {
    section("Tool hierarchy (polymorphism)");
    CalculatorTool calc;
    const Tool& asBase = calc;           // use it through the BASE class
    CHECK(asBase.name() == "calculator");
    CHECK(asBase.execute({{"expression", "6*7"}}).text == "6*7 = 42");
    CHECK(asBase.execute({{"expression", "1/0"}}).text.find("Calculator error") == 0);
    CHECK(asBase.definition()["function"]["name"] == "calculator");
    CHECK(asBase.definition()["type"] == "function");

    DateTimeTool clock;
    CHECK(clock.execute({{"utc_offset", "+05:30"}}).text.find("UTC+05:30") != std::string::npos);
    CHECK(DateTimeTool::now("UTC-08:00").find("UTC-08:00") != std::string::npos);

    EchoTool echo;
    CHECK(echo.execute({{"text", "hi"}}).text == "echo: hi");
    CHECK(echo.execute({{"text", "hi"}}).sources.size() == 1);
    CHECK(echo.execute({{"text", "bad"}}).text == "Error: bad input");              // ToolError
    CHECK(echo.execute({{"text", "boom"}}).text == "Tool 'echo' failed: kaboom");   // other exception
}

static void testRegistry() {
    section("ToolRegistry");
    ToolRegistry reg;
    reg.add(std::make_unique<CalculatorTool>());
    reg.add(std::make_unique<EchoTool>());
    CHECK(reg.size() == 2);
    CHECK(reg.find("echo") != nullptr);
    CHECK(reg.find("nope") == nullptr);
    CHECK(reg.definitions().size() == 2);
    CHECK_THROWS(reg.add(std::make_unique<EchoTool>()), ToolError);   // duplicate name
    CHECK_THROWS(reg.add(nullptr), ToolError);

    ToolRegistry moved = std::move(reg);                              // move keeps the tools valid
    CHECK(moved.find("calculator")->execute({{"expression", "2+2"}}).text == "2+2 = 4");

    setenv("OPENAI_API_KEY", "test", 1);
    unsetenv("TAVILY_API_KEY");
    ToolRegistry defaults = ToolRegistry::createDefault(Config::fromEnvironment());
    CHECK(defaults.size() == 4);
    for (const char* n : {"web_search", "fetch_url", "calculator", "get_datetime"})
        CHECK(defaults.find(n) != nullptr);
}

static void testConfig() {
    section("Config");
    setenv("OPENAI_API_KEY", "k", 1);
    setenv("OPENAI_BASE_URL", "http://localhost:11434/v1/", 1);
    setenv("VELORA_MODEL", "llama3", 1);
    unsetenv("VELORA_PORT");
    Config c = Config::fromEnvironment();
    CHECK(c.baseUrl() == "http://localhost:11434");
    CHECK(c.basePath() == "/v1");
    CHECK(c.model() == "llama3");
    CHECK(c.port() == 8080);
    CHECK(!c.isOpenAI());
    setenv("VELORA_PORT", "abc", 1);
    CHECK_THROWS(Config::fromEnvironment(), ConfigError);
    setenv("VELORA_PORT", "70000", 1);
    CHECK_THROWS(Config::fromEnvironment(), ConfigError);
    unsetenv("VELORA_PORT");
    unsetenv("OPENAI_BASE_URL");
    unsetenv("VELORA_MODEL");
    Config d = Config::fromEnvironment();
    CHECK(d.isOpenAI() && d.model() == "gpt-4.1-mini");
}

static void testExceptions() {
    section("Exceptions (inheritance)");
    try {
        throw ApiError::fromResponse(401, R"({"error":{"message":"bad key"}})");
    } catch (const VeloraError& e) {          // caught through the BASE class
        CHECK(std::string(e.what()).find("API key was rejected") != std::string::npos);
        CHECK(std::string(e.what()).find("bad key") != std::string::npos);
    }
    CHECK(ApiError::fromResponse(429, "").status() == 429);
    CHECK(std::string(ApiError::fromResponse(500, "oops").what()).find("had a problem") != std::string::npos);
    try { throw NetworkError("x"); } catch (const std::runtime_error&) { CHECK(true); }
}

static void testSearch() {
    section("SearchEngine (DuckDuckGo parser, WebSearchTool)");
    std::string html =
        "<a class=\"result__a\" href=\"//duckduckgo.com/l/?uddg=https%3A%2F%2Fexample.com%2Fa&amp;rut=1\">First &amp; best</a>"
        "<a class=\"result__snippet\" href=\"x\">The <b>first</b> snippet</a>"
        "<a class=\"result__a\" href=\"https://example.org/b\">Second</a>";
    auto hits = DuckDuckGoSearch::parseResults(html, 5);
    CHECK(hits.size() == 2);
    CHECK(hits[0].url == "https://example.com/a");
    CHECK(hits[0].title == "First & best");
    CHECK(hits[0].snippet == "The first snippet");
    CHECK(hits[1].url == "https://example.org/b");

    // Fake engines: first finds nothing, second succeeds => WebSearchTool falls through (polymorphism).
    struct Empty : SearchEngine {
        std::string name() const override { return "empty"; }
        SearchResponse search(const std::string&, int) const override { return {}; }
    };
    struct Counting : SearchEngine {
        mutable int calls = 0;
        std::string name() const override { return "counting"; }
        SearchResponse search(const std::string& q, int) const override {
            ++calls;
            return {{{"Result for " + q, "https://example.com/r", "snippet"}}, "quick"};
        }
    };
    WebSearchTool::EngineList engines;
    engines.push_back(std::make_unique<Empty>());
    auto counting = std::make_unique<Counting>();
    Counting* counter = counting.get();
    engines.push_back(std::move(counting));
    WebSearchTool tool(std::move(engines));

    ToolResult r = tool.execute({{"query", "cpp polymorphism"}});
    CHECK(r.sources.size() == 1 && r.sources[0].url == "https://example.com/r");
    CHECK(r.text.find("Quick answer from the search engine: quick") != std::string::npos);
    CHECK(r.text.find("Result for cpp polymorphism") != std::string::npos);
    tool.execute({{"query", "CPP polymorphism"}});     // same query (case-insensitive) => cache hit
    CHECK(counter->calls == 1);
    CHECK(tool.execute({{"query", "  "}}).text.find("Error:") == 0);
}

static void testServerHelpers() {
    section("HttpServer helpers");
    CHECK(HttpServer::originAllowed(""));
    CHECK(HttpServer::originAllowed("null"));
    CHECK(HttpServer::originAllowed("http://localhost:8080"));
    CHECK(HttpServer::originAllowed("http://127.0.0.1:3000"));
    CHECK(!HttpServer::originAllowed("http://localhost.evil.com"));
    CHECK(!HttpServer::originAllowed("https://evil.com"));

    std::vector<ChatMessage> out;
    std::string err;
    CHECK(HttpServer::parseConversation(json::parse(R"({"messages":[{"role":"user","content":"hi"}]})"), out, err));
    CHECK(out.size() == 1 && out[0].content == "hi");
    out.clear();
    CHECK(!HttpServer::parseConversation(json::parse(R"({"messages":[{"role":"assistant","content":"hi"}]})"), out, err));
    out.clear();
    CHECK(!HttpServer::parseConversation(json::parse(R"({"nothing":1})"), out, err));
    out.clear();
    CHECK(HttpServer::parseConversation(json::parse(R"({"message":"yo","history":[{"role":"user","content":"a"},{"role":"system","content":"x"}]})"), out, err));
    CHECK(out.size() == 2);  // "system" role from the browser is ignored (prompt-injection guard)
}

static void testOperators() {
    section("Operator overloading");
    std::ostringstream os;
    os << ChatMessage{"user", "hello"};
    CHECK(os.str() == "[user] hello");
    CHECK((Source{"a", "u"} == Source{"b", "u"}));
    CHECK((Source{"a", "u1"} != Source{"a", "u2"}));
}

static void testAgent() {
    section("Agent with MockProvider");

    // 1) Plain answer, streamed in one piece.
    {
        auto mock = std::make_unique<MockProvider>(std::vector<Completion>{{"Hello student!", {}, false}});
        MockProvider* m = mock.get();
        ToolRegistry reg;
        reg.add(std::make_unique<EchoTool>());
        Agent agent(std::move(mock), std::move(reg));
        Events ev;
        agent.chat({{"user", "hi"}}, ev.emitter());
        CHECK(ev.text() == "Hello student!");
        CHECK(ev.count("error") == 0);
        CHECK(m->seenMessages[0][0]["role"] == "system");
        CHECK(m->seenMessages[0][0]["content"].get<std::string>().find("VeloraAI") != std::string::npos);
        CHECK(m->seenMessages[0][1]["content"] == "hi");
        CHECK(m->seenToolCounts[0] == 1);
    }

    // 2) The model asks for a tool, we run it, then the model answers using the result.
    {
        Completion askTool;
        askTool.toolCalls.push_back({"call_1", "echo", R"({"text":"ping"})"});
        auto mock = std::make_unique<MockProvider>(std::vector<Completion>{askTool, {"Tool said: echo: ping", {}, false}});
        MockProvider* m = mock.get();
        ToolRegistry reg;
        reg.add(std::make_unique<EchoTool>());
        Agent agent(std::move(mock), std::move(reg));
        Events ev;
        agent.chat({{"user", "use the tool"}}, ev.emitter());
        CHECK(ev.count("status") == 1);
        CHECK(ev.list[0]["text"] == "Echoing");
        CHECK(ev.count("sources") == 1);
        CHECK(ev.text() == "Tool said: echo: ping");
        // second request must contain the assistant's tool call AND the tool result
        const json& second = m->seenMessages[1];
        CHECK(second[second.size() - 2]["tool_calls"][0]["function"]["name"] == "echo");
        CHECK(second.back()["role"] == "tool");
        CHECK(second.back()["content"] == "echo: ping");
        CHECK(second.back()["tool_call_id"] == "call_1");
    }

    // 3) Unknown tool and duplicate sources are handled.
    {
        Completion ask;
        ask.toolCalls.push_back({"c1", "ghost", "{}"});
        ask.toolCalls.push_back({"c2", "echo", R"({"text":"a"})"});
        Completion ask2;
        ask2.toolCalls.push_back({"c3", "echo", R"({"text":"b"})"});
        auto mock = std::make_unique<MockProvider>(std::vector<Completion>{ask, ask2, {"done", {}, false}});
        MockProvider* m = mock.get();
        ToolRegistry reg;
        reg.add(std::make_unique<EchoTool>());
        Agent agent(std::move(mock), std::move(reg));
        Events ev;
        agent.chat({{"user", "x"}}, ev.emitter());
        CHECK(ev.count("sources") == 1);          // same link twice -> shown once
        CHECK(ev.text() == "done");
        CHECK(m->seenMessages[1][m->seenMessages[1].size() - 2]["content"] == "Unknown tool: ghost");
    }

    // 4) Provider errors become friendly "error" events, never exceptions.
    {
        auto mock = std::make_unique<MockProvider>(std::vector<Completion>{});   // script empty -> NetworkError
        Agent agent(std::move(mock), ToolRegistry{});
        Events ev;
        agent.chat({{"user", "x"}}, ev.emitter());
        CHECK(ev.count("error") == 1);
        CHECK(ev.list[0]["text"] == "script exhausted");
    }

    // 5) Not configured.
    {
        Agent agent(std::make_unique<MockProvider>(std::vector<Completion>{}, false), ToolRegistry{});
        Events ev;
        agent.chat({{"user", "x"}}, ev.emitter());
        CHECK(ev.count("error") == 1);
        CHECK(!agent.isConfigured());
    }

    // 6) Tool loop never runs forever; the last round has NO tools.
    {
        std::vector<Completion> script;
        for (int i = 0; i < Agent::kMaxToolRounds; ++i) {
            Completion c;
            c.toolCalls.push_back({"c" + std::to_string(i), "echo", "{}"});
            script.push_back(c);
        }
        auto mock = std::make_unique<MockProvider>(script);
        MockProvider* m = mock.get();
        ToolRegistry reg;
        reg.add(std::make_unique<EchoTool>());
        Agent agent(std::move(mock), std::move(reg));
        Events ev;
        agent.chat({{"user", "loop"}}, ev.emitter());
        CHECK(ev.count("error") == 1);
        CHECK(m->seenToolCounts.size() == (std::size_t)Agent::kMaxToolRounds);
        CHECK(m->seenToolCounts.back() == 0);
    }

    // 7) History trimming: starts with a user message, bounded size.
    {
        auto mock = std::make_unique<MockProvider>(std::vector<Completion>{{"ok", {}, false}});
        MockProvider* m = mock.get();
        Agent agent(std::move(mock), ToolRegistry{});
        std::vector<ChatMessage> convo;
        for (int i = 0; i < 100; ++i) convo.push_back({i % 2 == 0 ? "user" : "assistant", "m" + std::to_string(i)});
        convo.push_back({"user", "last"});
        Events ev;
        agent.chat(convo, ev.emitter());
        const json& sent = m->seenMessages[0];
        CHECK(sent.size() <= Agent::kMaxMessages + 1 + 1);
        CHECK(sent[1]["role"] == "user");
        CHECK(sent.back()["content"] == "last");
    }

    // 8) "Stop" button: emit returns false -> the Agent stops quietly.
    {
        auto mock = std::make_unique<MockProvider>(std::vector<Completion>{{"text", {}, false}});
        Agent agent(std::move(mock), ToolRegistry{});
        int calls = 0;
        agent.chat({{"user", "x"}}, [&](const json&) { ++calls; return false; });
        CHECK(calls == 1);
    }
}

int main() {
    testStringUtil();
    testExpressionParser();
    testCache();
    testTools();
    testRegistry();
    testConfig();
    testExceptions();
    testSearch();
    testServerHelpers();
    testOperators();
    testAgent();

    std::cout << "\n" << (g_checks - g_failed) << "/" << g_checks << " checks passed\n";
    return g_failed == 0 ? 0 : 1;
}
