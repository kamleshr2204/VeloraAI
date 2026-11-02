#ifndef VELORA_TOOLS_WEBSEARCHTOOL_H
#define VELORA_TOOLS_WEBSEARCHTOOL_H

#include "search/SearchEngine.h"
#include "tools/Tool.h"
#include "util/Cache.h"

#include <memory>
#include <vector>

// Concrete tool #4: searches the live web.
//
// OOP ideas used here:
//   * Composition + polymorphism - a WebSearchTool HAS a list of SearchEngine objects
//     (Tavily, DuckDuckGo, Wikipedia ...) and tries them in order. It never needs to know
//     which concrete engine is which. Adding a new engine = writing a new child class,
//     with ZERO changes in this file. (This is the "Open/Closed Principle".)
//   * unique_ptr  - the tool OWNS its engines. When the tool dies, the engines die with it.
//   * Class template in action - Cache<std::string, ToolResult> remembers recent answers.
//   * mutable     - run() is const ("does not change the tool's visible behaviour"), but the
//     cache inside must be updated, so the cache member is marked mutable.
class WebSearchTool final : public Tool {
public:
    using EngineList = std::vector<std::unique_ptr<SearchEngine>>;

    explicit WebSearchTool(EngineList engines);

    std::string name() const override { return "web_search"; }
    std::string description() const override;
    nlohmann::json parameters() const override;
    std::string describe(const nlohmann::json& args) const override;

protected:
    ToolResult run(const nlohmann::json& args) const override;

private:
    EngineList engines_;
    mutable Cache<std::string, ToolResult> cache_;
};

#endif  // VELORA_TOOLS_WEBSEARCHTOOL_H
