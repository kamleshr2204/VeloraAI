// Tool: the ABSTRACT BASE CLASS for everything the AI can "do" besides talking
// (search the web, calculate, read a page, tell the time ...).
//
// OOP ideas used here:
//   * Abstract class + pure virtual functions  - name(), description(), parameters(),
//     describe() and run() are promises that every concrete tool must fulfil.
//   * Polymorphism   - the Agent only holds `const Tool*` pointers. It calls tool->execute()
//     and C++ picks the right child's run() at run time (late binding via the vtable).
//   * TEMPLATE METHOD pattern - execute() is a normal (non-virtual) function that wraps
//     the virtual run() with error handling. Child classes only write run(); they get the
//     safety net for free and cannot forget it.
//   * protected    - run() is hidden from outsiders but visible to child classes.
//   * Deleted copy - a Tool is an identity-bearing object (it may own caches, sockets...),
//     so copying it makes no sense. We forbid it with  = delete.
#ifndef VELORA_TOOLS_TOOL_H
#define VELORA_TOOLS_TOOL_H

#include "core/Types.h"

#include <nlohmann/json.hpp>

#include <string>

class Tool {
public:
    virtual ~Tool() = default;  // virtual destructor: always do this in a base class

    Tool(const Tool&) = delete;
    Tool& operator=(const Tool&) = delete;

    // ---- the promises every tool must keep (pure virtual) ----
    virtual std::string name() const = 0;          // e.g. "calculator"
    virtual std::string description() const = 0;   // tells the AI WHEN to use it
    virtual nlohmann::json parameters() const = 0; // JSON-schema of the arguments
    virtual std::string describe(const nlohmann::json& args) const = 0;  // text for the UI

    // ---- ready-made behaviour shared by all tools ----
    // Runs the tool; NEVER throws. Problems are turned into text the AI can read.
    ToolResult execute(const nlohmann::json& args) const;

    // The "function definition" sent to the AI model so it knows this tool exists.
    nlohmann::json definition() const;

protected:
    Tool() = default;  // only child classes can be constructed

    // The real work. May throw (ToolError for expected problems).
    virtual ToolResult run(const nlohmann::json& args) const = 0;

    // Helper: read a string argument safely ("" if missing or not a string).
    static std::string stringArg(const nlohmann::json& args, const char* key);
};

#endif  // VELORA_TOOLS_TOOL_H
