#pragma once

#include "../../Types.hpp"
#include <array>
#include <vector>

// The PS1 SPU reverb unit, from psx-spx "SPU Reverb Formula": same side and different side
// reflections, four combs and two all pass filters, run at 22050 Hz on a 16 bit work area. Its
// 44100 Hz input and output go through the SPU's 39 tap half band resampling filter.
//
// Real time safe: the work area is allocated once, at the size of the largest preset, and
// Process never allocates.
class PsxSpuReverb final
{
public:
    // The libsnd reverb modes (SPU_REV_MODE_*), in libsnd's order. psx-spx names Pipe
    // "Half Echo" and Echo "Chaos Echo".
    enum class Preset : u8
    {
        Off = 0,
        Room = 1,
        StudioSmall = 2,
        StudioMedium = 3,
        StudioLarge = 4,
        Hall = 5,
        SpaceEcho = 6,
        Echo = 7,
        Delay = 8,
        Pipe = 9,
    };
    static constexpr s32 kNumPresets = 10;

    // The reverb configuration registers (1F801DC0h-1F801DFEh), in register order
    struct Registers final
    {
        u16 dAPF1, dAPF2;
        s16 vIIR, vCOMB1, vCOMB2, vCOMB3, vCOMB4, vWALL, vAPF1, vAPF2;
        u16 mLSAME, mRSAME, mLCOMB1, mRCOMB1, mLCOMB2, mRCOMB2;
        u16 dLSAME, dRSAME, mLDIFF, mRDIFF, mLCOMB3, mRCOMB3, mLCOMB4, mRCOMB4;
        u16 dLDIFF, dRDIFF, mLAPF1, mRAPF1, mLAPF2, mRAPF2;
        s16 vLIN, vRIN;
    };

    // A preset's registers and its work area size in bytes
    static const Registers& PresetRegisters(Preset preset);
    static u32 PresetWorkAreaBytes(Preset preset);

    PsxSpuReverb();

    // Loads the preset's registers and work area size and clears the work area, like
    // SpuSetReverbModeType + SpuClearReverbWorkArea
    void SetPreset(Preset preset);
    Preset CurrentPreset() const
    {
        return mPreset;
    }

    // EVOLL/EVOLR, the reverb output volume (libsnd's "depth")
    void SetOutputVolume(s16 left, s16 right)
    {
        mOutputVolumeLeft = left;
        mOutputVolumeRight = right;
    }

    // ATTR bit 7. Off stops the writes to the work area, but the reads (and so what the work area
    // already holds) still reach the output, as on the SPU.
    void SetEnabled(bool enable)
    {
        mEnabled = enable;
    }

    // Zero fills the work area and the resampler state
    void Clear();
    // SPU reset: preset off, and the resampling and 22050 Hz phase start again
    void Reset();

    // One 44100 Hz sample: in is the mix of the voices that have reverb on, out is the reverb's
    // output (already multiplied by the output volume)
    void Process(s32 inLeft, s32 inRight, s32& outLeft, s32& outRight);

    // libsnd style depth (0-127) to an output volume register value
    static s16 DepthToVolume(s32 depth);

private:
    static constexpr u32 kFirTaps = 39;
    static constexpr u32 kInputHistory = 64;  // Power of 2 >= kFirTaps
    static constexpr u32 kOutputHistory = 32; // Power of 2 >= 20 wet samples

    // One channel's reverb at 22050 Hz
    void TickChannel(s32 in, s16 vIn, u16 mSame, u16 dSame, u16 mDiff, u16 dDiff, u16 mComb1, u16 mComb2, u16 mComb3, u16 mComb4, u16 mApf1, u16 mApf2, s32& wet);
    void Tick22050(s32 inLeft, s32 inRight, s32& wetLeft, s32& wetRight);

    // Work area access, offset in 16 bit words from the current buffer address
    s16 Read(s32 wordOffset) const;
    void Write(s32 wordOffset, s32 value);

    std::vector<s16> mWorkArea;
    u32 mWorkAreaWords = 8;
    u32 mBufferAddress = 0;

    Registers mRegs = {};
    Preset mPreset = Preset::Off;
    bool mEnabled = false;
    s16 mOutputVolumeLeft = 0;
    s16 mOutputVolumeRight = 0;

    // Resampler state
    std::array<s16, kInputHistory> mInputLeft = {};
    std::array<s16, kInputHistory> mInputRight = {};
    u32 mInputPos = 0;
    std::array<s16, kOutputHistory> mWetLeft = {};
    std::array<s16, kOutputHistory> mWetRight = {};
    u32 mWetPos = 0;
    bool mOddSample = false;
};
