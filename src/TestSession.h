#pragma once
#include <vector>
#include <random>
#include <cstdint>
#include <functional>

enum class AppState {
    Idle,
    Waiting,
    Stimulus,
    Foul,
    Result,
    Summary,
    Menu,
    Settings
};

class TestSession {
public:
    static constexpr int ROUNDS_PER_SESSION = 5;

    using StateChangedCallback = std::function<void(AppState newState)>;

    void setStateChangedCallback(StateChangedCallback cb);

    void start();
    void retryRound();
    void onTrigger(int64_t qpcTime);
    void onEscape();
    void proceedToNextRound();
    void update();

    // Stimulus time is captured by the renderer right after the stimulus frame
    // is presented (see Renderer::presentStimulus / getLastPresentTimeQPC).
    void setStimulusTime(int64_t qpc);
    bool needsStimulusTime() const;

    AppState state() const;
    double lastReactionMs() const;
    int currentRoundIndex() const;
    const std::vector<double>& roundTimes() const;

    double medianMs() const;
    double meanMs() const;
    double stddevMs() const;

    void setDisplayLatency(double ms);
    void setMouseLatency(double ms);
    // Auto-derived submit->scanout residual (one refresh period in vsync
    // modes), supplied by the renderer at session start.
    void setScanoutCompensationMs(double ms);
    // Total latency compensation applied to each round (for the summary UI).
    double appliedCompensationMs() const;

    void transitionTo(AppState newState);

    // Test helpers
    void setStimulusTimeForTest(int64_t qpc);
    void setRoundsForTest(const std::vector<double>& times);
    void setTargetDelayTicksForTest(int64_t ticks);
    void calculateStats();

private:
    int64_t generateRandomDelayTicks();

    AppState state_ = AppState::Idle;
    std::vector<double> roundTimes_;
    int currentRoundIndex_ = 0;

    int64_t stimulusQPC_ = 0;
    int64_t waitingStartQPC_ = 0;
    int64_t targetDelayTicks_ = 0;
    bool stimulusTimeCaptured_ = false;

    double displayLatencyMs_ = 0;
    double mouseLatencyMs_ = 0;
    double scanoutCompensationMs_ = 0;
    double lastReactionMs_ = 0;

    double medianMs_ = 0;
    double meanMs_ = 0;
    double stddevMs_ = 0;

    std::mt19937 rng_{ std::random_device{}() };
    std::uniform_int_distribution<int> delayDist_{ 2000, 6000 };

    StateChangedCallback onStateChanged_;
};
