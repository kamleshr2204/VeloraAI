// WebClient: downloads web pages for our tools.
//
// OOP ideas used here:
//   * A class with a CONSTRUCTOR that sets its behaviour. WebClient(true) is a "safe"
//     client that refuses private/local addresses (so a malicious web page cannot trick
//     the bot into reading things inside your own computer or network).
//   * explicit constructor - stops accidental conversions like  WebClient c = true;
//   * Private helper data - blockPrivate_ is hidden; behaviour is reached through get().
#ifndef VELORA_UTIL_WEBCLIENT_H
#define VELORA_UTIL_WEBCLIENT_H

#include <string>
#include <utility>
#include <vector>

struct WebPage {
    int status = 0;            // HTTP status, 0 = no answer
    std::string contentType;   // lower-case Content-Type header
    std::string body;
    std::string finalUrl;      // after following redirects
    std::string error;         // non-empty when something went wrong
    bool truncated = false;    // body was cut at maxBytes
};

using HeaderList = std::vector<std::pair<std::string, std::string>>;

class WebClient {
public:
    // blockPrivateAddresses = true  -> refuse localhost, 10.x.x.x, 192.168.x.x ...
    explicit WebClient(bool blockPrivateAddresses = false);

    // GET a page, following up to 5 redirects. Never throws: check page.error.
    WebPage get(std::string url, std::size_t maxBytes, const HeaderList& extraHeaders = {}) const;

    // POST a JSON body (used for the Tavily search API). Never throws: check page.error.
    WebPage postJson(const std::string& url, const std::string& jsonBody,
                     const HeaderList& extraHeaders = {}, int readTimeoutSeconds = 30) const;

private:
    bool blockPrivate_;
};

#endif  // VELORA_UTIL_WEBCLIENT_H
