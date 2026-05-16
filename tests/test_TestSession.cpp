#include <gtest/gtest.h>
#include "TestSession.h"
#include "Timer.h"
#include <thread>

class TestSessionTest : public ::testing::Test {
protected:
    void SetUp() override {
        Timer::init();
        session_.setStateChangedCallback([](AppState) {});
    }
    TestSession session_;
};

TEST_F(TestSessionTest, StartsInIdleState) {
    EXPECT_EQ(session_.state(), AppState::Idle);
}

TEST_F(TestSessionTest, StartTransitionsToWaiting) {
    session_.start();
    EXPECT_EQ(session_.state(), AppState::Waiting);
}

TEST_F(TestSessionTest, TriggerBeforeStimulusIsFoul) {
    session_.start();
    session_.onTrigger(Timer::now());
    EXPECT_EQ(session_.state(), AppState::Foul);
}

TEST_F(TestSessionTest, TriggerAfterStimulusGivesResult) {
    session_.start();
    session_.setStimulusTimeForTest(Timer::now());
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    session_.onTrigger(Timer::now());

    EXPECT_EQ(session_.state(), AppState::Result);
    EXPECT_GT(session_.lastReactionMs(), 15.0);
    EXPECT_LT(session_.lastReactionMs(), 100.0);
}

TEST_F(TestSessionTest, FiveRoundsReachSummary) {
    session_.start();
    for (int i = 0; i < 5; i++) {
        std::this_thread::sleep_for(std::chrono::seconds(7));
        session_.setStimulusTimeForTest(Timer::now());
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        session_.onTrigger(Timer::now());

        if (i < 4) {
            EXPECT_EQ(session_.state(), AppState::Result);
            session_.proceedToNextRound();
            EXPECT_EQ(session_.state(), AppState::Waiting);
        }
    }
    EXPECT_EQ(session_.state(), AppState::Summary);
    EXPECT_EQ(session_.currentRoundIndex(), 5);
}

TEST_F(TestSessionTest, StatsCalculation) {
    session_.start();
    session_.setRoundsForTest({200.0, 210.0, 205.0, 195.0, 215.0});
    session_.calculateStats();

    EXPECT_NEAR(session_.medianMs(), 205.0, 0.1);
    EXPECT_NEAR(session_.meanMs(), 205.0, 0.1);
}
