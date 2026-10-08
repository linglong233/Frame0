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

// Foul state ignores Raw Input triggers: retry is driven explicitly by the UI
// layer calling retryRound(), so repeated clicks do not bounce the user out of
// the foul screen before they see it.
TEST_F(TestSessionTest, FoulStateIgnoresTrigger) {
    session_.start();
    session_.onTrigger(Timer::now());
    ASSERT_EQ(session_.state(), AppState::Foul);

    // Repeated triggers while in Foul must not change state.
    session_.onTrigger(Timer::now());
    session_.onTrigger(Timer::now());
    EXPECT_EQ(session_.state(), AppState::Foul);

    // Explicit retry returns to Waiting.
    session_.retryRound();
    EXPECT_EQ(session_.state(), AppState::Waiting);
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

// Compensation model: the recorded time is the raw QPC interval minus the
// scanout residual (one refresh period in vsync modes, supplied by the
// renderer) and the user-configured display/mouse latencies.
TEST_F(TestSessionTest, CompensationSubtractedFromReactionTime) {
    session_.start();
    session_.setScanoutCompensationMs(16.667);
    session_.setDisplayLatency(5.0);
    session_.setMouseLatency(1.0);

    int64_t freq = Timer::frequency();
    int64_t t0 = 1000 * freq;
    session_.setStimulusTimeForTest(t0);
    session_.onTrigger(t0 + freq / 10);  // 100 ms raw interval

    EXPECT_NEAR(session_.lastReactionMs(), 100.0 - 16.667 - 5.0 - 1.0, 0.01);
    EXPECT_NEAR(session_.appliedCompensationMs(), 16.667 + 5.0 + 1.0, 0.001);
}

// P0 contract: update() transitions Waiting -> Stimulus WITHOUT capturing the
// stimulus timestamp. The timestamp is supplied separately by setStimulusTime()
// (called by the renderer right after the stimulus frame is presented). Until
// then, needsStimulusTime() stays true and no reaction can be computed.
TEST_F(TestSessionTest, UpdateDoesNotCaptureStimulusTime) {
    session_.start();
    EXPECT_FALSE(session_.needsStimulusTime());  // Waiting state

    session_.setTargetDelayTicksForTest(0);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    session_.update();

    ASSERT_EQ(session_.state(), AppState::Stimulus);
    EXPECT_TRUE(session_.needsStimulusTime());   // update() did not set it

    int64_t presentQpc = Timer::now();
    session_.setStimulusTime(presentQpc);
    EXPECT_FALSE(session_.needsStimulusTime());  // now captured

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    session_.onTrigger(Timer::now());
    EXPECT_EQ(session_.state(), AppState::Result);
    EXPECT_GT(session_.lastReactionMs(), 15.0);
}
