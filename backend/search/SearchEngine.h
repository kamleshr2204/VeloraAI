// SearchEngine: an ABSTRACT BASE CLASS (an "interface").
//
// OOP ideas used here:
//   * Abstraction   - the rest of the program only knows "a search engine can search()".
//                     It does not care HOW (Tavily API, DuckDuckGo page, Wikipedia).
//   * Pure virtual function  (= 0)  - a promise every child class MUST keep. A class with
//                     at least one pure virtual function cannot be instantiated.
//   * Virtual destructor - REQUIRED in a base class. Without it, deleting a child object
//                     through a base pointer is undefined behaviour (the child part would
//                     never be cleaned up).
//   * Polymorphism  - WebSearchTool holds  std::vector<std::unique_ptr<SearchEngine>>  and
//                     calls engine->search(); the correct child version runs at run time.
#ifndef VELORA_SEARCH_SEARCHENGINE_H
#define VELORA_SEARCH_SEARCHENGINE_H

#include <string>
#include <vector>

struct SearchHit {
    std::string title;
    std::string url;
    std::string snippet;
};

struct SearchResponse {
    std::vector<SearchHit> hits;
    std::string quickAnswer;  // some engines (Tavily) also return a short direct answer
};

class SearchEngine {
public:
    virtual ~SearchEngine() = default;

    virtual std::string name() const = 0;

    // Look something up. Must not throw; return an empty response if nothing was found.
    virtual SearchResponse search(const std::string& query, int maxResults) const = 0;
};

#endif  // VELORA_SEARCH_SEARCHENGINE_H
