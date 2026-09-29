#include <gtest/gtest.h>
#include "Sound/PsxSoundEngine.hpp"

// libsnd's voice volume, from the voice setup at 0x80076d94 in the AE PS1 executable
// (SLES_014.80). The expected values come from running that code in a MIPS interpreter.
TEST(PsxSoundEngine, LibsndVoiceVolume)
{
    struct Case final
    {
        bool mSeqNote;
        s32 mVelocity, mVabVol, mProgVol, mToneVol, mTonePan, mProgPan, mChannelPan, mSeqVolLeft, mSeqVolRight;
        s32 mLeft, mRight;
    };
    const Case cases[] = {
        {false, 103, 106, 44, 93, 95, 22, 112, 27, 41, 340, 982},
        {false, 100, 94, 125, 7, 120, 11, 78, 100, 43, 44, 90},
        {false, 45, 46, 23, 65, 8, 18, 21, 4, 115, 194, 2},
        {false, 7, 101, 82, 109, 15, 76, 32, 54, 12, 321, 47},
        {false, 55, 117, 43, 9, 96, 51, 88, 25, 52, 47, 126},
        {false, 102, 72, 4, 40, 51, 83, 34, 86, 109, 50, 31},
        {false, 87, 29, 96, 19, 57, 20, 68, 93, 75, 270, 82},
        {false, 29, 117, 70, 27, 11, 75, 3, 3, 23, 332, 3},
        {false, 78, 50, 63, 92, 20, 71, 22, 114, 23, 1264, 157},
        {false, 86, 58, 99, 78, 10, 83, 47, 81, 77, 1693, 286},
        {false, 75, 32, 52, 36, 8, 80, 52, 45, 76, 210, 28},
        {false, 116, 2, 101, 86, 43, 66, 124, 6, 106, 5, 86},
        {false, 4, 15, 90, 35, 32, 35, 66, 70, 101, 10, 2},
        {false, 44, 22, 59, 124, 1, 45, 81, 112, 57, 324, 5},
        {true, 126, 122, 57, 105, 86, 70, 56, 12, 18, 6, 32},
        {false, 94, 40, 52, 79, 76, 76, 95, 42, 118, 323, 972},
    };
    for (const Case& c : cases)
    {
        const PsxSoundEngine::VoiceVolume v = PsxSoundEngine::LibsndVoiceVolume(c.mSeqNote, c.mSeqNote, c.mVelocity, c.mVabVol, c.mProgVol, c.mToneVol, c.mTonePan, c.mProgPan,
            c.mChannelPan, c.mSeqVolLeft, c.mSeqVolRight);
        EXPECT_EQ(v.mLeft, c.mLeft);
        EXPECT_EQ(v.mRight, c.mRight);
    }

    // Everything at full volume and centred is the largest register value, either way
    EXPECT_EQ(PsxSoundEngine::LibsndVoiceVolume(true, true, 127, 127, 127, 127, 64, 64, 64, 127, 127).mLeft, 0x3FFF);
    EXPECT_EQ(PsxSoundEngine::LibsndVoiceVolume(false, false, 127, 127, 127, 127, 64, 64, 64, 127, 127).mLeft, 0x3FFF);
    // A SEQ's note is squared: velocity 66 gives 4424, a sound effect's 8514
    EXPECT_EQ(PsxSoundEngine::LibsndVoiceVolume(true, true, 66, 127, 127, 127, 64, 64, 64, 127, 127).mLeft, 4424);
    EXPECT_EQ(PsxSoundEngine::LibsndVoiceVolume(false, false, 66, 127, 127, 127, 64, 64, 64, 127, 127).mLeft, 8514);
}

// AO's libsnd squares a sound effect too: its voice setup (0x8007c73c in the AO PS1 executable,
// SLES_006.64) has no check for one. The expected values come from running it in a MIPS
// interpreter, which agreed with LibsndVoiceVolume for all of 3000 random SEQ notes and sound
// effects.
TEST(PsxSoundEngine, LibsndVoiceVolumeAoSfx)
{
    struct Case final
    {
        s32 mVelocity, mVabVol, mProgVol, mToneVol, mTonePan, mProgPan, mChannelPan, mSeqVolLeft, mSeqVolRight;
        s32 mLeft, mRight;
    };
    const Case cases[] = {
        {44, 71, 115, 93, 18, 24, 93, 14, 54, 78, 3},
        {46, 88, 65, 111, 15, 31, 57, 15, 101, 206, 2},
        {87, 72, 68, 112, 30, 78, 46, 26, 48, 333, 66},
        {71, 83, 91, 50, 80, 119, 116, 92, 76, 0, 173},
        {83, 79, 122, 93, 73, 18, 30, 107, 42, 983, 24},
        {74, 120, 68, 47, 89, 127, 116, 17, 23, 0, 194},
        {118, 74, 123, 47, 88, 5, 118, 90, 43, 4, 3},
        {97, 111, 95, 57, 101, 100, 127, 20, 42, 0, 822},
    };
    for (const Case& c : cases)
    {
        // A sound effect ignores the SEQ volume
        const PsxSoundEngine::VoiceVolume v = PsxSoundEngine::LibsndVoiceVolume(false, true, c.mVelocity, c.mVabVol, c.mProgVol, c.mToneVol, c.mTonePan, c.mProgPan,
            c.mChannelPan, c.mSeqVolLeft, c.mSeqVolRight);
        EXPECT_EQ(v.mLeft, c.mLeft);
        EXPECT_EQ(v.mRight, c.mRight);
    }

    EXPECT_EQ(PsxSoundEngine::LibsndVoiceVolume(false, true, 66, 127, 127, 127, 64, 64, 64, 127, 127).mLeft, 4424);
}

TEST(PsxSoundEngine, SfxVelocityAndPan)
{
    s32 velocity = 0;
    s32 pan = 0;
    PsxSoundEngine::SfxVelocityAndPan(100, 100, velocity, pan);
    EXPECT_EQ(velocity, 100);
    EXPECT_EQ(pan, 64);
    PsxSoundEngine::SfxVelocityAndPan(100, 50, velocity, pan);
    EXPECT_EQ(velocity, 100);
    EXPECT_EQ(pan, 32);
    PsxSoundEngine::SfxVelocityAndPan(50, 100, velocity, pan);
    EXPECT_EQ(velocity, 100);
    EXPECT_EQ(pan, 95);
}
