#pragma once
#include <vector>
#include <string>
#include <filesystem>

struct SessionResult {
    std::string timestamp;
    std::vector<double> rounds;
    double median = 0;
    double mean = 0;
    double stddev = 0;
    int refreshRate = 0;
    int pollingRate = 0;
    bool fullscreen = false;
};

void appendHistory(const std::filesystem::path& path, const SessionResult& result);
std::vector<SessionResult> loadHistory(const std::filesystem::path& path);
