#ifndef VELORA_SEARCH_TAVILYSEARCH_H
#define VELORA_SEARCH_TAVILYSEARCH_H

#include "search/SearchEngine.h"
#include "util/WebClient.h"

// Child class #1: uses the Tavily search API (needs a free API key).
//   "class TavilySearch : public SearchEngine"  means  "TavilySearch IS-A SearchEngine".
class TavilySearch : public SearchEngine {
public:
    explicit TavilySearch(std::string apiKey);

    // "override" asks the compiler to check that we really are replacing a base-class function.
    std::string name() const override { return "Tavily"; }
    SearchResponse search(const std::string& query, int maxResults) const override;

private:
    std::string apiKey_;
    WebClient web_;  // composition: a TavilySearch HAS-A WebClient
};

#endif  // VELORA_SEARCH_TAVILYSEARCH_H
