#ifndef VELORA_TOOLS_DATETIMETOOL_H
#define VELORA_TOOLS_DATETIMETOOL_H

#include "tools/Tool.h"

// Concrete tool #2: the clock.
class DateTimeTool final : public Tool {
public:
    std::string name() const override { return "get_datetime"; }
    std::string description() const override {
        return "Current date and time. Optionally for a UTC offset such as +05:30 or -08:00.";
    }
    nlohmann::json parameters() const override;
    std::string describe(const nlohmann::json&) const override { return "Checking the date and time"; }

    // static: usable without any DateTimeTool object, e.g. by the prompt builder.
    //   DateTimeTool::now()           -> computer's local time
    //   DateTimeTool::now("+05:30")   -> time at a UTC offset
    static std::string now(const std::string& utcOffset = "");

protected:
    ToolResult run(const nlohmann::json& args) const override;
};

#endif  // VELORA_TOOLS_DATETIMETOOL_H
