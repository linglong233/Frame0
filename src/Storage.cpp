#include "Storage.h"
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

static SessionResult jsonToResult(const json& j) {
    SessionResult r;
    r.timestamp = j.value("timestamp", "");
    r.rounds = j.value("rounds", std::vector<double>{});
    r.median = j.value("median", 0.0);
    r.mean = j.value("mean", 0.0);
    r.stddev = j.value("stddev", 0.0);
    r.refreshRate = j.value("refreshRate", 0);
    r.pollingRate = j.value("pollingRate", 0);
    r.fullscreen = j.value("fullscreen", false);
    return r;
}

static json resultToJson(const SessionResult& r) {
    json j;
    j["timestamp"] = r.timestamp;
    j["rounds"] = r.rounds;
    j["median"] = r.median;
    j["mean"] = r.mean;
    j["stddev"] = r.stddev;
    j["refreshRate"] = r.refreshRate;
    j["pollingRate"] = r.pollingRate;
    j["fullscreen"] = r.fullscreen;
    return j;
}

std::vector<SessionResult> loadHistory(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) return {};
    try {
        std::ifstream f(path);
        json arr = json::parse(f);
        if (!arr.is_array()) return {};
        std::vector<SessionResult> results;
        for (const auto& item : arr) {
            results.push_back(jsonToResult(item));
        }
        return results;
    } catch (...) {
        return {};
    }
}

void appendHistory(const std::filesystem::path& path, const SessionResult& result) {
    std::filesystem::create_directories(path.parent_path());
    auto existing = loadHistory(path);
    existing.push_back(result);
    if (existing.size() > MAX_HISTORY) {
        existing.erase(existing.begin(),
                       existing.begin() + (existing.size() - MAX_HISTORY));
    }
    try {
        json arr = json::array();
        for (const auto& r : existing) {
            arr.push_back(resultToJson(r));
        }
        std::ofstream f(path);
        f << arr.dump(2);
    } catch (...) {
        // Write failures (disk full, permissions) must not crash the app.
    }
}
