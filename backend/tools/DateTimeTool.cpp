#include "tools/DateTimeTool.h"

#include "util/StringUtil.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>

using json = nlohmann::json;

json DateTimeTool::parameters() const {
    return {{"type", "object"},
            {"properties", {{"utc_offset", {{"type", "string"}, {"description", "Optional, e.g. +05:30"}}}}},
            {"required", json::array()}};
}

ToolResult DateTimeTool::run(const json& args) const {
    return {now(stringArg(args, "utc_offset")), {}};
}

std::string DateTimeTool::now(const std::string& utcOffset) {
    using namespace std::chrono;
    std::time_t t = system_clock::to_time_t(system_clock::now());

    // Try to understand an offset like "+05:30", "-08:00" or "UTC+5".
    int offsetMin = 0;
    bool haveOffset = false;
    std::string o = util::trim(utcOffset);
    if (util::startsWith(util::lower(o), "utc")) o = util::trim(o.substr(3));
    if (!o.empty() && (o[0] == '+' || o[0] == '-')) {
        int sign = o[0] == '-' ? -1 : 1;
        int h = 0, m = 0;
        if (std::sscanf(o.c_str() + 1, "%d:%d", &h, &m) >= 1 && h <= 14) {
            offsetMin = sign * (h * 60 + m);
            haveOffset = true;
        }
    }

    auto utcFormat = [](std::time_t when, const char* format) {
        std::tm tm{};
#ifdef _WIN32
        gmtime_s(&tm, &when);
#else
        gmtime_r(&when, &tm);
#endif
        char buf[128];
        std::strftime(buf, sizeof buf, format, &tm);
        return std::string(buf);
    };

    if (haveOffset) {
        char zone[16];
        std::snprintf(zone, sizeof zone, "UTC%c%02d:%02d", offsetMin < 0 ? '-' : '+',
                      std::abs(offsetMin) / 60, std::abs(offsetMin) % 60);
        return utcFormat(t + offsetMin * 60, "%A, %d %B %Y, %H:%M:%S ") + zone;
    }

    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &t);
#else
    localtime_r(&t, &local);
#endif
    char buf[160];
    std::strftime(buf, sizeof buf, "%A, %d %B %Y, %H:%M:%S (computer's local time, %Z)", &local);
    return std::string(buf) + "; UTC is " + utcFormat(t, "%Y-%m-%d %H:%M:%S");
}
