// Small "plain data" types shared by the whole backend.
//
// OOP ideas used here:
//   * struct  - a class whose members are public by default. Good for simple
//               bundles of data that have no rules to protect.
//   * operator overloading - we teach C++ how to use  <<  and  ==  with our types.
#ifndef VELORA_CORE_TYPES_H
#define VELORA_CORE_TYPES_H

#include <ostream>
#include <string>
#include <vector>

// One message in the conversation. role is "user" or "assistant".
struct ChatMessage {
    std::string role;
    std::string content;
};

// A web page the answer was based on (shown as a clickable chip in the UI).
struct Source {
    std::string title;
    std::string url;
};

// What a tool gives back: text for the AI to read + links for the human to click.
struct ToolResult {
    std::string text;
    std::vector<Source> sources;
};

// ---- operator overloading --------------------------------------------------
// With this, we can write:  std::cout << message;
inline std::ostream& operator<<(std::ostream& os, const ChatMessage& m) {
    return os << "[" << m.role << "] " << m.content;
}

// Two sources are "the same" when they point to the same URL.
inline bool operator==(const Source& a, const Source& b) { return a.url == b.url; }
inline bool operator!=(const Source& a, const Source& b) { return !(a == b); }

#endif  // VELORA_CORE_TYPES_H
