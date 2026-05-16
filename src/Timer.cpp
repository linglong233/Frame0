#include "Timer.h"
#include <windows.h>

int64_t Timer::freq_ = 0;

void Timer::init() {
    LARGE_INTEGER li;
    QueryPerformanceFrequency(&li);
    freq_ = li.QuadPart;
}

int64_t Timer::now() {
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    return li.QuadPart;
}

double Timer::nowMs() {
    return static_cast<double>(now()) * 1000.0 / static_cast<double>(freq_);
}

double Timer::elapsedMs(int64_t startQPC) {
    return static_cast<double>(now() - startQPC) * 1000.0 / static_cast<double>(freq_);
}

int64_t Timer::frequency() {
    return freq_;
}
