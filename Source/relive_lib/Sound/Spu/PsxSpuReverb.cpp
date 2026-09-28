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
    SetPreset(Preset::Off);
}

void PsxSpuReverb::SetPreset(Preset preset)
{
    mPreset = preset;
    mRegs = PresetRegisters(preset);
    mWorkAreaWords = PresetWorkAreaBytes(preset) / sizeof(s16);
    Clear();
}

void PsxSpuReverb::Clear()
{
    std::fill(mWorkArea.begin(), mWorkArea.end(), static_cast<s16>(0));
    mBufferAddress = 0;
    mInputLeft.fill(0);
    mInputRight.fill(0);
    mInputPos = 0;
    mWetLeft.fill(0);
    mWetRight.fill(0);
    mWetPos = 0;
    mOddSample = false;
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

void PsxSpuReverb::Tick22050(s32 inLeft, s32 inRight, s32& wetLeft, s32& wetRight)
{
    const Registers& r = mRegs;

    // Input from the mixer
    const s32 lIn = Mul(r.vLIN, inLeft);
    const s32 rIn = Mul(r.vRIN, inRight);

    // The reflections only write to the work area, so with writes off (ATTR bit 7 clear) they do
    // nothing: psx-spx "Reverb Disable"
    if (mEnabled)
    {
        // Same side reflection (L to L, R to R)
        {
            const s32 lPrev = Read(Words(r.mLSAME) - 1);
            const s32 rPrev = Read(Words(r.mRSAME) - 1);
            const s32 l = Saturate16(lIn + Mul(Read(Words(r.dLSAME)), r.vWALL) - lPrev);
            const s32 rr = Saturate16(rIn + Mul(Read(Words(r.dRSAME)), r.vWALL) - rPrev);
            Write(Words(r.mLSAME), Mul(l, r.vIIR) + lPrev);
            Write(Words(r.mRSAME), Mul(rr, r.vIIR) + rPrev);
        }

        // Different side reflection (R to L, L to R)
        {
            const s32 lPrev = Read(Words(r.mLDIFF) - 1);
            const s32 rPrev = Read(Words(r.mRDIFF) - 1);
            const s32 l = Saturate16(lIn + Mul(Read(Words(r.dRDIFF)), r.vWALL) - lPrev);
            const s32 rr = Saturate16(rIn + Mul(Read(Words(r.dLDIFF)), r.vWALL) - rPrev);
            Write(Words(r.mLDIFF), Mul(l, r.vIIR) + lPrev);
            Write(Words(r.mRDIFF), Mul(rr, r.vIIR) + rPrev);
        }
    }

    // Early echo (combs)
    s32 lOut = Saturate16(Mul(r.vCOMB1, Read(Words(r.mLCOMB1))) + Mul(r.vCOMB2, Read(Words(r.mLCOMB2))) + Mul(r.vCOMB3, Read(Words(r.mLCOMB3))) + Mul(r.vCOMB4, Read(Words(r.mLCOMB4))));
    s32 rOut = Saturate16(Mul(r.vCOMB1, Read(Words(r.mRCOMB1))) + Mul(r.vCOMB2, Read(Words(r.mRCOMB2))) + Mul(r.vCOMB3, Read(Words(r.mRCOMB3))) + Mul(r.vCOMB4, Read(Words(r.mRCOMB4))));

    // Late reverb, all pass filter 1
    {
        const s32 lDelayed = Read(Words(r.mLAPF1) - Words(r.dAPF1));
        const s32 rDelayed = Read(Words(r.mRAPF1) - Words(r.dAPF1));
        lOut = Saturate16(lOut - Mul(r.vAPF1, lDelayed));
        rOut = Saturate16(rOut - Mul(r.vAPF1, rDelayed));
        Write(Words(r.mLAPF1), lOut);
        Write(Words(r.mRAPF1), rOut);
        lOut = Saturate16(Mul(lOut, r.vAPF1) + lDelayed);
        rOut = Saturate16(Mul(rOut, r.vAPF1) + rDelayed);
    }

    // Late reverb, all pass filter 2
    {
        const s32 lDelayed = Read(Words(r.mLAPF2) - Words(r.dAPF2));
        const s32 rDelayed = Read(Words(r.mRAPF2) - Words(r.dAPF2));
        lOut = Saturate16(lOut - Mul(r.vAPF2, lDelayed));
        rOut = Saturate16(rOut - Mul(r.vAPF2, rDelayed));
        Write(Words(r.mLAPF2), lOut);
        Write(Words(r.mRAPF2), rOut);
        lOut = Saturate16(Mul(lOut, r.vAPF2) + lDelayed);
        rOut = Saturate16(Mul(rOut, r.vAPF2) + rDelayed);
    }

    // Output to the mixer
    wetLeft = Mul(lOut, mOutputVolumeLeft);
    wetRight = Mul(rOut, mOutputVolumeRight);

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

    mInputPos = (mInputPos + 1) & (kInputHistory - 1);
    mOddSample = !mOddSample;
}
