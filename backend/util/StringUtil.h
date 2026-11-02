// Small text helpers. They have no data of their own to protect, so they are
// plain functions grouped in a namespace instead of a class.
// (Rule of thumb: use a class when data + behaviour belong together.
//  Use free functions when you just transform inputs into outputs.)
#ifndef VELORA_UTIL_STRINGUTIL_H
#define VELORA_UTIL_STRINGUTIL_H

#include <string>

namespace util {

std::string lower(std::string s);
std::string trim(const std::string& s);
bool startsWith(const std::string& s, const std::string& prefix);
bool endsWith(const std::string& s, const std::string& suffix);

std::string urlEncode(const std::string& s);
std::string urlDecode(const std::string& s);

std::string decodeEntities(const std::string& s);    // "&amp;" -> "&"
std::string stripTags(const std::string& s);          // "<b>Hi</b>" -> "Hi"
std::string htmlToText(const std::string& html);      // whole web page -> readable text

// Cut a string to at most `max` bytes without breaking a UTF-8 character.
std::string truncate(const std::string& s, size_t max);

}  // namespace util

#endif  // VELORA_UTIL_STRINGUTIL_H
