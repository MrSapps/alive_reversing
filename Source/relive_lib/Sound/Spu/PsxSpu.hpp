#pragma once

#include "../../Types.hpp"
#include "PsxSpuReverb.hpp"
#include <array>

// An emulated PS1 SPU (Sound Processing Unit), implemented from the psx-spx documentation
// ("Sound Processing Unit (SPU)"). It is the bottom layer of the PS1 accurate sound engine: a
// libsnd style layer (SEQ player, VAB tone mapping, voice allocation) goes on top of it.
//
// Differences from the real SPU:
// - The voices play 16 bit PCM (the PC samples) instead of decoding ADPCM blocks. A sample's loop
//   points are given in samples instead of by ADPCM block flags.
// - No noise generator, pitch modulation (PMON), CD/external audio input, IRQs or capture buffers.
//   The games don't use them.
//
// Everything is integer arithmetic, so the output is deterministic: the same register writes and
// Render calls always give the same samples.
//
// Real time use: Render never allocates, locks or does I/O, and costs at most one pass over the
// active voices per output sample. The class isn't thread safe, so the register writes and Render
// must come from the same thread (e.g. the audio callback, fed by a command queue) or be
// serialised by the owner.

// How many low-passed copies of a sample there can be, see PsxSpuSample::mFiltered
static constexpr s32 kPsxSpuFilterLevels = 3;

// A voice's sample. The SPU doesn't own the PCM: it must stay alive while a voice plays it.
struct PsxSpuSample final
{
    const s16* mPcm = nullptr;
    u32 mLength = 0; // In samples

    // For the band-limited interpolation (not the hardware): mFiltered[k - 1] is mPcm low-passed to
    // below 1/2^k of its Nyquist (PsxSpu::FilterSample), the same length so the loop points are
    // too. A voice played at more than 2^k times the rate reads it at a stride of 2^k, which keeps
    // its cost the same at any pitch. Optional: without one the voice reads mPcm, at a cost that
    // grows with the pitch.
    std::array<const s16*, kPsxSpuFilterLevels> mFiltered = {};

    // A looping sample jumps back to mLoopStart when it reaches mLoopEnd, forever (the ADPCM
    // "End+Repeat" code). A one shot sample stops when it reaches its end, and the voice is
    // muted (the ADPCM "End+Mute" code: release, envelope level 0).
    bool mLoop = false;
    u32 mLoopStart = 0;
    u32 mLoopEnd = 0; // 0 = mLength
};

// The ADSR (and volume sweep) envelope generator, stepped once per 44100 Hz sample.
class PsxSpuEnvelope final
{
public:
    enum class Phase : u8
    {
        Off, // Released to 0, or muted by a one shot sample's end
        Attack,
        Decay,
        Sustain,
        Release,
    };

    // The raw ADSR1/ADSR2 registers (see "SPU Volume and ADSR Generator" in psx-spx)
    void SetRegisters(u16 adsr1, u16 adsr2)
    {
        mAdsr1 = adsr1;
        mAdsr2 = adsr2;
    }

    u16 Adsr1() const
    {
        return mAdsr1;
    }

    u16 Adsr2() const
    {
        return mAdsr2;
    }

    // Starts the attack from level 0
    void KeyOn();
    // Starts the release from the current level
    void KeyOff();
    // Level 0 and Off, what a one shot sample's end does
    void Mute();

    // One 44100 Hz step
    void Tick();

    s16 Level() const
    {
        return mLevel;
    }

    Phase CurrentPhase() const
    {
        return mPhase;
    }

    // Sustain level from ADSR1 bits 0-3: (N + 1) * 0x800. The decay ends as soon as the level is
    // at or below it, so N = 15 (0x8000) skips the decay.
    static s32 SustainLevel(u16 adsr1);

    // The envelope step shared by the ADSR and the volume sweep, straight from psx-spx
    // "Envelope Operation depending on Shift/Step/Mode/Direction". stepValue is 0-3
    // ("+7,+6,+5,+4" / "-8,-7,-6,-5"), shift is 0-31. counter is the phase's 15 bit step counter.
    // neverSteps is the "all rate bits set" case: no step and no saturation at all.
    struct Rate final
    {
        bool mExponential = false;
        bool mDecreasing = false;
        bool mPhaseNegative = false;
        u8 mShift = 0;
        u8 mStep = 0;
        bool mNeverSteps = false;
    };
    static void Step(const Rate& rate, s32& level, u32& counter);

private:
    Rate CurrentRate() const;
    void EnterPhase(Phase phase);

    u16 mAdsr1 = 0;
    u16 mAdsr2 = 0;
    Phase mPhase = Phase::Off;
    s16 mLevel = 0;
    u32 mCounter = 0;
};

// One stereo volume register pair (VOLL/VOLR, MVOLL/MVOLR), fixed or sweep mode.
class PsxSpuVolume final
{
public:
    // The raw register. Bit 15 = 0: fixed volume, bits 0-14 are volume / 2. Bit 15 = 1: sweep,
    // with bits 0-14 the sweep mode/direction/phase/shift/step.
    void SetRegister(u16 reg);

    // A fixed volume, -0x8000 to 0x7FFE
    void SetFixed(s16 volume)
    {
        SetRegister(static_cast<u16>((volume >> 1) & 0x7FFF));
    }

    void Tick();

    s16 Current() const
    {
        return mCurrent;
    }

private:
    u16 mRegister = 0;
    s16 mCurrent = 0;
    u32 mCounter = 0;
};

class PsxSpu final
{
public:
    // How a voice's sample is resampled to 44100 Hz
    enum class Interpolation : u8
    {
        // The SPU's 4 point gaussian interpolation at every pitch. Exact, but a voice played above
        // 44100 Hz (PITCH > 1000h) aliases: its sample's high frequencies fold back into the audible
        // range, which is heard as a buzz on bright samples at high notes.
        Gaussian,
        // Not the hardware: gaussian at and below 44100 Hz, and a band-limited (windowed sinc)
        // resampler above it, which filters out what would alias. Same timing as the gaussian.
        BandLimited,
    };

    static constexpr s32 kNumVoices = 24;
    static constexpr u32 kSampleRate = 44100;
    static constexpr u16 kPitch44100 = 0x1000;
    // Larger pitches play at this rate. NOT the hardware, which plays PITCH above 3FFFh at 4000h
    // ("IF Step>3FFFh then Step=4000h"). The voices play the PC samples, and the PC data has some
    // at a higher rate than the PS1's (with the tone's centre note lowered to match), so a note the
    // PS1 played below 4000h can need more here: AO's secret area jingle (vag 43) is 2.75x the PS1
    // rate, and its high notes need up to AAD8h. With the hardware limit they played up to 1.4
    // octaves low. 16x covers samples at up to 4x the PS1 rate.
    static constexpr u32 kMaxPitch = 0x10000;

    PsxSpu();

    // SPU reset: every voice off, volumes 0, reverb off
    void Reset();

    // Per voice registers. voice is 0-23, anything else is ignored.
    // Like SSA, the sample is only picked up by the next key on: a playing voice keeps playing
    // the sample it was keyed on with.
    void SetVoiceSample(s32 voice, const PsxSpuSample& sample);
    // 0x1000 = 44100 Hz. Up to kMaxPitch, higher values play at kMaxPitch.
    void SetVoicePitch(s32 voice, u32 pitch);
    // Raw VOLL/VOLR registers (see PsxSpuVolume::SetRegister)
    void SetVoiceVolumeRegisters(s32 voice, u16 left, u16 right);
    // Fixed volume, -0x8000 to 0x7FFE (0x3FFF register = 0x7FFE)
    void SetVoiceVolume(s32 voice, s16 left, s16 right);
    void SetVoiceAdsr(s32 voice, u16 adsr1, u16 adsr2);
    // EON: the voice goes to the reverb as well as the mixer
    void SetVoiceReverb(s32 voice, bool enable);

    // KON/KOFF for one voice
    void KeyOn(s32 voice);
    void KeyOff(s32 voice);
    // KON/KOFF with a bit per voice, like the registers: they take effect after the next sample
    // Render makes (as DuckStation does it, from hardware tests), a voice's key off before its key
    // on. KeyOn/KeyOff take effect at once.
    void KeyOnMask(u32 voices);
    void KeyOffMask(u32 voices);
    // Silences a voice at once (level 0, no release), like a one shot sample's end. Not a hardware
    // register: for stealing a voice, or before freeing the sample it plays.
    void StopVoice(s32 voice);

    // ENDX: a bit per voice, set when the voice reaches its sample's (loop) end, cleared by key on.
    // Every voice starts with its bit set.
    u32 Endx() const
    {
        return mEndx;
    }

    // ENVX, the voice's current ADSR level (0-0x7FFF)
    s16 VoiceEnvelope(s32 voice) const;
    PsxSpuEnvelope::Phase VoicePhase(s32 voice) const;
    // False once a voice has released to 0 or reached the end of a one shot sample. A voice that
    // isn't active is free for the next note.
    bool IsVoiceActive(s32 voice) const;

    // MVOLL/MVOLR, raw registers
    void SetMasterVolumeRegisters(u16 left, u16 right);
    void SetMasterVolume(s16 left, s16 right);

    // The reverb (EVOL, the reverb output volume, is the reverb depth)
    void SetReverbPreset(PsxSpuReverb::Preset preset)
    {
        mReverb.SetPreset(preset);
    }
    void SetReverbDepth(s16 left, s16 right)
    {
        mReverb.SetOutputVolume(left, right);
    }
    // ATTR bit 7, the reverb master enable
    void SetReverbEnabled(bool enable)
    {
        mReverb.SetEnabled(enable);
    }
    PsxSpuReverb& Reverb()
    {
        return mReverb;
    }

    void SetInterpolation(Interpolation interpolation)
    {
        mInterpolation = interpolation;
    }

    Interpolation CurrentInterpolation() const
    {
        return mInterpolation;
    }

    // Mixes frames 44100 Hz stereo samples into out (left, right, left, ...), overwriting it
    void Render(s16* out, u32 frames);

    // Pitch helpers for the libsnd layer

    // PITCH register for a sample authored at sampleRate Hz, played at its natural speed
    static u16 PitchFromSampleRate(u32 sampleRate);

    // PITCH register to play note + fine (0-127, 1/128 semitones) on a VAB tone with root note
    // centre and fine tune shift (1/128 semitones), exactly as libsnd's SsPitchFromNote does it.
    // The shift RAISES the pitch: it's added to the note's fine tune, not to the root note. The
    // tone's sample plays at 44100 Hz (0x1000) at its root note. The result is rounded down to 1/16
    // semitones, and isn't clamped: values above kMaxPitch play at kMaxPitch.
    static u32 PitchFromNote(s32 note, s32 fine, s32 centre, s32 shift);

    // sample's mPcm low-passed for PsxSpuSample::mFiltered[level - 1]. A looping sample is filtered
    // as the loop repeats.
    static std::vector<s16> FilterSample(const PsxSpuSample& sample, s32 level);
    // How many low-passed copies a sample needs (0-kPsxSpuFilterLevels) for the band-limited
    // interpolation to play it at up to maxPitch at its normal cost
    static s32 FilterLevelsFor(u32 maxPitch);

    // The 512 entry 4 point interpolation table ("gaussian"), from psx-spx
    static const std::array<s16, 512>& GaussTable();

    // 4 point interpolation of oldest, older, old, new at index i (0-255, pitch counter bits 4-11)
    static s32 Interpolate(const s16 samples[4], u32 i);

private:
    struct Voice final
    {
        PsxSpuSample mNextSample; // Set by SetVoiceSample, used from the next key on
        PsxSpuSample mSample;
        u32 mPitch = 0;
        PsxSpuVolume mVolumeLeft;
        PsxSpuVolume mVolumeRight;
        PsxSpuEnvelope mEnvelope;
        bool mReverb = false;
        bool mPlaying = false; // Reading the sample (false after a one shot sample's end)

        u32 mCounter = 0;  // Pitch counter, bits 0-11: fraction, bits 4-11: interpolation index
        u32 mPosition = 0; // Next sample to read
        s16 mHistory[4] = {}; // oldest, older, old, new
        u32 mSamplesRead = 0; // Since key on, loops unrolled: mHistory[3] is sample mSamplesRead - 1
    };

    bool ValidVoice(s32 voice) const
    {
        return voice >= 0 && voice < kNumVoices;
    }

    s16 ReadNextSample(s32 voiceIdx, Voice& voice);
    // The sample at index (since key on, loops unrolled) without moving the voice on: 0 before the
    // key on and after a one shot sample's end
    static s32 SampleAt(const Voice& voice, s64 index);
    // The same from a sample's PCM or one of its low-passed copies (level 1-3)
    static s32 SampleAt(const PsxSpuSample& sample, s64 index, s32 level);
    static s32 BandLimitedSample(const Voice& voice);
    void TickVoice(s32 voiceIdx, Voice& voice, s32& left, s32& right, s32& reverbLeft, s32& reverbRight);

    void ApplyPendingKeys();

    std::array<Voice, kNumVoices> mVoices;
    u32 mPendingKeyOn = 0;
    u32 mPendingKeyOff = 0;
    Interpolation mInterpolation = Interpolation::Gaussian;
    u32 mEndx = 0;
    PsxSpuVolume mMasterLeft;
    PsxSpuVolume mMasterRight;
    PsxSpuReverb mReverb;
};
