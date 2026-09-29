// Unit tests for the emulated PS1 SPU (Sound/Spu). The expected numbers come from the psx-spx
// "Sound Processing Unit (SPU)" formulas: closed forms where the formula has one, otherwise the
// result of stepping the psx-spx pseudocode by hand (the comments say which).

#include "Sound/Spu/PsxSpu.hpp"
#include "Sound/SoundGold.hpp"
#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

using Phase = PsxSpuEnvelope::Phase;

// ADSR2 with a sustain that holds (shift 1Fh, step 3: never steps) and the given release bits
static u16 HoldingAdsr2(u16 releaseBits)
{
    return static_cast<u16>(0x1FC0 | releaseBits);
}

// Ticks from key on until the level reaches the peak (0x7FFF), then until the decay reaches the
// sustain level
struct AdsrTiming final
{
    u32 mToPeak = 0;
    u32 mToSustain = 0;
    s16 mSustainLevel = 0;
};

static AdsrTiming TimeAttackAndDecay(u16 adsr1, u16 adsr2)
{
    PsxSpuEnvelope env;
    env.SetRegisters(adsr1, adsr2);
    env.KeyOn();

    AdsrTiming timing;
    u32 ticks = 0;
    while (env.CurrentPhase() == Phase::Attack && ticks < 10000000)
    {
        env.Tick();
        ticks++;
    }
    timing.mToPeak = ticks;
    EXPECT_EQ(env.Level(), 0x7FFF);

    while (env.CurrentPhase() == Phase::Decay && ticks < 10000000)
    {
        env.Tick();
        ticks++;
    }
    timing.mToSustain = ticks;
    timing.mSustainLevel = env.Level();
    EXPECT_EQ(env.CurrentPhase(), Phase::Sustain);
    return timing;
}

// Ticks from key off (at level 0x7FFF) until the release reaches 0
static u32 TimeRelease(u16 adsr2)
{
    PsxSpuEnvelope env;
    env.SetRegisters(0x000F, adsr2); // Fastest linear attack, sustain level 0x8000: holds at 0x7FFF
    env.KeyOn();
    for (s32 i = 0; i < 10; i++)
    {
        env.Tick();
    }
    EXPECT_EQ(env.Level(), 0x7FFF);
    EXPECT_EQ(env.CurrentPhase(), Phase::Sustain);

    env.KeyOff();
    u32 ticks = 0;
    while (env.CurrentPhase() == Phase::Release && ticks < 10000000)
    {
        env.Tick();
        ticks++;
    }
    EXPECT_EQ(env.Level(), 0);
    EXPECT_EQ(env.CurrentPhase(), Phase::Off);
    return ticks;
}

static u32 CeilDiv(u32 a, u32 b)
{
    return (a + b - 1) / b;
}

// ---------------------------------------------------------------------------------------------
// Gaussian table

TEST(PsxSpu, GaussTableKnownEntries)
{
    const std::array<s16, 512>& gauss = PsxSpu::GaussTable();
    ASSERT_EQ(gauss.size(), 512u);

    // The first 16 entries are the -1 lobe
    for (u32 i = 0; i < 16; i++)
    {
        EXPECT_EQ(gauss[i], -1) << i;
    }
    EXPECT_EQ(gauss[0x10], 0);
    EXPECT_EQ(gauss[0x17], 1);
    EXPECT_EQ(gauss[0x7F], 0x019C);
    EXPECT_EQ(gauss[0x80], 0x01A8);
    EXPECT_EQ(gauss[0xFF], 0x12C7);
    EXPECT_EQ(gauss[0x100], 0x1307);
    EXPECT_EQ(gauss[0x180], 0x3E4C);
    EXPECT_EQ(gauss[0x1FF], 0x59B3); // The peak

    s32 total = 0;
    for (s16 v : gauss)
    {
        total += v;
    }
    EXPECT_EQ(total, 4177922);

    // Rising all the way
    for (u32 i = 0x10; i < 0x1FF; i++)
    {
        EXPECT_LE(gauss[i], gauss[i + 1]) << i;
    }
}

TEST(PsxSpu, GaussTableFourTapSums)
{
    // psx-spx: each four taps sum to 7F80h (8000h * 255/256), rounding spreads them over
    // 7F7Fh..7F81h
    const std::array<s16, 512>& gauss = PsxSpu::GaussTable();
    s32 atExact = 0;
    for (u32 i = 0; i < 256; i++)
    {
        const s32 sum = gauss[0x000 + i] + gauss[0x0FF - i] + gauss[0x100 + i] + gauss[0x1FF - i];
        EXPECT_GE(sum, 0x7F7F) << i;
        EXPECT_LE(sum, 0x7F81) << i;
        atExact += sum == 0x7F80 ? 1 : 0;
    }
    EXPECT_GT(atExact, 128);
}

TEST(PsxSpu, InterpolationOfAConstantIsJustBelowUnity)
{
    const s16 samples[4] = {0x4000, 0x4000, 0x4000, 0x4000};
    for (u32 i = 0; i < 256; i++)
    {
        const s32 out = PsxSpu::Interpolate(samples, i);
        // 0x4000 * (7F7Fh..7F81h) / 8000h = 3FBFh..3FC0h, minus up to 4 for the per tap truncation
        EXPECT_GE(out, 0x3FBF - 4) << i;
        EXPECT_LE(out, 0x3FC0) << i;
    }

    // At index 0 the output is mostly the second oldest sample
    const s16 impulse[4] = {0, 0x7FFF, 0, 0};
    EXPECT_EQ(PsxSpu::Interpolate(impulse, 0), (0x59B3 * 0x7FFF) >> 15);
}

// ---------------------------------------------------------------------------------------------
// Pitch

// Plays a one shot sample of length samples and returns how many 44100 Hz samples the voice
// stays active for. Also checks that the ENDX bit is set exactly when the voice stops.
static u32 FramesUntilEnd(u16 pitch, u32 length)
{
    std::vector<s16> pcm(length, 0x4000);
    PsxSpu spu;
    spu.SetMasterVolume(0x7FFE, 0x7FFE);
    PsxSpuSample sample;
    sample.mPcm = pcm.data();
    sample.mLength = length;
    spu.SetVoiceSample(3, sample);
    spu.SetVoicePitch(3, pitch);
    spu.SetVoiceVolume(3, 0x7FFE, 0x7FFE);
    spu.SetVoiceAdsr(3, 0x000F, HoldingAdsr2(0));

    EXPECT_EQ(spu.Endx() & (1u << 3), 1u << 3); // Set until the first key on
    spu.KeyOn(3);
    EXPECT_EQ(spu.Endx() & (1u << 3), 0u);

    u32 frames = 0;
    u32 nonSilent = 0;
    s16 out[2] = {};
    while (spu.IsVoiceActive(3) && frames < length * 64)
    {
        EXPECT_EQ(spu.Endx() & (1u << 3), 0u) << frames;
        spu.Render(out, 1);
        nonSilent += out[0] != 0 ? 1 : 0;
        frames++;
    }
    EXPECT_EQ(spu.Endx() & (1u << 3), 1u << 3);
    EXPECT_GT(nonSilent, frames / 2);

    // Silent once the sample has ended
    for (s32 i = 0; i < 100; i++)
    {
        spu.Render(out, 1);
        EXPECT_EQ(out[0], 0);
        EXPECT_EQ(out[1], 0);
    }
    return frames;
}

TEST(PsxSpu, PitchSetsTheNumberOfOutputSamples)
{
    // The pitch counter advances by PITCH each 44100 Hz sample, and a whole sample is 1000h, so
    // length samples last ceil(length * 1000h / PITCH) output samples
    const u32 length = 1000;
    EXPECT_EQ(FramesUntilEnd(0x1000, length), length);
    EXPECT_EQ(FramesUntilEnd(0x0800, length), length * 2);
    EXPECT_EQ(FramesUntilEnd(0x2000, length), length / 2);
    EXPECT_EQ(FramesUntilEnd(0x3000, length), CeilDiv(length * 0x1000, 0x3000));
    EXPECT_EQ(FramesUntilEnd(0x0123, length), CeilDiv(length * 0x1000, 0x0123));
    // Anything above 3FFFh plays at 4000h
    EXPECT_EQ(FramesUntilEnd(0xFFFF, length), CeilDiv(length * 0x1000, 0x4000));
}

TEST(PsxSpu, PitchZeroNeverAdvances)
{
    std::vector<s16> pcm(10, 0x4000);
    PsxSpu spu;
    PsxSpuSample sample;
    sample.mPcm = pcm.data();
    sample.mLength = 10;
    spu.SetVoiceSample(0, sample);
    spu.SetVoicePitch(0, 0);
    spu.SetVoiceAdsr(0, 0x000F, HoldingAdsr2(0));
    spu.KeyOn(0);
    std::vector<s16> out(2 * 1000);
    spu.Render(out.data(), 1000);
    EXPECT_TRUE(spu.IsVoiceActive(0));
    EXPECT_EQ(spu.Endx() & 1u, 0u);
}

TEST(PsxSpu, PitchHelpers)
{
    EXPECT_EQ(PsxSpu::PitchFromSampleRate(44100), 0x1000);
    EXPECT_EQ(PsxSpu::PitchFromSampleRate(22050), 0x0800);
    EXPECT_EQ(PsxSpu::PitchFromSampleRate(11025), 0x0400);
    EXPECT_EQ(PsxSpu::PitchFromSampleRate(8000), 743); // 8000 * 4096 / 44100 = 743.04
    EXPECT_EQ(PsxSpu::PitchFromSampleRate(1000000), 0x3FFF);

    // The root note plays the sample at 44100 Hz, an octave is a factor of 2
    EXPECT_EQ(PsxSpu::PitchFromNote(60, 0, 60, 0), 0x1000);
    EXPECT_EQ(PsxSpu::PitchFromNote(72, 0, 60, 0), 0x2000);
    EXPECT_EQ(PsxSpu::PitchFromNote(48, 0, 60, 0), 0x0800);
    EXPECT_EQ(PsxSpu::PitchFromNote(36, 0, 60, 0), 0x0400);
    EXPECT_EQ(PsxSpu::PitchFromNote(61, 0, 60, 0), 4339); // floor(4096 * 2^(1/12))
    EXPECT_EQ(PsxSpu::PitchFromNote(59, 0, 60, 0), 3866); // floor(4096 * 2^(11/12)) >> 1

    // libsnd ADDS the shift to the fine tune: a shift of 64 RAISES the pitch by half a semitone.
    // POSITIV9.SEQ's tone (AE MINES.VH program 27: centre 90, shift 70) played at its root.
    EXPECT_EQ(PsxSpu::PitchFromNote(60, 0, 60, 64), 4216); // floor(4096 * 2^(8/192))
    EXPECT_EQ(PsxSpu::PitchFromNote(90, 0, 90, 70), 4216); // 70 / 8 = 8.75, rounded down to 8
    // Fine tune and shift are rounded down to 1/16 semitones together
    EXPECT_EQ(PsxSpu::PitchFromNote(60, 7, 60, 0), 0x1000);
    EXPECT_EQ(PsxSpu::PitchFromNote(60, 4, 60, 4), 4110);
    // Past a semitone they carry into the note: 100 + 100 = 200 = 1 semitone + 72
    EXPECT_EQ(PsxSpu::PitchFromNote(60, 100, 60, 100), PsxSpu::PitchFromNote(61, 72, 60, 0));

    // Not clamped: two octaves up is 4000h, which the SPU plays at its highest rate
    EXPECT_EQ(PsxSpu::PitchFromNote(84, 0, 60, 0), 0x4000);
    // More than 60 semitones below the root (libsnd would read before its table):
    // floor(4096 * 2^(5/12)) >> 6 = 85, 4096 * 2^(-67/12) = 85.3
    EXPECT_EQ(PsxSpu::PitchFromNote(0, 0, 67, 0), 85);
}

// ---------------------------------------------------------------------------------------------
// ADSR

TEST(PsxSpu, AdsrLinearAttack)
{
    // Linear, shift <= 11: +(7 - step) << (11 - shift) every sample
    // Attack shift 0, step 0 (+7): 7 << 11 = 14336 a sample
    EXPECT_EQ(TimeAttackAndDecay(0x000F, HoldingAdsr2(0)).mToPeak, CeilDiv(0x7FFF, 7 << 11));

    // Shift > 11: +(7 - step) every 1 << (shift - 11) samples.
    // Attack shift 20, step 1 (+6), decay shift 11, sustain level 10
    const AdsrTiming t = TimeAttackAndDecay(0x51BA, HoldingAdsr2(0));
    EXPECT_EQ(t.mToPeak, CeilDiv(0x7FFF, 6) * (1u << (20 - 11)));
    EXPECT_EQ(t.mToPeak, 2796544u);
}

TEST(PsxSpu, AdsrExponentialAttack)
{
    // Exponential, shift 0, step 0: +14336 while the level is <= 6000h, then a quarter of that.
    // 0 -> 14336 -> 28672 (> 6000h) -> +3584 -> 32256 -> +3584 -> 7FFFh: 4 samples
    EXPECT_EQ(TimeAttackAndDecay(0x800F, HoldingAdsr2(0)).mToPeak, 4u);

    // Exponential, shift 12, step 0: +7 every 2 samples while <= 6000h (3511 steps to 24577),
    // then above 6000h the counter increment is a quarter: +7 every 8 samples (1170 steps to 7FFFh).
    // 3511 * 2 + 1170 * 8 = 16382
    const AdsrTiming b = TimeAttackAndDecay(0xB043, HoldingAdsr2(0));
    EXPECT_EQ(b.mToPeak, 3511u * 2 + 1170u * 8);

    // Exponential, shift 10, step 2: +10 every sample while <= 6000h (2458 steps to 24580), then
    // above it the step and the counter increment are both halved: +5 every 2 samples (1638 steps).
    // 2458 + 1638 * 2 = 5734
    const AdsrTiming d = TimeAttackAndDecay(0xAAF0, HoldingAdsr2(0));
    EXPECT_EQ(d.mToPeak, 2458u + 1638u * 2);
}

TEST(PsxSpu, AdsrDecayToSustainLevel)
{
    // Decay: exponential, step -8 << (11 - shift) scaled by level / 8000h, until the level is at
    // or below (N + 1) * 800h.

    // Decay shift 0, sustain level 7 (4000h): 7FFFh - (16384 * 7FFFh >> 15 = 16384) = 16383,
    // one sample after the peak
    const AdsrTiming a = TimeAttackAndDecay(0x0007, HoldingAdsr2(0));
    EXPECT_EQ(a.mToPeak, 3u);
    EXPECT_EQ(a.mToSustain, 3u + 1);
    EXPECT_EQ(a.mSustainLevel, 16383);

    // The rest were counted by stepping the psx-spx pseudocode from 7FFFh:
    // decay shift 4, sustain level 3 (2000h): 44 samples, ends at 8093
    const AdsrTiming b = TimeAttackAndDecay(0xB043, HoldingAdsr2(0));
    EXPECT_EQ(b.mToSustain - b.mToPeak, 44u);
    EXPECT_EQ(b.mSustainLevel, 8093);

    // decay shift 11, sustain level 10 (5800h): 1439 samples, ends at 22524
    const AdsrTiming c = TimeAttackAndDecay(0x51BA, HoldingAdsr2(0));
    EXPECT_EQ(c.mToSustain - c.mToPeak, 1439u);
    EXPECT_EQ(c.mSustainLevel, 22524);

    // decay shift 15, sustain level 0 (800h): 145344 samples, ends at 2048 exactly
    const AdsrTiming d = TimeAttackAndDecay(0xAAF0, HoldingAdsr2(0));
    EXPECT_EQ(d.mToSustain - d.mToPeak, 145344u);
    EXPECT_EQ(d.mSustainLevel, 0x800);

    // Sustain level 15 (8000h): the first decay sample goes straight to the sustain, without a
    // decay step
    const AdsrTiming e = TimeAttackAndDecay(0x000F, HoldingAdsr2(0));
    EXPECT_EQ(e.mToSustain, e.mToPeak + 1);
    EXPECT_EQ(e.mSustainLevel, 0x7FFF);
}

TEST(PsxSpu, AdsrSustainRate)
{
    // Linear increasing sustain, shift 9, step 2 (+5): +5 << 2 = +20 a sample from 16383 (the
    // level decay shift 0 / sustain level 7 ends at) to 7FFFh
    PsxSpuEnvelope env;
    env.SetRegisters(0x0007, 0x0980);
    env.KeyOn();
    u32 ticks = 0;
    while (env.CurrentPhase() != Phase::Sustain)
    {
        env.Tick();
        ticks++;
    }
    ASSERT_EQ(env.Level(), 16383);
    u32 sustainTicks = 0;
    while (env.Level() < 0x7FFF && sustainTicks < 100000)
    {
        env.Tick();
        sustainTicks++;
    }
    EXPECT_EQ(sustainTicks, CeilDiv(0x7FFF - 16383, 20));
    EXPECT_EQ(env.CurrentPhase(), Phase::Sustain); // Only key off ends the sustain

    // Linear decreasing sustain, shift 0, step 0 (-8 << 11): to 0 and stays there
    env.SetRegisters(0x000F, 0x4000);
    env.KeyOn();
    for (s32 i = 0; i < 3; i++)
    {
        env.Tick();
    }
    ASSERT_EQ(env.Level(), 0x7FFF);
    ticks = 0;
    while (env.Level() > 0)
    {
        env.Tick();
        ticks++;
    }
    EXPECT_EQ(ticks, CeilDiv(0x7FFF, 8 << 11));
    env.Tick();
    EXPECT_EQ(env.Level(), 0);
    EXPECT_EQ(env.CurrentPhase(), Phase::Sustain);
}

TEST(PsxSpu, AdsrRelease)
{
    // Linear: -8 << (11 - shift) a sample. Shift 5: -512
    EXPECT_EQ(TimeRelease(HoldingAdsr2(5)), CeilDiv(0x7FFF, 8 << 6));
    // Shift 13: -8 every 4 samples
    EXPECT_EQ(TimeRelease(HoldingAdsr2(13)), CeilDiv(0x7FFF, 8) * 4);

    // Exponential shift 8 (counted by stepping the psx-spx pseudocode): 2427 samples
    EXPECT_EQ(TimeRelease(HoldingAdsr2(0x20 | 8)), 2427u);

    // Release shift 1Fh never steps (all rate bits set)
    PsxSpuEnvelope env;
    env.SetRegisters(0x000F, HoldingAdsr2(0x1F));
    env.KeyOn();
    for (s32 i = 0; i < 10; i++)
    {
        env.Tick();
    }
    env.KeyOff();
    for (s32 i = 0; i < 100000; i++)
    {
        env.Tick();
    }
    EXPECT_EQ(env.Level(), 0x7FFF);
    EXPECT_EQ(env.CurrentPhase(), Phase::Release);
}

TEST(PsxSpu, AdsrKeyOffDuringAttack)
{
    PsxSpuEnvelope env;
    env.SetRegisters(0x5000, HoldingAdsr2(0)); // Slow attack, fastest release
    env.KeyOn();
    for (s32 i = 0; i < 5000; i++)
    {
        env.Tick();
    }
    const s16 level = env.Level();
    ASSERT_GT(level, 0);
    ASSERT_LT(level, 0x7FFF);
    env.KeyOff();
    u32 ticks = 0;
    while (env.CurrentPhase() != Phase::Off)
    {
        env.Tick();
        ticks++;
    }
    EXPECT_EQ(ticks, CeilDiv(static_cast<u32>(level), 8 << 11));
}

// ---------------------------------------------------------------------------------------------
// Loops

static std::vector<s16> Ramp(u32 length)
{
    std::vector<s16> pcm(length);
    for (u32 i = 0; i < length; i++)
    {
        pcm[i] = static_cast<s16>((static_cast<s32>(i) * 997) % 20000 - 10000);
    }
    return pcm;
}

static std::vector<s16> RenderVoice(const PsxSpuSample& sample, u32 frames, u32* pEndxFrame)
{
    PsxSpu spu;
    spu.SetMasterVolume(0x7FFE, 0x7FFE);
    spu.SetVoiceSample(0, sample);
    spu.SetVoicePitch(0, 0x1000);
    spu.SetVoiceVolume(0, 0x7FFE, 0x7FFE);
    spu.SetVoiceAdsr(0, 0x000F, HoldingAdsr2(0));
    spu.KeyOn(0);

    std::vector<s16> out(frames);
    for (u32 i = 0; i < frames; i++)
    {
        s16 frame[2] = {};
        spu.Render(frame, 1);
        out[i] = frame[0];
        if (pEndxFrame && *pEndxFrame == 0 && (spu.Endx() & 1u))
        {
            *pEndxFrame = i + 1;
        }
    }
    return out;
}

TEST(PsxSpu, LoopWholeSample)
{
    const std::vector<s16> pcm = Ramp(100);
    PsxSpuSample sample;
    sample.mPcm = pcm.data();
    sample.mLength = 100;
    sample.mLoop = true;

    u32 endxFrame = 0;
    const std::vector<s16> out = RenderVoice(sample, 1000, &endxFrame);
    // ENDX is set when the loop end is reached, but the voice keeps playing
    EXPECT_EQ(endxFrame, 100u);
    // Once the envelope is at its peak, the output repeats every 100 samples
    for (u32 i = 10; i < 900; i++)
    {
        ASSERT_EQ(out[i], out[i + 100]) << i;
    }
    // And is the sample, delayed by the interpolation (at index 0 it's mostly 2 samples back)
    EXPECT_NE(out[500], 0);
}

TEST(PsxSpu, LoopPart)
{
    const std::vector<s16> pcm = Ramp(100);
    PsxSpuSample sample;
    sample.mPcm = pcm.data();
    sample.mLength = 100;
    sample.mLoop = true;
    sample.mLoopStart = 40;
    sample.mLoopEnd = 70;

    u32 endxFrame = 0;
    const std::vector<s16> out = RenderVoice(sample, 1000, &endxFrame);
    EXPECT_EQ(endxFrame, 70u);
    // Samples 40-69 repeat: a period of 30
    for (u32 i = 80; i < 900; i++)
    {
        ASSERT_EQ(out[i], out[i + 30]) << i;
    }

    // Playing the loop part alone as a whole sample loop gives the same output once both are
    // inside the loop. At frame i >= 70 the first voice's newest sample is 40 + (i - 70) % 30, the
    // second one's is (j % 30) of the loop, so frame i matches frame j = i + 50.
    std::vector<s16> loopOnly(pcm.begin() + 40, pcm.begin() + 70);
    PsxSpuSample loopSample;
    loopSample.mPcm = loopOnly.data();
    loopSample.mLength = 30;
    loopSample.mLoop = true;
    const std::vector<s16> outLoopOnly = RenderVoice(loopSample, 1000, nullptr);
    for (u32 i = 100; i < 900; i++)
    {
        ASSERT_EQ(out[i], outLoopOnly[i + 50]) << i;
    }
}

TEST(PsxSpu, OneShotStopsAndIsFree)
{
    const std::vector<s16> pcm = Ramp(100);
    PsxSpuSample sample;
    sample.mPcm = pcm.data();
    sample.mLength = 100;

    u32 endxFrame = 0;
    const std::vector<s16> out = RenderVoice(sample, 300, &endxFrame);
    EXPECT_EQ(endxFrame, 100u);
    for (u32 i = 100; i < 300; i++)
    {
        ASSERT_EQ(out[i], 0) << i;
    }
}

TEST(PsxSpu, KeyOnRestartsAndSampleChangesAtKeyOn)
{
    const std::vector<s16> a(50, 1000);
    const std::vector<s16> b(500, -1000);
    PsxSpu spu;
    spu.SetMasterVolume(0x7FFE, 0x7FFE);
    spu.SetVoicePitch(0, 0x1000);
    spu.SetVoiceVolume(0, 0x7FFE, 0x7FFE);
    spu.SetVoiceAdsr(0, 0x000F, HoldingAdsr2(0));
    PsxSpuSample sa;
    sa.mPcm = a.data();
    sa.mLength = 50;
    spu.SetVoiceSample(0, sa);
    spu.KeyOn(0);

    s16 frame[2] = {};
    for (s32 i = 0; i < 10; i++)
    {
        spu.Render(frame, 1);
    }
    // A new sample only takes effect at the next key on
    PsxSpuSample sb;
    sb.mPcm = b.data();
    sb.mLength = 500;
    spu.SetVoiceSample(0, sb);
    spu.Render(frame, 1);
    EXPECT_GT(frame[0], 0);

    spu.KeyOn(0);
    for (s32 i = 0; i < 100; i++)
    {
        spu.Render(frame, 1);
    }
    EXPECT_LT(frame[0], 0);
    EXPECT_TRUE(spu.IsVoiceActive(0));
}

// ---------------------------------------------------------------------------------------------
// Reverb

static std::vector<s16> RenderImpulse(PsxSpuReverb::Preset preset, bool voiceReverb, bool masterReverb, u32 frames)
{
    static const s16 kImpulse[4] = {0x7FFF, 0x7FFF, 0, 0};
    PsxSpu spu;
    spu.SetMasterVolume(0x7FFE, 0x7FFE);
    spu.SetReverbPreset(preset);
    spu.SetReverbDepth(0x7FFF, 0x7FFF);
    spu.SetReverbEnabled(masterReverb);

    PsxSpuSample sample;
    sample.mPcm = kImpulse;
    sample.mLength = 4;
    spu.SetVoiceSample(0, sample);
    spu.SetVoicePitch(0, 0x1000);
    spu.SetVoiceVolume(0, 0x7FFE, 0x4000);
    spu.SetVoiceAdsr(0, 0x000F, HoldingAdsr2(0));
    spu.SetVoiceReverb(0, voiceReverb);
    spu.KeyOn(0);

    std::vector<s16> out(frames * 2);
    spu.Render(out.data(), frames);
    return out;
}

static s32 PeakAfter(const std::vector<s16>& stereo, u32 fromFrame)
{
    s32 peak = 0;
    for (size_t i = fromFrame * 2; i < stereo.size(); i++)
    {
        peak = std::max(peak, std::abs(static_cast<s32>(stereo[i])));
    }
    return peak;
}

TEST(PsxSpuReverb, ImpulseResponseIsDeterministicAndNotSilent)
{
    const PsxSpuReverb::Preset presets[] = {
        PsxSpuReverb::Preset::Room,
        PsxSpuReverb::Preset::StudioSmall,
        PsxSpuReverb::Preset::StudioMedium,
        PsxSpuReverb::Preset::StudioLarge,
        PsxSpuReverb::Preset::Hall,
        PsxSpuReverb::Preset::SpaceEcho,
        PsxSpuReverb::Preset::Echo,
        PsxSpuReverb::Preset::Delay,
        PsxSpuReverb::Preset::Pipe,
    };

    for (PsxSpuReverb::Preset preset : presets)
    {
        const std::vector<s16> a = RenderImpulse(preset, true, true, 44100);
        const std::vector<s16> b = RenderImpulse(preset, true, true, 44100);
        EXPECT_EQ(a, b) << static_cast<s32>(preset);
        // The dry impulse is over after a few samples, anything later is the reverb
        EXPECT_GT(PeakAfter(a, 16), 0) << static_cast<s32>(preset);
    }

    const std::vector<s16> hall = RenderImpulse(PsxSpuReverb::Preset::Hall, true, true, 44100);
    EXPECT_GT(PeakAfter(hall, 1000), 0);
}

TEST(PsxSpuReverb, SilentWhenOff)
{
    // Preset off
    EXPECT_EQ(PeakAfter(RenderImpulse(PsxSpuReverb::Preset::Off, true, true, 22050), 16), 0);
    // Voice not sent to the reverb
    EXPECT_EQ(PeakAfter(RenderImpulse(PsxSpuReverb::Preset::Hall, false, true, 22050), 16), 0);
    // Reverb master enable (ATTR bit 7) off: nothing is written to the work area
    EXPECT_EQ(PeakAfter(RenderImpulse(PsxSpuReverb::Preset::Hall, true, false, 22050), 16), 0);

    // The dry impulse itself is there in all cases
    EXPECT_GT(PeakAfter(RenderImpulse(PsxSpuReverb::Preset::Off, true, true, 16), 0), 0);
}

TEST(PsxSpuReverb, DepthScalesTheOutput)
{
    PsxSpuReverb full;
    PsxSpuReverb half;
    full.SetPreset(PsxSpuReverb::Preset::Room);
    half.SetPreset(PsxSpuReverb::Preset::Room);
    full.SetEnabled(true);
    half.SetEnabled(true);
    full.SetOutputVolume(PsxSpuReverb::DepthToVolume(127), PsxSpuReverb::DepthToVolume(127));
    half.SetOutputVolume(PsxSpuReverb::DepthToVolume(64), PsxSpuReverb::DepthToVolume(64));

    s64 energyFull = 0;
    s64 energyHalf = 0;
    for (u32 i = 0; i < 20000; i++)
    {
        const s32 in = i < 64 ? 0x4000 : 0;
        s32 l = 0;
        s32 r = 0;
        full.Process(in, in, l, r);
        energyFull += std::abs(l) + std::abs(r);
        half.Process(in, in, l, r);
        energyHalf += std::abs(l) + std::abs(r);
    }
    EXPECT_GT(energyFull, 0);
    EXPECT_GT(energyHalf, energyFull * 4 / 10);
    EXPECT_LT(energyHalf, energyFull * 6 / 10);

    EXPECT_EQ(PsxSpuReverb::DepthToVolume(0), 0);
    EXPECT_EQ(PsxSpuReverb::DepthToVolume(127), 0x7FFF);
}

TEST(PsxSpuReverb, PresetTable)
{
    // Spot checks against psx-spx "SPU Reverb Examples"
    EXPECT_EQ(PsxSpuReverb::PresetWorkAreaBytes(PsxSpuReverb::Preset::Room), 0x26C0u);
    EXPECT_EQ(PsxSpuReverb::PresetWorkAreaBytes(PsxSpuReverb::Preset::Hall), 0xADE0u);
    EXPECT_EQ(PsxSpuReverb::PresetWorkAreaBytes(PsxSpuReverb::Preset::Delay), 0x18040u);
    EXPECT_EQ(PsxSpuReverb::PresetWorkAreaBytes(PsxSpuReverb::Preset::Pipe), 0x3C00u);

    const PsxSpuReverb::Registers& room = PsxSpuReverb::PresetRegisters(PsxSpuReverb::Preset::Room);
    EXPECT_EQ(room.dAPF1, 0x007D);
    EXPECT_EQ(room.vIIR, 0x6D80);
    EXPECT_EQ(room.vCOMB2, static_cast<s16>(0xBED0));
    EXPECT_EQ(room.mLSAME, 0x04D6);
    EXPECT_EQ(room.mRAPF2, 0x005C);
    EXPECT_EQ(room.vLIN, -0x8000);
    EXPECT_EQ(room.vRIN, -0x8000);

    const PsxSpuReverb::Registers& off = PsxSpuReverb::PresetRegisters(PsxSpuReverb::Preset::Off);
    EXPECT_EQ(off.vLIN, 0);
    EXPECT_EQ(off.mLSAME, 1);
}

// ---------------------------------------------------------------------------------------------
// Mixer

// A second of several voices: different pitches, envelopes, pans, loops and reverb
static std::vector<s16> RenderScene(u32 chunkFrames)
{
    // Integer only waveforms, so the checksum can't depend on the platform's sin()
    std::vector<s16> tri;
    std::vector<s16> saw_pcm;
    for (s32 i = 0; i < 2200; i++)
    {
        const s32 phase = i % 100;
        const s32 value = phase < 50 ? phase * 480 - 12000 : (100 - phase) * 480 - 12000;
        tri.push_back(static_cast<s16>(value));
    }
    for (s32 i = 0; i < 3000; i++)
    {
        saw_pcm.push_back(static_cast<s16>((i % 80) * 250 - 10000));
    }

    PsxSpu spu;
    spu.SetMasterVolume(0x6000, 0x6000);
    spu.SetReverbPreset(PsxSpuReverb::Preset::Room);
    spu.SetReverbDepth(PsxSpuReverb::DepthToVolume(80), PsxSpuReverb::DepthToVolume(80));
    spu.SetReverbEnabled(true);

    PsxSpuSample sine;
    sine.mPcm = tri.data();
    sine.mLength = static_cast<u32>(tri.size());
    sine.mLoop = true;
    PsxSpuSample saw;
    saw.mPcm = saw_pcm.data();
    saw.mLength = static_cast<u32>(saw_pcm.size());

    spu.SetVoiceSample(0, sine);
    spu.SetVoicePitch(0, PsxSpu::PitchFromNote(60, 0, 60, 0));
    spu.SetVoiceVolume(0, 0x3000, 0x1000);
    spu.SetVoiceAdsr(0, 0x3A8F, 0x5FC5);
    spu.SetVoiceReverb(0, true);

    spu.SetVoiceSample(1, sine);
    spu.SetVoicePitch(1, PsxSpu::PitchFromNote(67, 30, 60, 0));
    spu.SetVoiceVolume(1, 0x1000, 0x3000);
    spu.SetVoiceAdsr(1, 0xBF1F, 0x9FC5);

    spu.SetVoiceSample(2, saw);
    spu.SetVoicePitch(2, PsxSpu::PitchFromSampleRate(22050));
    spu.SetVoiceVolume(2, 0x2000, 0x2000);
    spu.SetVoiceAdsr(2, 0x00FF, 0x1FC0);
    spu.SetVoiceReverb(2, true);

    // Voice 23 with a volume sweep (linear increase)
    spu.SetVoiceSample(23, sine);
    spu.SetVoicePitch(23, 0x0C00);
    spu.SetVoiceVolumeRegisters(23, 0x8000 | (20 << 2), 0x0800);
    spu.SetVoiceAdsr(23, 0x000F, 0x1FC0);

    spu.KeyOnMask((1u << 0) | (1u << 2) | (1u << 23));

    const u32 total = PsxSpu::kSampleRate;
    std::vector<s16> out(total * 2);
    u32 done = 0;
    while (done < total)
    {
        if (done >= 11025 && done < 11025 + chunkFrames)
        {
            spu.KeyOn(1);
        }
        if (done >= 22050 && done < 22050 + chunkFrames)
        {
            spu.KeyOffMask((1u << 0) | (1u << 1) | (1u << 23));
        }
        const u32 n = std::min(chunkFrames, total - done);
        spu.Render(&out[done * 2], n);
        done += n;
    }
    return out;
}

TEST(PsxSpu, RenderChecksum)
{
    const std::vector<s16> out = RenderScene(441);
    const u32 crc = SoundGoldSession::Crc32(reinterpret_cast<const u8*>(out.data()), out.size() * sizeof(s16));
    EXPECT_EQ(crc, 0xB66CA325u) << std::hex << "crc 0x" << crc;

    // Not silent, and the reverb keeps sounding after the key offs
    EXPECT_GT(PeakAfter(out, 0), 1000);
    EXPECT_GT(PeakAfter(out, 40000), 0);
}

TEST(PsxSpu, RenderIsTheSameInAnyBlockSize)
{
    // Rendering in blocks must not change the output (no clicks at block edges). The key ons and
    // offs happen at the same frames in both because both block sizes divide 11025.
    EXPECT_EQ(RenderScene(441), RenderScene(49));
    EXPECT_EQ(RenderScene(441), RenderScene(11025));
}

TEST(PsxSpu, MixClamps)
{
    const std::vector<s16> loud(1000, 0x7FFF);
    PsxSpu spu;
    spu.SetMasterVolume(0x7FFE, 0x7FFE);
    PsxSpuSample sample;
    sample.mPcm = loud.data();
    sample.mLength = 1000;
    for (s32 i = 0; i < PsxSpu::kNumVoices; i++)
    {
        spu.SetVoiceSample(i, sample);
        spu.SetVoicePitch(i, 0x1000);
        spu.SetVoiceVolume(i, 0x7FFE, -0x8000);
        spu.SetVoiceAdsr(i, 0x000F, HoldingAdsr2(0));
    }
    spu.KeyOnMask(0xFFFFFF);
    std::vector<s16> out(2 * 500);
    spu.Render(out.data(), 500);
    // The mix clamps to 7FFFh / -8000h before the master volume (7FFEh) is applied
    EXPECT_EQ(out[2 * 400], (0x7FFF * 0x7FFE) >> 15);
    EXPECT_EQ(out[2 * 400 + 1], (-0x8000 * 0x7FFE) >> 15);
}

// ---------------------------------------------------------------------------------------------
// Band-limited interpolation

// A looped sine of the given period (in source samples), played at pitch for frames, left channel
static std::vector<s32> RenderSine(PsxSpu::Interpolation interpolation, u16 pitch, s32 period, u32 frames)
{
    static std::vector<s16> pcm;
    pcm.resize(static_cast<size_t>(period) * 200);
    for (size_t i = 0; i < pcm.size(); i++)
    {
        pcm[i] = static_cast<s16>(std::lround(12000.0 * std::sin(2.0 * 3.14159265358979323846 * static_cast<f64>(i) / period)));
    }

    static PsxSpu spu;
    spu.Reset();
    spu.SetInterpolation(interpolation);
    spu.SetMasterVolume(0x7FFE, 0x7FFE);
    PsxSpuSample sample;
    sample.mPcm = pcm.data();
    sample.mLength = static_cast<u32>(pcm.size());
    sample.mLoop = true;
    spu.SetVoiceSample(0, sample);
    spu.SetVoicePitch(0, pitch);
    spu.SetVoiceVolume(0, 0x7FFE, 0x7FFE);
    spu.SetVoiceAdsr(0, 0x000F, 0x1FC0); // Fastest attack, then hold
    spu.KeyOn(0);

    std::vector<s16> out(frames * 2);
    spu.Render(out.data(), frames);
    std::vector<s32> left(frames);
    for (u32 i = 0; i < frames; i++)
    {
        left[i] = out[i * 2];
    }
    return left;
}

static f64 Rms(const std::vector<s32>& x, size_t from)
{
    f64 sum = 0;
    for (size_t i = from; i < x.size(); i++)
    {
        sum += static_cast<f64>(x[i]) * x[i];
    }
    return std::sqrt(sum / static_cast<f64>(x.size() - from));
}

TEST(PsxSpu, BandLimitedIsGaussianUpTo44100)
{
    for (const u16 pitch : {0x0800, 0x0C00, 0x1000})
    {
        EXPECT_EQ(RenderSine(PsxSpu::Interpolation::BandLimited, pitch, 37, 4000), RenderSine(PsxSpu::Interpolation::Gaussian, pitch, 37, 4000)) << pitch;
    }
}

TEST(PsxSpu, BandLimitedRemovesAliasing)
{
    // A period of 6 samples at 3E2Ch (3.89x) is a 28.6 kHz tone, above the output's Nyquist: the
    // gaussian folds it back to 15.5 kHz, the band-limited resampler filters it out
    const f64 gaussian = Rms(RenderSine(PsxSpu::Interpolation::Gaussian, 0x3E2C, 6, 8000), 2000);
    const f64 bandLimited = Rms(RenderSine(PsxSpu::Interpolation::BandLimited, 0x3E2C, 6, 8000), 2000);
    EXPECT_GT(gaussian, 1000.0);
    EXPECT_LT(bandLimited, gaussian / 100.0); // At least 40 dB down
}

TEST(PsxSpu, BandLimitedKeepsInBandTonesAndTiming)
{
    // A period of 64 samples at 3E2Ch is 2677 Hz: both should play it the same, at the same time,
    // so a voice bent across 1000h doesn't jump
    const std::vector<s32> gaussian = RenderSine(PsxSpu::Interpolation::Gaussian, 0x3E2C, 64, 8000);
    const std::vector<s32> bandLimited = RenderSine(PsxSpu::Interpolation::BandLimited, 0x3E2C, 64, 8000);
    const f64 g = Rms(gaussian, 2000);
    const f64 b = Rms(bandLimited, 2000);
    EXPECT_NEAR(b / g, 1.0, 0.05);

    f64 diff = 0;
    for (size_t i = 2000; i < gaussian.size(); i++)
    {
        const f64 d = static_cast<f64>(gaussian[i]) - bandLimited[i];
        diff += d * d;
    }
    diff = std::sqrt(diff / static_cast<f64>(gaussian.size() - 2000));
    EXPECT_LT(diff, g * 0.1); // In phase: a one sample shift would be a 60% difference
}
