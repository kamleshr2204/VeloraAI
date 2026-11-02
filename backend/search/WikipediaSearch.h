#ifndef VELORA_SEARCH_WIKIPEDIASEARCH_H
#define VELORA_SEARCH_WIKIPEDIASEARCH_H

#include "search/SearchEngine.h"
#include "util/WebClient.h"

// Child class #3: Wikipedia's public search API. Our last-resort engine.
class WikipediaSearch : public SearchEngine {
public:
    WikipediaSearch() = default;

    std::string name() const override { return "Wikipedia"; }
    SearchResponse search(const std::string& query, int maxResults) const override;

private:
    WebClient web_{false};
};

#endif  // VELORA_SEARCH_WIKIPEDIASEARCH_H
