#pragma once
#include <string>
#include <filesystem>

enum class TriggerKeyType { MouseLeft, MouseRight, Keyboard };

struct Config {
    TriggerKeyType triggerKeyType = TriggerKeyType::MouseLeft;
    unsigned triggerKeyCode = 0;
    double displayLatencyMs = 0.0;
    double mouseLatencyMs = 0.0;
    bool fullscreen = true;
};

Config loadConfig(const std::filesystem::path& path);
void saveConfig(const Config& cfg, const std::filesystem::path& path);
Config defaultConfig();
