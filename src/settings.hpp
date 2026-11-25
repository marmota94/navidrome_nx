#pragma once
#include <jansson.h>
#include <cstdio>
#include <cerrno>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstdlib>
#include <unistd.h>
#include "bubbles.hpp"
#include "atomic_file.hpp"
#include "equalizer.hpp"

struct Settings {
    bool showClock = true;
    bool sidebarCollapsed = false;
    bool remoteEnabled = false;
    bool cacheSongs = false;
    int background = 0;
    int backgroundOpacity = 80;
    int backgroundStyle = 0;
    int bubbleSpeed = 2;
    int bubbleCount = DefaultBubbleCount;
    int columns = 4;
    int visualizer = 0;
    EqualizerGains equalizer{};
};
inline Settings preferences;
inline constexpr const char* SettingsPath = "sdmc:/switch/navidrome_nx/settings.json";

inline Settings parseSettings(json_t* root) {
    if (!json_is_object(root)) throw std::runtime_error("settings_format");
    Settings result;
    auto boolean = [root](const char* key, bool fallback) {
        auto value = json_object_get(root, key);
        if (!value) return fallback;
        if (!json_is_boolean(value)) throw std::runtime_error("settings_format");
        return json_is_true(value);
    };
    auto integer = [root](const char* key, int fallback, int min, int max) {
        auto value = json_object_get(root, key);
        if (!value) return fallback;
        if (!json_is_integer(value) || json_integer_value(value) < min || json_integer_value(value) > max)
            throw std::runtime_error("settings_format");
        return static_cast<int>(json_integer_value(value));
    };
    result.showClock = boolean("showClock", true);
    result.sidebarCollapsed = boolean("sidebarCollapsed", false);
    result.remoteEnabled = boolean("remoteEnabled", false);
    result.cacheSongs = boolean("cacheSongs", false);
    result.background = integer("background", 0, 0, 6);
    result.backgroundOpacity = integer("backgroundOpacity", 80, 0, 100);
    result.backgroundStyle = integer("backgroundStyle", 0, 0, 1);
    result.bubbleCount = integer("bubbleCount", DefaultBubbleCount, 0, MaxBubbleCount);
    result.bubbleSpeed = integer("bubbleSpeed", 2, 0, 3);
    result.columns = integer("columns", 4, 4, 6);
    result.visualizer = integer("visualizer", 0, 0, 2);
    result.equalizer = {{
        integer("eqBass", 0, -EqualizerGainLimit, EqualizerGainLimit),
        integer("eqMid", 0, -EqualizerGainLimit, EqualizerGainLimit),
        integer("eqTreble", 0, -EqualizerGainLimit, EqualizerGainLimit)
    }};
    return result;
}
inline Settings loadSettings(const char* path = SettingsPath) {
    errno = 0;
    auto file = std::fopen(path, "rb");
    const auto backup = std::string(path) + ".bak";
    if (!file && errno == ENOENT) file = std::fopen(backup.c_str(), "rb");
    if (!file) {
        if (errno == ENOENT) return {};
        throw std::runtime_error("settings_read");
    }
    json_t* root = json_loadf(file, JSON_REJECT_DUPLICATES, nullptr);
    std::fclose(file);
    if (!root) throw std::runtime_error("settings_format");
    try { auto result = parseSettings(root); json_decref(root); return result; }
    catch (...) { json_decref(root); throw; }
}
inline void saveSettings(const Settings& value, const char* path = SettingsPath) {
    auto root = json_pack("{s:b,s:b,s:b,s:b,s:i,s:i,s:i,s:i,s:i,s:i,s:i,s:i,s:i,s:i}",
        "showClock", value.showClock, "sidebarCollapsed", value.sidebarCollapsed,
        "remoteEnabled", value.remoteEnabled, "cacheSongs", value.cacheSongs, "background", value.background,
        "bubbleSpeed", value.bubbleSpeed, "columns", value.columns, "backgroundStyle", value.backgroundStyle, "bubbleCount", value.bubbleCount, "backgroundOpacity", value.backgroundOpacity, "visualizer", value.visualizer,
        "eqBass", value.equalizer[0], "eqMid", value.equalizer[1], "eqTreble", value.equalizer[2]);
    if (!root) throw std::runtime_error("settings_write");
    try { parseSettings(root); } catch (...) { json_decref(root); throw; }
    char* encoded = json_dumps(root, JSON_INDENT(2));
    json_decref(root);
    if (!encoded) throw std::runtime_error("settings_write");
    const std::string payload(encoded); std::free(encoded);
    writeAtomicFile(path, payload);
}
inline double bubbleRate(int speed) {
    constexpr double rates[] = {0, 0.5, 1, 2};
    return rates[speed];
}

inline int adjustSetting(int value, int minimum, int maximum, int direction) {
    return direction < 0 ? (value <= minimum ? maximum : value - 1)
                         : (value >= maximum ? minimum : value + 1);
}
