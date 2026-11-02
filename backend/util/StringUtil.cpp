#include "util/StringUtil.h"

#include <cctype>
#include <map>

namespace util {

std::string lower(std::string s) {
    for (auto& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace((unsigned char)s[a])) ++a;
    while (b > a && std::isspace((unsigned char)s[b - 1])) --b;
    return s.substr(a, b - a);
}

bool startsWith(const std::string& s, const std::string& p) {
    return s.size() >= p.size() && s.compare(0, p.size(), p) == 0;
}

bool endsWith(const std::string& s, const std::string& p) {
    return s.size() >= p.size() && s.compare(s.size() - p.size(), p.size(), p) == 0;
}

std::string urlEncode(const std::string& s) {
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : s) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out += (char)c;
        } else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 15];
        }
    }
    return out;
}

std::string urlDecode(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '%' && i + 2 < s.size() && std::isxdigit((unsigned char)s[i + 1]) &&
            std::isxdigit((unsigned char)s[i + 2])) {
            out += (char)std::stoi(s.substr(i + 1, 2), nullptr, 16);
            i += 2;
        } else if (s[i] == '+') {
            out += ' ';
        } else {
            out += s[i];
        }
    }
    return out;
}

namespace {

void appendUtf8(std::string& out, unsigned long cp) {
    if (cp < 0x80) {
        out += (char)cp;
    } else if (cp < 0x800) {
        out += (char)(0xC0 | (cp >> 6));
        out += (char)(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += (char)(0xE0 | (cp >> 12));
        out += (char)(0x80 | ((cp >> 6) & 0x3F));
        out += (char)(0x80 | (cp & 0x3F));
    } else if (cp < 0x110000) {
        out += (char)(0xF0 | (cp >> 18));
        out += (char)(0x80 | ((cp >> 12) & 0x3F));
        out += (char)(0x80 | ((cp >> 6) & 0x3F));
        out += (char)(0x80 | (cp & 0x3F));
    }
}

}  // namespace

std::string decodeEntities(const std::string& s) {
    static const std::map<std::string, std::string> named = {
        {"amp", "&"},   {"lt", "<"},       {"gt", ">"},      {"quot", "\""},
        {"apos", "'"},  {"nbsp", " "},     {"ndash", "-"},   {"mdash", "-"},
        {"hellip", "..."}, {"rsquo", "'"}, {"lsquo", "'"},   {"ldquo", "\""},
        {"rdquo", "\""},   {"copy", "(c)"}, {"middot", "*"}};
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] != '&') { out += s[i]; continue; }
        size_t semi = s.find(';', i);
        if (semi == std::string::npos || semi - i > 10) { out += s[i]; continue; }
        std::string ent = s.substr(i + 1, semi - i - 1);
        if (!ent.empty() && ent[0] == '#') {
            try {
                unsigned long cp = (ent.size() > 1 && (ent[1] == 'x' || ent[1] == 'X'))
                                       ? std::stoul(ent.substr(2), nullptr, 16)
                                       : std::stoul(ent.substr(1));
                appendUtf8(out, cp);
                i = semi;
                continue;
            } catch (...) {
            }
        } else {
            auto it = named.find(ent);
            if (it != named.end()) { out += it->second; i = semi; continue; }
        }
        out += s[i];
    }
    return out;
}

std::string stripTags(const std::string& s) {
    std::string out;
    bool inTag = false;
    for (char c : s) {
        if (c == '<') inTag = true;
        else if (c == '>') inTag = false;
        else if (!inTag) out += c;
    }
    return trim(decodeEntities(out));
}

std::string truncate(const std::string& s, size_t max) {
    if (s.size() <= max) return s;
    size_t cut = max;
    while (cut > 0 && ((unsigned char)s[cut] & 0xC0) == 0x80) --cut;  // stay on a UTF-8 boundary
    return s.substr(0, cut) + "\n...[truncated]";
}

std::string htmlToText(const std::string& html) {
    std::string low = lower(html);
    std::string work;
    work.reserve(html.size());

    // Whole elements whose content is never visible text.
    static const char* dropped[] = {"script", "style", "noscript", "svg", "template", "head"};
    size_t i = 0;
    while (i < html.size()) {
        if (html[i] != '<') { work += html[i++]; continue; }

        if (low.compare(i, 4, "<!--") == 0) {                 // comment
            size_t e = low.find("-->", i + 4);
            i = e == std::string::npos ? html.size() : e + 3;
            continue;
        }

        bool skipped = false;
        for (const char* tag : dropped) {
            std::string open = std::string("<") + tag;
            if (low.compare(i, open.size(), open) == 0 && (i + open.size() < low.size())) {
                char next = low[i + open.size()];
                if (next == '>' || next == ' ' || next == '\n' || next == '\t' || next == '/') {
                    std::string close = std::string("</") + tag + ">";
                    size_t e = low.find(close, i);
                    i = e == std::string::npos ? html.size() : e + close.size();
                    skipped = true;
                    break;
                }
            }
        }
        if (skipped) continue;

        size_t e = html.find('>', i);
        if (e == std::string::npos) break;
        std::string tag = low.substr(i + 1, e - i - 1);
        std::string name;
        for (char c : tag) {
            if (std::isalnum((unsigned char)c) || c == '/') name += c;
            else break;
        }
        static const char* blocks[] = {"p",  "/p",  "div", "/div", "br",  "li",      "/li",
                                       "tr", "/tr", "h1",  "/h1",  "h2",  "/h2",     "h3",
                                       "/h3", "h4", "/h4", "section", "/section", "article",
                                       "/article", "ul", "/ul", "ol", "/ol", "table", "/table",
                                       "pre", "blockquote"};
        for (const char* b : blocks)
            if (name == b) { work += '\n'; break; }
        if (name == "td" || name == "th") work += ' ';
        i = e + 1;
    }

    std::string text = decodeEntities(work);

    // Collapse whitespace but keep paragraph breaks.
    std::string out;
    int newlines = 0;
    bool space = false;
    for (char c : text) {
        if (c == '\n') { ++newlines; space = false; continue; }
        if (c == ' ' || c == '\t' || c == '\r') { space = true; continue; }
        if (newlines) { out += newlines > 1 ? "\n\n" : "\n"; newlines = 0; space = false; }
        else if (space) { out += ' '; space = false; }
        out += c;
    }
    return trim(out);
}

}  // namespace util
