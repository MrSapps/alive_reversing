#include "PsxSpuReverb.hpp"
#include <algorithm>
#include <cstring>

// Implemented from psx-spx, "SPU Reverb Registers", "SPU Reverb Formula" and
// "SPU Reverb Examples".

struct ReverbPresetData final
{
    u32 mWorkAreaBytes;
    u16 mRegs[32]; // In PsxSpuReverb::Registers order
};

static_assert(sizeof(PsxSpuReverb::Registers) == sizeof(u16) * 32, "Registers must be the 32 SPU reverb registers");

// psx-spx "SPU Reverb Examples", in PsxSpuReverb::Preset order
static const ReverbPresetData kReverbPresets[PsxSpuReverb::kNumPresets] = {
    // Reverb off, 0x10 bytes
    {0x10, {
        0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001,
        0x0000, 0x0000, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001, 0x0001,
        0x0000, 0x0000, 0x0001, 0x0001, 0x0001, 0x0001, 0x0000, 0x0000,
    }},
    // Room, 0x26C0 bytes
    {0x26C0, {
        0x007D, 0x005B, 0x6D80, 0x54B8, 0xBED0, 0x0000, 0x0000, 0xBA80,
        0x5800, 0x5300, 0x04D6, 0x0333, 0x03F0, 0x0227, 0x0374, 0x01EF,
        0x0334, 0x01B5, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x01B4, 0x0136, 0x00B8, 0x005C, 0x8000, 0x8000,
    }},
    // Studio Small, 0x1F40 bytes
    {0x1F40, {
        0x0033, 0x0025, 0x70F0, 0x4FA8, 0xBCE0, 0x4410, 0xC0F0, 0x9C00,
        0x5280, 0x4EC0, 0x03E4, 0x031B, 0x03A4, 0x02AF, 0x0372, 0x0266,
        0x031C, 0x025D, 0x025C, 0x018E, 0x022F, 0x0135, 0x01D2, 0x00B7,
        0x018F, 0x00B5, 0x00B4, 0x0080, 0x004C, 0x0026, 0x8000, 0x8000,
    }},
    // Studio Medium, 0x4840 bytes
    {0x4840, {
        0x00B1, 0x007F, 0x70F0, 0x4FA8, 0xBCE0, 0x4510, 0xBEF0, 0xB4C0,
        0x5280, 0x4EC0, 0x0904, 0x076B, 0x0824, 0x065F, 0x07A2, 0x0616,
        0x076C, 0x05ED, 0x05EC, 0x042E, 0x050F, 0x0305, 0x0462, 0x02B7,
        0x042F, 0x0265, 0x0264, 0x01B2, 0x0100, 0x0080, 0x8000, 0x8000,
    }},
    // Studio Large, 0x6FE0 bytes
    {0x6FE0, {
        0x00E3, 0x00A9, 0x6F60, 0x4FA8, 0xBCE0, 0x4510, 0xBEF0, 0xA680,
        0x5680, 0x52C0, 0x0DFB, 0x0B58, 0x0D09, 0x0A3C, 0x0BD9, 0x0973,
        0x0B59, 0x08DA, 0x08D9, 0x05E9, 0x07EC, 0x04B0, 0x06EF, 0x03D2,
        0x05EA, 0x031D, 0x031C, 0x0238, 0x0154, 0x00AA, 0x8000, 0x8000,
    }},
    // Hall, 0xADE0 bytes
    {0xADE0, {
        0x01A5, 0x0139, 0x6000, 0x5000, 0x4C00, 0xB800, 0xBC00, 0xC000,
        0x6000, 0x5C00, 0x15BA, 0x11BB, 0x14C2, 0x10BD, 0x11BC, 0x0DC1,
        0x11C0, 0x0DC3, 0x0DC0, 0x09C1, 0x0BC4, 0x07C1, 0x0A00, 0x06CD,
        0x09C2, 0x05C1, 0x05C0, 0x041A, 0x0274, 0x013A, 0x8000, 0x8000,
    }},
    // Space Echo, 0xF6C0 bytes
    {0xF6C0, {
        0x033D, 0x0231, 0x7E00, 0x5000, 0xB400, 0xB000, 0x4C00, 0xB000,
        0x6000, 0x5400, 0x1ED6, 0x1A31, 0x1D14, 0x183B, 0x1BC2, 0x16B2,
        0x1A32, 0x15EF, 0x15EE, 0x1055, 0x1334, 0x0F2D, 0x11F6, 0x0C5D,
        0x1056, 0x0AE1, 0x0AE0, 0x07A2, 0x0464, 0x0232, 0x8000, 0x8000,
    }},
    // Chaos Echo (almost infinite), 0x18040 bytes
    {0x18040, {
        0x0001, 0x0001, 0x7FFF, 0x7FFF, 0x0000, 0x0000, 0x0000, 0x8100,
        0x0000, 0x0000, 0x1FFF, 0x0FFF, 0x1005, 0x0005, 0x0000, 0x0000,
        0x1005, 0x0005, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x1004, 0x1002, 0x0004, 0x0002, 0x8000, 0x8000,
    }},
    // Delay (one-shot echo), 0x18040 bytes
    {0x18040, {
        0x0001, 0x0001, 0x7FFF, 0x7FFF, 0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x1FFF, 0x0FFF, 0x1005, 0x0005, 0x0000, 0x0000,
        0x1005, 0x0005, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
        0x0000, 0x0000, 0x1004, 0x1002, 0x0004, 0x0002, 0x8000, 0x8000,
    }},
    // Half Echo, 0x3C00 bytes
    {0x3C00, {
        0x0017, 0x0013, 0x70F0, 0x4FA8, 0xBCE0, 0x4510, 0xBEF0, 0x8500,
        0x5F80, 0x54C0, 0x0371, 0x02AF, 0x02E5, 0x01DF, 0x02B0, 0x01D7,
        0x0358, 0x026A, 0x01D6, 0x011E, 0x012D, 0x00B1, 0x011F, 0x0059,
        0x01A0, 0x00E3, 0x0058, 0x0040, 0x0028, 0x0014, 0x8000, 0x8000,
    }},
};

// The 39 tap filter the SPU resamples the reverb input (44100 -> 22050 Hz) and output
// (22050 -> 44100 Hz) with, psx-spx "Reverb Buffer Resampling". It's a half band filter: every
// odd tap but the centre one is 0.
static const s16 kReverbFir[39] = {
    -0x0001, 0x0000, 0x0002, 0x0000, -0x000A, 0x0000, 0x0023, 0x0000,
    -0x0067, 0x0000, 0x010A, 0x0000, -0x0268, 0x0000, 0x0534, 0x0000,
    -0x0B90, 0x0000, 0x2806, 0x4000, 0x2806, 0x0000, -0x0B90, 0x0000,
    0x0534, 0x0000, -0x0268, 0x0000, 0x010A, 0x0000, -0x0067, 0x0000,
    0x0023, 0x0000, -0x000A, 0x0000, 0x0002, 0x0000, -0x0001,
};

static u32 MaxWorkAreaBytes()
{
    u32 ret = 0;
    for (const ReverbPresetData& preset : kReverbPresets)
    {
        ret = std::max(ret, preset.mWorkAreaBytes);
    }
    return ret;
}

static s32 Saturate16(s32 value)
{
    return std::clamp(value, -0x8000, 0x7FFF);
}

// A volume multiply: the result is divided by 0x8000 and saturated. psx-spx measured that the
// hardware saturates intermediate values to 16 bits, not only the values it writes to memory.
static s32 Mul(s32 a, s32 b)
{
    return Saturate16((a * b) >> 15);
}

// Register address (in 8 byte units) to 16 bit words
static s32 Words(u16 reg)
{
    return static_cast<s32>(reg) * 4;
}

const PsxSpuReverb::Registers& PsxSpuReverb::PresetRegisters(Preset preset)
{
    static const std::array<Registers, kNumPresets> sRegs = []()
    {
        std::array<Registers, kNumPresets> regs = {};
        for (s32 i = 0; i < kNumPresets; i++)
        {
            memcpy(&regs[i], kReverbPresets[i].mRegs, sizeof(Registers));
        }
        return regs;
    }();
    return sRegs[static_cast<s32>(preset) % kNumPresets];
}

u32 PsxSpuReverb::PresetWorkAreaBytes(Preset preset)
{
    return kReverbPresets[static_cast<s32>(preset) % kNumPresets].mWorkAreaBytes;
}

PsxSpuReverb::PsxSpuReverb()
{
    // The one allocation: big enough for every preset, so changing preset never allocates
    mWorkArea.resize(MaxWorkAreaBytes() / sizeof(s16));
    Reset();
}

void PsxSpuReverb::Reset()
{
    mInputLeft.fill(0);
    mInputRight.fill(0);
    mInputPos = 0;
    mWetLeft.fill(0);
    mWetRight.fill(0);
    mWetPos = 0;
    // The reverb runs on the second sample after a reset, then every other one. Checked against
    // DuckStation's SPU playing the same register writes: with the other phase the output differed
    // by 34 dB under the signal, with this by 60 dB (rounding).
    mOddSample = false;
    SetPreset(Preset::Off);
}

void PsxSpuReverb::SetPreset(Preset preset)
{
    mPreset = preset;
    mRegs = PresetRegisters(preset);
    mWorkAreaWords = PresetWorkAreaBytes(preset) / sizeof(s16);
    // Writing mBASE starts the buffer at the work area's start. The 22050 Hz phase and the
    // resampling filters carry on.
    mBufferAddress = 0;
    Clear();
}

void PsxSpuReverb::Clear()
{
    // SpuClearReverbWorkArea only zeroes the work area in SPU RAM
    std::fill(mWorkArea.begin(), mWorkArea.end(), static_cast<s16>(0));
}

s16 PsxSpuReverb::DepthToVolume(s32 depth)
{
    return static_cast<s16>(std::clamp(depth, 0, 127) * 0x7FFF / 127);
}

s16 PsxSpuReverb::Read(s32 wordOffset) const
{
    const s32 size = static_cast<s32>(mWorkAreaWords);
    s32 addr = (static_cast<s32>(mBufferAddress) + wordOffset) % size;
    if (addr < 0)
    {
        addr += size;
    }
    return mWorkArea[addr];
}

void PsxSpuReverb::Write(s32 wordOffset, s32 value)
{
    if (!mEnabled)
    {
        return;
    }
    const s32 size = static_cast<s32>(mWorkAreaWords);
    s32 addr = (static_cast<s32>(mBufferAddress) + wordOffset) % size;
    if (addr < 0)
    {
        addr += size;
    }
    mWorkArea[addr] = static_cast<s16>(Saturate16(value));
}

// x * (0x8000 - alpha), how the IIR weighs the old value: as the hardware does it, including
// alpha = -8000h (Mednafen's measurements)
static s32 IirOld(s32 x, s16 alpha)
{
    if (alpha == -0x8000)
    {
        return x == -0x8000 ? 0 : x * -0x10000;
    }
    return x * (0x8000 - alpha);
}

// -x, with -8000h giving 7FFFh
static s32 Negate(s32 x)
{
    return x == -0x8000 ? 0x7FFF : -x;
}

void PsxSpuReverb::TickChannel(s32 in, s16 vIn, u16 mSame, u16 dSame, u16 mDiff, u16 dDiff, u16 mComb1, u16 mComb2, u16 mComb3, u16 mComb4, u16 mApf1, u16 mApf2, s32& wet)
{
    const Registers& r = mRegs;

    // The psx-spx formula, with each step's rounding and saturation where the hardware has them
    // (as Mednafen and DuckStation measured it). The reflections only write to the work area, so
    // with writes off (ATTR bit 7 clear) they do nothing.
    if (mEnabled)
    {
        // Same side reflection
        const s32 sameIn = Saturate16((((Read(Words(dSame)) * r.vWALL) >> 14) + ((in * vIn) >> 14)) >> 1);
        const s32 same = Saturate16((((sameIn * r.vIIR) >> 14) + (IirOld(Read(Words(mSame) - 1), r.vIIR) >> 14)) >> 1);
        // Different side reflection
        const s32 diffIn = Saturate16((((Read(Words(dDiff)) * r.vWALL) >> 14) + ((in * vIn) >> 14)) >> 1);
        const s32 diff = Saturate16((((diffIn * r.vIIR) >> 14) + (IirOld(Read(Words(mDiff) - 1), r.vIIR) >> 14)) >> 1);
        Write(Words(mSame), same);
        Write(Words(mDiff), diff);
    }

    // Early echo (combs), not saturated
    const s32 comb = ((Read(Words(mComb1)) * r.vCOMB1) >> 14) + ((Read(Words(mComb2)) * r.vCOMB2) >> 14) + ((Read(Words(mComb3)) * r.vCOMB3) >> 14)
                   + ((Read(Words(mComb4)) * r.vCOMB4) >> 14);

    // Late reverb: the two all pass filters
    const s32 apf1Delayed = Read(Words(mApf1) - Words(r.dAPF1));
    const s32 apf2Delayed = Read(Words(mApf2) - Words(r.dAPF2));
    const s32 apf1 = Saturate16((comb + ((apf1Delayed * Negate(r.vAPF1)) >> 14)) >> 1);
    const s32 apf2 = Saturate16(apf1Delayed + ((((apf1 * r.vAPF1) >> 14) + ((apf2Delayed * Negate(r.vAPF2)) >> 14)) >> 1));
    wet = Saturate16(apf2Delayed + ((apf2 * r.vAPF2) >> 15));
    Write(Words(mApf1), apf1);
    Write(Words(mApf2), apf2);
}

void PsxSpuReverb::Tick22050(s32 inLeft, s32 inRight, s32& wetLeft, s32& wetRight)
{
    const Registers& r = mRegs;
    // The left channel is done before the right, as the hardware does: a right one could read a
    // word the left one wrote
    TickChannel(inLeft, r.vLIN, r.mLSAME, r.dLSAME, r.mLDIFF, r.dRDIFF, r.mLCOMB1, r.mLCOMB2, r.mLCOMB3, r.mLCOMB4, r.mLAPF1, r.mLAPF2, wetLeft);
    TickChannel(inRight, r.vRIN, r.mRSAME, r.dRSAME, r.mRDIFF, r.dLDIFF, r.mRCOMB1, r.mRCOMB2, r.mRCOMB3, r.mRCOMB4, r.mRAPF1, r.mRAPF2, wetRight);

    // BufferAddress = MAX(ESA, (BufferAddress + 2) AND 7FFFEh): the next word, wrapping in the
    // work area
    mBufferAddress = (mBufferAddress + 1) % mWorkAreaWords;
}

void PsxSpuReverb::Process(s32 inLeft, s32 inRight, s32& outLeft, s32& outRight)
{
    mInputLeft[mInputPos] = static_cast<s16>(Saturate16(inLeft));
    mInputRight[mInputPos] = static_cast<s16>(Saturate16(inRight));

    if (mOddSample)
    {
        // Every other sample: filter the input down to 22050 Hz and run the reverb once
        s32 downLeft = 0;
        s32 downRight = 0;
        for (u32 k = 0; k < kFirTaps; k++)
        {
            const u32 idx = (mInputPos - k) & (kInputHistory - 1);
            downLeft += kReverbFir[k] * mInputLeft[idx];
            downRight += kReverbFir[k] * mInputRight[idx];
        }

        s32 wetLeft = 0;
        s32 wetRight = 0;
        Tick22050(Saturate16(downLeft >> 15), Saturate16(downRight >> 15), wetLeft, wetRight);

        mWetPos = (mWetPos + 1) & (kOutputHistory - 1);
        mWetLeft[mWetPos] = static_cast<s16>(wetLeft);
        mWetRight[mWetPos] = static_cast<s16>(wetRight);

        // Up to 44100 Hz: the output is the wet stream with a 0 between each sample, filtered.
        // On this phase the even taps meet the wet samples. They sum to ~0x4000, so the gain of 2
        // that zero stuffing needs makes the shift 14.
        s32 upLeft = 0;
        s32 upRight = 0;
        for (u32 j = 0; j < (kFirTaps + 1) / 2; j++)
        {
            const u32 idx = (mWetPos - j) & (kOutputHistory - 1);
            upLeft += kReverbFir[j * 2] * mWetLeft[idx];
            upRight += kReverbFir[j * 2] * mWetRight[idx];
        }
        outLeft = Saturate16(upLeft >> 14);
        outRight = Saturate16(upRight >> 14);
    }
    else
    {
        // On this phase only the centre tap (0x4000, times the gain of 2) meets a wet sample
        const u32 idx = (mWetPos - 9) & (kOutputHistory - 1);
        outLeft = mWetLeft[idx];
        outRight = mWetRight[idx];
    }
    // The output volume (the reverb depth) is applied to the 44100 Hz output, so a change takes
    // effect at once
    outLeft = Mul(outLeft, mOutputVolumeLeft);
    outRight = Mul(outRight, mOutputVolumeRight);

    mInputPos = (mInputPos + 1) & (kInputHistory - 1);
    mOddSample = !mOddSample;
}
