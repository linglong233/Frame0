#include <gtest/gtest.h>
#include "FullscreenRecovery.h"

TEST(FullscreenRecoveryTest, RestoresDesiredFullscreenAfterInactiveDxgiExit) {
    FullscreenRecovery recovery(true);

    EXPECT_EQ(recovery.onActivationChanged(false, true), FullscreenRecoveryAction::None);
    EXPECT_EQ(recovery.onActualFullscreenChanged(false), FullscreenRecoveryAction::None);

    EXPECT_TRUE(recovery.desiredFullscreen());
    EXPECT_EQ(recovery.onActivationChanged(true, false), FullscreenRecoveryAction::RestoreFullscreen);
}

TEST(FullscreenRecoveryTest, DoesNotRestoreWhenUserRequestedWindowedMode) {
    FullscreenRecovery recovery(false);

    EXPECT_EQ(recovery.onActivationChanged(false, false), FullscreenRecoveryAction::None);
    EXPECT_EQ(recovery.onActivationChanged(true, false), FullscreenRecoveryAction::None);
    EXPECT_FALSE(recovery.desiredFullscreen());
}

TEST(FullscreenRecoveryTest, ActiveDxgiExitRestoresOnlyWhenFullscreenIsDesired) {
    FullscreenRecovery recovery(true);

    EXPECT_EQ(recovery.onActualFullscreenChanged(false), FullscreenRecoveryAction::RestoreFullscreen);

    recovery.setDesiredFullscreen(false, false);
    EXPECT_EQ(recovery.onActualFullscreenChanged(false), FullscreenRecoveryAction::None);
}
