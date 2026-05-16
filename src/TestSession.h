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
    void onTrigger(int64_t qpcTime);
    void onEscape();
    void proceedToNextRound();
    void update();

    AppState state() const;
    double lastReactionMs() const;
    int currentRoundIndex() const;
    const std::vector<double>& roundTimes() const;

    double medianMs() const;
    double meanMs() const;
    double stddevMs() const;

    void setDisplayLatency(double ms);
    void setMouseLatency(double ms);

    // Test helpers
    void setStimulusTimeForTest(int64_t qpc);
    void setRoundsForTest(const std::vector<double>& times);
    void calculateStats();

private:
    void transitionTo(AppState newState);
    int64_t generateRandomDelayTicks();

    AppState state_ = AppState::Idle;
    std::vector<double> roundTimes_;
    int currentRoundIndex_ = 0;

    int64_t stimulusQPC_ = 0;
    int64_t waitingStartQPC_ = 0;
    int64_t targetDelayTicks_ = 0;

    double displayLatencyMs_ = 0;
    double mouseLatencyMs_ = 0;
    double lastReactionMs_ = 0;

    double medianMs_ = 0;
    double meanMs_ = 0;
    double stddevMs_ = 0;

    std::mt19937 rng_{ std::random_device{}() };
    std::uniform_int_distribution<int> delayDist_{ 2000, 6000 };

    StateChangedCallback onStateChanged_;
};
