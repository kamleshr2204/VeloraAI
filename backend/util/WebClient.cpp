#include "util/WebClient.h"

#include "util/StringUtil.h"

#include "httplib.h"

#include <cstring>

using util::endsWith;
using util::lower;
using util::startsWith;

namespace {

const char* kUserAgent =
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/124.0 Safari/537.36 VeloraAI/1.0";

// ---- URL parsing -----------------------------------------------------------

struct ParsedUrl {
    std::string scheme, host, path = "/";
    int port = 0;
    std::string origin() const { return scheme + "://" + host + ":" + std::to_string(port); }
};

bool parseUrl(const std::string& url, ParsedUrl& out) {
    size_t p = url.find("://");
    if (p == std::string::npos) return false;
    out.scheme = lower(url.substr(0, p));
    if (out.scheme != "http" && out.scheme != "https") return false;

    size_t hostStart = p + 3;
    size_t pathStart = url.find_first_of("/?#", hostStart);
    std::string authority = url.substr(
        hostStart, pathStart == std::string::npos ? std::string::npos : pathStart - hostStart);
    std::string rest = pathStart == std::string::npos ? "/" : url.substr(pathStart);
    size_t hash = rest.find('#');
    if (hash != std::string::npos) rest = rest.substr(0, hash);
    if (rest.empty() || rest[0] != '/') rest = "/" + rest;
    out.path = rest;

    if (authority.rfind('@') != std::string::npos) return false;  // no user:pass@ in URLs
    out.port = out.scheme == "https" ? 443 : 80;
    size_t colon = authority.rfind(':');
    if (colon != std::string::npos && authority.find(']') == std::string::npos) {
        try {
            out.port = std::stoi(authority.substr(colon + 1));
        } catch (...) {
            return false;
        }
        authority = authority.substr(0, colon);
    }
    out.host = lower(authority);
    return !out.host.empty() && out.port > 0 && out.port < 65536;
}

// ---- "is this address inside my own computer / network?" -------------------

bool isPrivateAddress(const sockaddr* sa) {
    if (sa->sa_family == AF_INET) {
        auto* a = (const sockaddr_in*)sa;
        unsigned long ip = ntohl(a->sin_addr.s_addr);
        unsigned b1 = (ip >> 24) & 255, b2 = (ip >> 16) & 255;
        return b1 == 0 || b1 == 10 || b1 == 127 || (b1 == 169 && b2 == 254) ||
               (b1 == 172 && b2 >= 16 && b2 <= 31) || (b1 == 192 && b2 == 168) ||
               (b1 == 100 && b2 >= 64 && b2 <= 127) || b1 >= 224;
    }
    if (sa->sa_family == AF_INET6) {
        auto* a = (const sockaddr_in6*)sa;
        const unsigned char* b = a->sin6_addr.s6_addr;
        bool loopback = true;
        for (int i = 0; i < 15; ++i)
            if (b[i]) loopback = false;
        if (loopback && (b[15] == 1 || b[15] == 0)) return true;
        if ((b[0] & 0xFE) == 0xFC) return true;                  // fc00::/7
        if (b[0] == 0xFE && (b[1] & 0xC0) == 0x80) return true;  // fe80::/10
        bool mapped = true;                                      // ::ffff:a.b.c.d
        for (int i = 0; i < 10; ++i)
            if (b[i]) mapped = false;
        if (mapped && b[10] == 0xFF && b[11] == 0xFF) {
            sockaddr_in v4{};
            v4.sin_family = AF_INET;
            std::memcpy(&v4.sin_addr, b + 12, 4);
            return isPrivateAddress((const sockaddr*)&v4);
        }
    }
    return false;
}

bool hostIsSafe(const std::string& host) {
    if (host == "localhost" || endsWith(host, ".localhost") || endsWith(host, ".local") ||
        endsWith(host, ".internal"))
        return false;
    addrinfo hints{};
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    if (getaddrinfo(host.c_str(), nullptr, &hints, &res) != 0 || !res) return false;
    bool ok = true;
    for (addrinfo* p = res; p; p = p->ai_next)
        if (isPrivateAddress(p->ai_addr)) ok = false;
    freeaddrinfo(res);
    return ok;
}

}  // namespace

// ---------------------------------------------------------------------------

WebClient::WebClient(bool blockPrivateAddresses) : blockPrivate_(blockPrivateAddresses) {}

WebPage WebClient::get(std::string url, std::size_t maxBytes, const HeaderList& extra) const {
    WebPage page;
    for (int hop = 0; hop < 5; ++hop) {
        ParsedUrl u;
        if (!parseUrl(url, u)) {
            page.error = "Invalid or unsupported URL (use http/https).";
            return page;
        }
        if (blockPrivate_ && !hostIsSafe(u.host)) {
            page.error = "Blocked: that address is private, local or could not be resolved.";
            return page;
        }

        httplib::Client cli(u.origin());
        cli.set_connection_timeout(8, 0);
        cli.set_read_timeout(15, 0);
        cli.set_follow_location(false);  // we follow redirects ourselves, re-checking each hop

        httplib::Headers headers = {
            {"User-Agent", kUserAgent},
            {"Accept", "text/html,application/xhtml+xml,text/plain,application/json;q=0.9,*/*;q=0.5"},
            {"Accept-Language", "en-US,en;q=0.9"},
            {"Accept-Encoding", "identity"}};
        for (const auto& h : extra) headers.emplace(h.first, h.second);

        std::string location;
        page = WebPage{};
        page.finalUrl = url;
        auto res = cli.Get(
            u.path, headers,
            [&](const httplib::Response& r) {
                page.status = r.status;
                page.contentType = lower(r.get_header_value("Content-Type"));
                location = r.get_header_value("Location");
                return r.status < 300 || r.status >= 400;  // skip bodies of redirects
            },
            [&](const char* data, size_t len) {
                if (page.body.size() + len > maxBytes) {
                    page.body.append(data, maxBytes - page.body.size());
                    page.truncated = true;
                    return false;
                }
                page.body.append(data, len);
                return true;
            });

        if (page.status >= 300 && page.status < 400 && !location.empty()) {
            if (startsWith(location, "//")) location = u.scheme + ":" + location;
            else if (startsWith(location, "/")) location = u.scheme + "://" + u.host + location;
            else if (!startsWith(lower(location), "http")) location = u.scheme + "://" + u.host + "/" + location;
            url = location;
            continue;  // next hop
        }
        if (!res && !page.truncated && page.status == 0)
            page.error = "Could not connect to " + u.host + ".";
        return page;
    }
    page.error = "Too many redirects.";
    return page;
}

WebPage WebClient::postJson(const std::string& url, const std::string& jsonBody,
                            const HeaderList& extra, int readTimeoutSeconds) const {
    WebPage page;
    page.finalUrl = url;
    ParsedUrl u;
    if (!parseUrl(url, u)) {
        page.error = "Invalid or unsupported URL (use http/https).";
        return page;
    }
    if (blockPrivate_ && !hostIsSafe(u.host)) {
        page.error = "Blocked: that address is private, local or could not be resolved.";
        return page;
    }

    httplib::Client cli(u.origin());
    cli.set_connection_timeout(8, 0);
    cli.set_read_timeout(readTimeoutSeconds, 0);

    httplib::Headers headers = {{"User-Agent", kUserAgent}};
    for (const auto& h : extra) headers.emplace(h.first, h.second);

    auto res = cli.Post(u.path, headers, jsonBody, "application/json");
    if (!res) {
        page.error = "Could not connect to " + u.host + ".";
        return page;
    }
    page.status = res->status;
    page.contentType = lower(res->get_header_value("Content-Type"));
    page.body = res->body;
    return page;
}
