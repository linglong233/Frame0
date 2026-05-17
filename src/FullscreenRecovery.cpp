#include "FullscreenRecovery.h"

FullscreenRecovery::FullscreenRecovery(bool desiredFullscreen)
    : desiredFullscreen_(desiredFullscreen) {}

FullscreenRecoveryAction FullscreenRecovery::setDesiredFullscreen(
    bool desiredFullscreen,
    bool actualFullscreen) {
    desiredFullscreen_ = desiredFullscreen;
    return actionForActual(actualFullscreen);
}

FullscreenRecoveryAction FullscreenRecovery::onActivationChanged(
    bool active,
    bool actualFullscreen) {
    appActive_ = active;
    return actionForActual(actualFullscreen);
}

FullscreenRecoveryAction FullscreenRecovery::onActualFullscreenChanged(
    bool actualFullscreen) const {
    return actionForActual(actualFullscreen);
}

bool FullscreenRecovery::desiredFullscreen() const {
    return desiredFullscreen_;
}

bool FullscreenRecovery::isAppActive() const {
    return appActive_;
}

FullscreenRecoveryAction FullscreenRecovery::actionForActual(
    bool actualFullscreen) const {
    if (!appActive_) return FullscreenRecoveryAction::None;
    if (desiredFullscreen_ && !actualFullscreen) {
        return FullscreenRecoveryAction::RestoreFullscreen;
    }
    if (!desiredFullscreen_ && actualFullscreen) {
        return FullscreenRecoveryAction::ExitFullscreen;
    }
    return FullscreenRecoveryAction::None;
}
