#pragma once

enum class FullscreenRecoveryAction {
    None,
    RestoreFullscreen,
    ExitFullscreen
};

class FullscreenRecovery {
public:
    explicit FullscreenRecovery(bool desiredFullscreen = false);

    FullscreenRecoveryAction setDesiredFullscreen(bool desiredFullscreen,
                                                  bool actualFullscreen);
    FullscreenRecoveryAction onActivationChanged(bool active,
                                                 bool actualFullscreen);
    FullscreenRecoveryAction onActualFullscreenChanged(bool actualFullscreen) const;

    bool desiredFullscreen() const;
    bool isAppActive() const;

private:
    FullscreenRecoveryAction actionForActual(bool actualFullscreen) const;

    bool desiredFullscreen_ = false;
    bool appActive_ = true;
};
