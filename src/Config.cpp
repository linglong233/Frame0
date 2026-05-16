#include "Config.h"
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

static std::string triggerKeyTypeToString(TriggerKeyType t) {
    switch (t) {
        case TriggerKeyType::MouseLeft:  return "mouseLeft";
        case TriggerKeyType::MouseRight: return "mouseRight";
        case TriggerKeyType::Keyboard:   return "keyboard";
    }
    return "mouseLeft";
}

static TriggerKeyType stringToTriggerKeyType(const std::string& s) {
    if (s == "mouseRight") return TriggerKeyType::MouseRight;
    if (s == "keyboard")   return TriggerKeyType::Keyboard;
    return TriggerKeyType::MouseLeft;
}

Config defaultConfig() {
    return {};
}

Config loadConfig(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) return defaultConfig();
    try {
        std::ifstream f(path);
        json j = json::parse(f);
        Config cfg;
        cfg.triggerKeyType = stringToTriggerKeyType(j.value("triggerKeyType", "mouseLeft"));
        cfg.triggerKeyCode = j.value("triggerKeyCode", 0u);
        cfg.displayLatencyMs = j.value("displayLatencyMs", 0.0);
        cfg.mouseLatencyMs = j.value("mouseLatencyMs", 0.0);
        cfg.fullscreen = j.value("fullscreen", true);
        return cfg;
    } catch (...) {
        return defaultConfig();
    }
}

void saveConfig(const Config& cfg, const std::filesystem::path& path) {
    std::filesystem::create_directories(path.parent_path());
    json j;
    j["triggerKeyType"] = triggerKeyTypeToString(cfg.triggerKeyType);
    j["triggerKeyCode"] = cfg.triggerKeyCode;
    j["displayLatencyMs"] = cfg.displayLatencyMs;
    j["mouseLatencyMs"] = cfg.mouseLatencyMs;
    j["fullscreen"] = cfg.fullscreen;
    std::ofstream f(path);
    f << j.dump(2);
}
