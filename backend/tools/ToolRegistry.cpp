#include "tools/ToolRegistry.h"

#include "core/Config.h"
#include "core/Exceptions.h"
#include "search/DuckDuckGoSearch.h"
#include "search/TavilySearch.h"
#include "search/WikipediaSearch.h"
#include "tools/CalculatorTool.h"
#include "tools/DateTimeTool.h"
#include "tools/FetchUrlTool.h"
#include "tools/WebSearchTool.h"

void ToolRegistry::add(std::unique_ptr<Tool> tool) {
    if (!tool) throw ToolError("Cannot register an empty tool.");
    const std::string name = tool->name();
    if (byName_.count(name)) throw ToolError("A tool named '" + name + "' is already registered.");

    byName_[name] = tool.get();          // remember the address...
    tools_.push_back(std::move(tool));   // ...then hand over ownership
}

const Tool* ToolRegistry::find(const std::string& name) const {
    auto it = byName_.find(name);
    return it == byName_.end() ? nullptr : it->second;
}

nlohmann::json ToolRegistry::definitions() const {
    nlohmann::json list = nlohmann::json::array();
    for (const auto& tool : tools_) list.push_back(tool->definition());  // polymorphic call
    return list;
}

ToolRegistry ToolRegistry::createDefault(const Config& config) {
    ToolRegistry registry;

    // Search engines, best first. Each one is a different child of SearchEngine.
    WebSearchTool::EngineList engines;
    if (!config.tavilyKey().empty()) engines.push_back(std::make_unique<TavilySearch>(config.tavilyKey()));
    engines.push_back(std::make_unique<DuckDuckGoSearch>());
    engines.push_back(std::make_unique<WikipediaSearch>());

    // Each tool is a different child of Tool.
    registry.add(std::make_unique<WebSearchTool>(std::move(engines)));
    registry.add(std::make_unique<FetchUrlTool>());
    registry.add(std::make_unique<CalculatorTool>());
    registry.add(std::make_unique<DateTimeTool>());
    return registry;
}
