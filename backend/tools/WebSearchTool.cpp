#include "tools/WebSearchTool.h"

#include "core/Exceptions.h"
#include "util/StringUtil.h"

#include <algorithm>
#include <sstream>

using json = nlohmann::json;

WebSearchTool::WebSearchTool(EngineList engines)
    : engines_(std::move(engines)),     // "move" = take over the engines, no copying
      cache_(100, std::chrono::seconds(300)) {}  // keep up to 100 answers for 5 minutes

std::string WebSearchTool::description() const {
    return "Search the live internet. Use it for anything recent or changing (news, prices, scores, "
           "weather, releases, who currently holds a role), for facts you are not sure about, and "
           "whenever the user asks you to look something up. Returns titles, links and snippets.";
}

json WebSearchTool::parameters() const {
    return {{"type", "object"},
            {"properties",
             {{"query", {{"type", "string"}, {"description", "A focused search query."}}},
              {"max_results", {{"type", "integer"}, {"description", "1-8, default 6."}}}}},
            {"required", json::array({"query"})}};
}

std::string WebSearchTool::describe(const json& args) const {
    return "Searching the web: " + util::truncate(stringArg(args, "query"), 80);
}

ToolResult WebSearchTool::run(const json& args) const {
    std::string query = util::trim(stringArg(args, "query"));
    if (query.empty()) throw ToolError("'query' is required.");
    int max = std::clamp(args.value("max_results", 6), 1, 8);

    const std::string cacheKey = util::lower(query) + "|" + std::to_string(max);
    if (auto cached = cache_.get(cacheKey)) return *cached;

    // Ask each engine in turn; the first one that finds something wins.
    // engine->search() is a VIRTUAL call, so each engine runs its own code.
    SearchResponse found;
    for (const auto& engine : engines_) {
        found = engine->search(query, max);
        if (!found.hits.empty()) break;
    }

    if (found.hits.empty()) {
        return {"No results found (the search service may be unreachable). "
                "Say so honestly and answer from your own knowledge if you can.",
                {}};
    }

    ToolResult result;
    std::ostringstream out;
    if (!found.quickAnswer.empty()) out << "Quick answer from the search engine: " << found.quickAnswer << "\n\n";
    int n = 1;
    for (const auto& h : found.hits) {
        out << "[" << n++ << "] " << h.title << "\n" << h.url << "\n" << h.snippet << "\n\n";
        result.sources.push_back({h.title, h.url});
    }
    out << "Use fetch_url on the most relevant link if the snippets are not enough.";
    result.text = out.str();

    cache_.put(cacheKey, result);
    return result;
}
