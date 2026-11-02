#include "tools/CalculatorTool.h"

#include "tools/ExpressionParser.h"
#include "util/StringUtil.h"

using json = nlohmann::json;

std::string CalculatorTool::description() const {
    return "Exact math. Always use this for arithmetic, percentages, unit maths, compound interest "
           "etc. instead of calculating in your head. Supports + - * / % ^ ! ( ), pi, e and "
           "sqrt, sin, cos, tan, asin, acos, atan, ln, log (base 10), log2, exp, abs, floor, ceil, "
           "round, pow, min, max. Trig uses radians (use rad(x) to convert degrees).";
}

json CalculatorTool::parameters() const {
    return {{"type", "object"},
            {"properties",
             {{"expression", {{"type", "string"}, {"description", "e.g. 1200*(1+0.07)^5"}}}}},
            {"required", json::array({"expression"})}};
}

std::string CalculatorTool::describe(const json& args) const {
    return "Calculating " + util::truncate(stringArg(args, "expression"), 80);
}

ToolResult CalculatorTool::run(const json& args) const {
    std::string expression = stringArg(args, "expression");
    try {
        return {expression + " = " + ExpressionParser::evaluateToString(expression), {}};
    } catch (const std::exception& e) {
        // A maths mistake is not a crash: tell the AI what was wrong so it can retry.
        return {std::string("Calculator error: ") + e.what(), {}};
    }
}
