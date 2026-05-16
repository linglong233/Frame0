#include <gtest/gtest.h>
#include "Timer.h"
#include <windows.h>

TEST(TimerTest, FrequencyIsPositive) {
    Timer::init();
    EXPECT_GT(Timer::frequency(), 0);
}

TEST(TimerTest, NowReturnsIncreasingValues) {
    Timer::init();
    int64_t t1 = Timer::now();
    int64_t t2 = Timer::now();
    EXPECT_GE(t2, t1);
}

TEST(TimerTest, NowMsReturnsPositiveValue) {
    Timer::init();
    double ms = Timer::nowMs();
    EXPECT_GT(ms, 0.0);
}

TEST(TimerTest, ElapsedMsReturnsPositiveForLaterTime) {
    Timer::init();
    int64_t start = Timer::now();
    Sleep(10);
    double elapsed = Timer::elapsedMs(start);
    EXPECT_GE(elapsed, 8.0);
    EXPECT_LE(elapsed, 50.0);
}
