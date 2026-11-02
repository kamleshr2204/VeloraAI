#include "search/DuckDuckGoSearch.h"

#include "util/StringUtil.h"

using namespace util;

std::vector<SearchHit> DuckDuckGoSearch::parseResults(const std::string& html, int maxResults) {
    std::vector<SearchHit> hits;

    size_t pos = 0;
    while ((int)hits.size() < maxResults) {
        size_t a = html.find("class=\"result__a\"", pos);
        if (a == std::string::npos) break;
        size_t hrefPos = html.find("href=\"", a);
        if (hrefPos == std::string::npos) break;
        hrefPos += 6;
        size_t hrefEnd = html.find('"', hrefPos);
        size_t titleStart = html.find('>', hrefEnd);
        size_t titleEnd = html.find("</a>", titleStart);
        if (hrefEnd == std::string::npos || titleStart == std::string::npos ||
            titleEnd == std::string::npos)
            break;

        std::string href = decodeEntities(html.substr(hrefPos, hrefEnd - hrefPos));
        std::string title = stripTags(html.substr(titleStart + 1, titleEnd - titleStart - 1));

        // The snippet belongs to this result if it appears before the next result.
        size_t nextResult = html.find("class=\"result__a\"", titleEnd);
        size_t s = html.find("class=\"result__snippet\"", titleEnd);
        std::string snippet;
        if (s != std::string::npos && (nextResult == std::string::npos || s < nextResult)) {
            size_t sStart = html.find('>', s);
            size_t sEnd = html.find("</a>", sStart);
            if (sStart != std::string::npos && sEnd != std::string::npos)
                snippet = stripTags(html.substr(sStart + 1, sEnd - sStart - 1));
        }
        pos = titleEnd;

        // DuckDuckGo wraps the real address:  //duckduckgo.com/l/?uddg=<encoded url>&rut=...
        std::string url = href;
        size_t u = href.find("uddg=");
        if (u != std::string::npos) {
            size_t e = href.find('&', u);
            url = urlDecode(href.substr(u + 5, e == std::string::npos ? std::string::npos : e - u - 5));
        } else if (startsWith(url, "//")) {
            url = "https:" + url;
        }
        if (!startsWith(url, "http") || url.find("duckduckgo.com/y.js") != std::string::npos)
            continue;  // skip ads / junk
        hits.push_back({title, url, snippet});
    }
    return hits;
}

SearchResponse DuckDuckGoSearch::search(const std::string& query, int maxResults) const {
    SearchResponse out;
    WebPage page = web_.get("https://html.duckduckgo.com/html/?q=" + urlEncode(query), 600000);
    if (!page.error.empty() || page.status != 200) return out;
    out.hits = parseResults(page.body, maxResults);
    return out;
}
