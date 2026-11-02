#include "search/TavilySearch.h"

#include "util/StringUtil.h"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

TavilySearch::TavilySearch(std::string apiKey) : apiKey_(std::move(apiKey)), web_(false) {}

SearchResponse TavilySearch::search(const std::string& query, int maxResults) const {
    SearchResponse out;
    json body = {{"query", query},
                 {"max_results", maxResults},
                 {"include_answer", true},
                 {"search_depth", "basic"}};

    WebPage page = web_.postJson("https://api.tavily.com/search", body.dump(),
                                 {{"Authorization", "Bearer " + apiKey_}});
    if (!page.error.empty() || page.status != 200) return out;

    try {
        json j = json::parse(page.body);
        if (j.contains("answer") && j["answer"].is_string()) out.quickAnswer = j["answer"];
        for (const auto& r : j.value("results", json::array()))
            out.hits.push_back({r.value("title", ""), r.value("url", ""),
                                util::truncate(r.value("content", ""), 700)});
    } catch (...) {
        // Unreadable answer: behave as "no results" so the next engine can try.
    }
    return out;
}
