#pragma once
#include <cstdint>

class Timer {
public:
    static void init();
    static int64_t now();
    static double nowMs();
    static double elapsedMs(int64_t startQPC);
    static int64_t frequency();
private:
    static int64_t freq_;
};
