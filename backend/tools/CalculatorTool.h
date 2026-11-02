#ifndef VELORA_TOOLS_CALCULATORTOOL_H
#define VELORA_TOOLS_CALCULATORTOOL_H

#include "tools/Tool.h"

// Concrete tool #1. Language models are bad at arithmetic, so they ask us instead.
//
// "final" says: nobody may inherit from CalculatorTool any further.
// "override" on each function says: "I am replacing the virtual function of Tool".
class CalculatorTool final : public Tool {
public:
    std::string name() const override { return "calculator"; }
    std::string description() const override;
    nlohmann::json parameters() const override;
    std::string describe(const nlohmann::json& args) const override;

protected:
    ToolResult run(const nlohmann::json& args) const override;
};

#endif  // VELORA_TOOLS_CALCULATORTOOL_H
