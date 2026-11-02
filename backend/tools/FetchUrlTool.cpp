#include "tools/FetchUrlTool.h"

#include "core/Exceptions.h"
#include "util/StringUtil.h"

using json = nlohmann::json;

json FetchUrlTool::parameters() const {
    return {{"type", "object"},
            {"properties", {{"url", {{"type", "string"}, {"description", "Full http(s) URL."}}}}},
            {"required", json::array({"url"})}};
}

std::string FetchUrlTool::describe(const json& args) const {
    return "Reading " + util::truncate(stringArg(args, "url"), 80);
}

ToolResult FetchUrlTool::run(const json& args) const {
    std::string url = util::trim(stringArg(args, "url"));
    if (url.empty()) throw ToolError("'url' is required.");

    WebPage page = web_.get(url, 1500000);
    if (!page.error.empty()) throw ToolError(page.error);
    if (page.status < 200 || page.status >= 300)
        throw ToolError("the page returned HTTP " + std::to_string(page.status) + ".");

    const std::string& ct = page.contentType;
    bool isText = ct.empty() || ct.find("text/") != std::string::npos ||
                  ct.find("json") != std::string::npos || ct.find("xml") != std::string::npos;
    if (!isText)
        throw ToolError("cannot read this file type (" + ct + "). Only web pages and text are supported.");

    std::string title, text;
    if (ct.find("html") != std::string::npos || ct.empty()) {
        std::string low = util::lower(page.body);
        size_t a = low.find("<title");
        if (a != std::string::npos) {
            size_t s = low.find('>', a), e = low.find("</title>", a);
            if (s != std::string::npos && e != std::string::npos && e > s)
                title = util::stripTags(page.body.substr(s + 1, e - s - 1));
        }
        text = util::htmlToText(page.body);
    } else {
        text = page.body;
    }

    if (text.size() < 40)
        return {"The page has almost no readable text (it may need JavaScript or a login).", {}};

    ToolResult result;
    result.sources.push_back({title.empty() ? url : title, page.finalUrl});
    result.text = "Page: " + (title.empty() ? url : title) + "\nURL: " + page.finalUrl + "\n\n" +
                  util::truncate(text, 12000);
    return result;
}
