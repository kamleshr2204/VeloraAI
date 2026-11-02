// ToolRegistry: a "phone book" of all tools.
//
// OOP ideas used here:
//   * Polymorphic container - one vector holds Tool objects of DIFFERENT child types,
//     stored as  std::unique_ptr<Tool>  (a pointer to the base class). Storing the
//     objects themselves would "slice" them (cut off the child part), so we must use
//     pointers. unique_ptr also cleans up automatically (RAII) - no manual delete.
//   * Factory       - createDefault() builds a ready-to-use registry (the one place that
//     knows which concrete tools exist).
//   * Move-only ownership - add() takes the unique_ptr "by value" and the caller must
//     std::move() it in: ownership is transferred, never shared by accident.
#ifndef VELORA_TOOLS_TOOLREGISTRY_H
#define VELORA_TOOLS_TOOLREGISTRY_H

#include "tools/Tool.h"

#include <map>
#include <memory>
#include <vector>

class Config;

class ToolRegistry {
public:
    // Adds a tool. Throws ToolError if a tool with the same name already exists.
    void add(std::unique_ptr<Tool> tool);

    // Finds a tool by name, or returns nullptr. (The registry keeps ownership.)
    const Tool* find(const std::string& name) const;

    // JSON list of all tool definitions, to send to the AI model.
    nlohmann::json definitions() const;

    std::size_t size() const { return tools_.size(); }

    // Factory: calculator + clock + page reader + web search (engines chosen from Config).
    static ToolRegistry createDefault(const Config& config);

private:
    std::vector<std::unique_ptr<Tool>> tools_;  // owns the tools
    std::map<std::string, Tool*> byName_;       // fast lookup (does NOT own)
};

#endif  // VELORA_TOOLS_TOOLREGISTRY_H
