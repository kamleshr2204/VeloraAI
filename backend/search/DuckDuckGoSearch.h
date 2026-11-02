#ifndef VELORA_SEARCH_DUCKDUCKGOSEARCH_H
#define VELORA_SEARCH_DUCKDUCKGOSEARCH_H

#include "search/SearchEngine.h"
#include "util/WebClient.h"

// Child class #2: reads DuckDuckGo's simple HTML result page (no API key needed).
class DuckDuckGoSearch : public SearchEngine {
public:
    DuckDuckGoSearch() = default;

    std::string name() const override { return "DuckDuckGo"; }
    SearchResponse search(const std::string& query, int maxResults) const override;

    // A "static" member function belongs to the class, not to an object, so tests can call
    //   DuckDuckGoSearch::parseResults(html, 5)   without creating any object or using the internet.
    static std::vector<SearchHit> parseResults(const std::string& html, int maxResults);

private:
    WebClient web_{false};
};

#endif  // VELORA_SEARCH_DUCKDUCKGOSEARCH_H
