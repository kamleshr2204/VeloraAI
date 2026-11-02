#include "tools/Tool.h"

#include "core/Exceptions.h"

ToolResult Tool::execute(const nlohmann::json& args) const {
    try {
        return run(args);  // virtual call: runs the CHILD's version
    } catch (const ToolError& e) {
        return {std::string("Error: ") + e.what(), {}};  // an expected problem
    } catch (const std::exception& e) {
        return {"Tool '" + name() + "' failed: " + e.what(), {}};  // an unexpected one
    }
}

nlohmann::json Tool::definition() const {
    return {{"type", "function"},
            {"function",
             {{"name", name()}, {"description", description()}, {"parameters", parameters()}}}};
}

std::string Tool::stringArg(const nlohmann::json& args, const char* key) {
    if (args.is_object() && args.contains(key) && args[key].is_string())
        return args[key].get<std::string>();
    return "";
}
