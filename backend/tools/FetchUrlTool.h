#ifndef VELORA_TOOLS_FETCHURLTOOL_H
#define VELORA_TOOLS_FETCHURLTOOL_H

#include "tools/Tool.h"
#include "util/WebClient.h"

// Concrete tool #3: opens a web page and returns its readable text.
class FetchUrlTool final : public Tool {
public:
    FetchUrlTool() : web_(true) {}  // true = block private/local addresses (safety!)

    std::string name() const override { return "fetch_url"; }
    std::string description() const override {
        return "Open a web page and read its text. Use after web_search when snippets are not "
               "enough, or when the user gives you a link.";
    }
    nlohmann::json parameters() const override;
    std::string describe(const nlohmann::json& args) const override;

protected:
    ToolResult run(const nlohmann::json& args) const override;

private:
    WebClient web_;
};

#endif  // VELORA_TOOLS_FETCHURLTOOL_H
