#include "TestSession.h"
#include "Timer.h"
#include <algorithm>
#include <cmath>
#include <numeric>

void TestSession::setStateChangedCallback(StateChangedCallback cb) {
    onStateChanged_ = std::move(cb);
}

void TestSession::transitionTo(AppState newState) {
    state_ = newState;
    if (onStateChanged_) onStateChanged_(newState);
}

AppState TestSession::state() const { return state_; }
double TestSession::lastReactionMs() const { return lastReactionMs_; }
int TestSession::currentRoundIndex() const { return currentRoundIndex_; }
const std::vector<double>& TestSession::roundTimes() const { return roundTimes_; }

void TestSession::setDisplayLatency(double ms) { displayLatencyMs_ = ms; }
void TestSession::setMouseLatency(double ms) { mouseLatencyMs_ = ms; }

void TestSession::start() {
    roundTimes_.clear();
    currentRoundIndex_ = 0;
    lastReactionMs_ = 0;
    medianMs_ = meanMs_ = stddevMs_ = 0;
    waitingStartQPC_ = Timer::now();
    targetDelayTicks_ = generateRandomDelayTicks();
    transitionTo(AppState::Waiting);
}

void TestSession::retryRound() {
    waitingStartQPC_ = Timer::now();
    targetDelayTicks_ = generateRandomDelayTicks();
    transitionTo(AppState::Waiting);
}

void TestSession::onTrigger(int64_t qpcTime) {
    if (state_ == AppState::Waiting) {
        transitionTo(AppState::Foul);
        return;
    }
    if (state_ == AppState::Stimulus) {
        double rawMs = static_cast<double>(qpcTime - stimulusQPC_) * 1000.0 / static_cast<double>(Timer::frequency());
        lastReactionMs_ = rawMs - displayLatencyMs_ - mouseLatencyMs_;
        if (lastReactionMs_ < 0) lastReactionMs_ = 0;
        roundTimes_.push_back(lastReactionMs_);
        currentRoundIndex_++;

        if (currentRoundIndex_ >= ROUNDS_PER_SESSION) {
            calculateStats();
            transitionTo(AppState::Summary);
        } else {
            transitionTo(AppState::Result);
        }
    }
}

void TestSession::onEscape() {
    if (state_ == AppState::Waiting || state_ == AppState::Foul) {
        transitionTo(AppState::Idle);
    }
}

void TestSession::proceedToNextRound() {
    if (state_ != AppState::Result) return;
    waitingStartQPC_ = Timer::now();
    targetDelayTicks_ = generateRandomDelayTicks();
    transitionTo(AppState::Waiting);
}

void TestSession::update() {
    if (state_ != AppState::Waiting) return;
    int64_t elapsed = Timer::now() - waitingStartQPC_;
    if (elapsed >= targetDelayTicks_) {
        stimulusQPC_ = Timer::now();
        transitionTo(AppState::Stimulus);
    }
}

int64_t TestSession::generateRandomDelayTicks() {
    int delayMs = delayDist_(rng_);
    return static_cast<int64_t>(delayMs) * Timer::frequency() / 1000;
}

void TestSession::calculateStats() {
    if (roundTimes_.empty()) return;

    auto sorted = roundTimes_;
    std::sort(sorted.begin(), sorted.end());

    size_t n = sorted.size();
    if (n % 2 == 0) {
        medianMs_ = (sorted[n / 2 - 1] + sorted[n / 2]) / 2.0;
    } else {
        medianMs_ = sorted[n / 2];
    }

    double sum = std::accumulate(sorted.begin(), sorted.end(), 0.0);
    meanMs_ = sum / static_cast<double>(n);

    double sqSum = 0;
    for (double v : sorted) {
        sqSum += (v - meanMs_) * (v - meanMs_);
    }
    stddevMs_ = std::sqrt(sqSum / static_cast<double>(n));
}

double TestSession::medianMs() const { return medianMs_; }
double TestSession::meanMs() const { return meanMs_; }
double TestSession::stddevMs() const { return stddevMs_; }

// Test helpers
void TestSession::setStimulusTimeForTest(int64_t qpc) {
    stimulusQPC_ = qpc;
    state_ = AppState::Stimulus;
}

void TestSession::setRoundsForTest(const std::vector<double>& times) {
    roundTimes_ = times;
    currentRoundIndex_ = static_cast<int>(times.size());
}
