#include "search/WikipediaSearch.h"

#include "util/StringUtil.h"

#include <nlohmann/json.hpp>

#include <algorithm>

using json = nlohmann::json;

SearchResponse WikipediaSearch::search(const std::string& query, int maxResults) const {
    SearchResponse out;
    std::string url =
        "https://en.wikipedia.org/w/api.php?action=query&generator=search&gsrlimit=" +
        std::to_string(std::min(maxResults, 5)) + "&gsrsearch=" + util::urlEncode(query) +
        "&prop=extracts&exintro=1&explaintext=1&exchars=900&format=json&utf8=1";

    WebPage page = web_.get(url, 400000);
    if (!page.error.empty() || page.status != 200) return out;

    try {
        json j = json::parse(page.body);
        if (!j.contains("query") || !j["query"].contains("pages")) return out;

        std::vector<std::pair<int, SearchHit>> ordered;
        for (auto& [id, p] : j["query"]["pages"].items()) {
            std::string title = p.value("title", "");
            std::string slug = title;
            std::replace(slug.begin(), slug.end(), ' ', '_');
            ordered.push_back({p.value("index", 99),
                               {title + " - Wikipedia",
                                "https://en.wikipedia.org/wiki/" + util::urlEncode(slug),
                                p.value("extract", "")}});
        }
        std::sort(ordered.begin(), ordered.end(),
                  [](const auto& a, const auto& b) { return a.first < b.first; });
        for (auto& o : ordered) out.hits.push_back(o.second);
    } catch (...) {
        // Unreadable answer: return what we have (nothing).
    }
    return out;
}
