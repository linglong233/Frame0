#pragma once
#include <string>
#include <filesystem>

enum class TriggerKeyType { MouseLeft, MouseRight, Keyboard };

struct Config {
    TriggerKeyType triggerKeyType = TriggerKeyType::MouseLeft;
    unsigned triggerKeyCode = 0;
    double displayLatencyMs = 0.0;
    double mouseLatencyMs = 0.0;
    int pollingRate = 0;  // user-configured mouse polling rate (Hz), 0 = unknown
    bool fullscreen = true;
};

Config loadConfig(const std::filesystem::path& path);
void saveConfig(const Config& cfg, const std::filesystem::path& path);
Config defaultConfig();
