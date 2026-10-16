// Unit tests for the volume SFX_Play_Pitch plays at.

#include "Sfx.hpp"
#include <gtest/gtest.h>

static const FP kFull = FP_FromInteger(1);
static const FP kHalf = FP_FromDouble(0.5);

TEST(SfxPitchVolume, FullScaleKeepsTheGivenVolume)
{
    EXPECT_EQ(relive::SfxPitchVolume(90, 40, kFull), 40);
}

TEST(SfxPitchVolume, NoVolumeUsesTheDefault)
{
    EXPECT_EQ(relive::SfxPitchVolume(90, 0, kFull), 90);
}

TEST(SfxPitchVolume, HalfScaleScalesTheGivenVolumeNotTheDefault)
{
    // This used to be 2 * 90 / 3 = 60 whatever the volume was
    EXPECT_EQ(relive::SfxPitchVolume(90, 30, kHalf), 20);
    EXPECT_EQ(relive::SfxPitchVolume(90, 115, kHalf), 76);
}

TEST(SfxPitchVolume, HalfScaleWithNoVolumeScalesTheDefault)
{
    EXPECT_EQ(relive::SfxPitchVolume(90, 0, kHalf), 60);
}
