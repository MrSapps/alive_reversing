#include "PsxSpu.hpp"
#include <algorithm>
#include <cmath>
#include <vector>

// Implemented from psx-spx, "Sound Processing Unit (SPU)": "SPU ADPCM Pitch" (pitch counter,
// 4 point interpolation), "SPU Volume and ADSR Generator" and "SPU Voice Flags".

// psx-spx "4-Point Gaussian Interpolation"
static const std::array<s16, 512> kGaussTable = {{
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    0, 0, 0, 0, 0, 0, 0, 1,
    1, 1, 1, 2, 2, 2, 3, 3,
    3, 4, 4, 5, 5, 6, 7, 7,
    8, 9, 9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 21, 22, 24,
    25, 27, 28, 30, 32, 33, 35, 37,
    39, 41, 44, 46, 48, 51, 53, 56,
    58, 61, 64, 67, 70, 73, 77, 80,
    84, 87, 91, 95, 99, 103, 107, 111,
    116, 120, 125, 130, 135, 140, 145, 150,
    156, 161, 167, 173, 179, 186, 192, 199,
    205, 212, 219, 227, 234, 242, 250, 257,
    266, 274, 283, 291, 300, 309, 319, 328,
    338, 348, 358, 369, 379, 390, 401, 412,
    424, 436, 448, 460, 473, 485, 498, 512,
    525, 539, 553, 567, 582, 597, 612, 627,
    643, 659, 675, 692, 708, 726, 743, 761,
    779, 797, 816, 835, 854, 874, 894, 914,
    935, 956, 977, 999, 1020, 1043, 1066, 1089,
    1112, 1136, 1160, 1184, 1209, 1234, 1260, 1286,
    1312, 1339, 1366, 1394, 1422, 1450, 1479, 1508,
    1537, 1567, 1598, 1628, 1660, 1691, 1723, 1756,
    1789, 1822, 1856, 1890, 1924, 1959, 1995, 2031,
    2067, 2104, 2141, 2179, 2217, 2256, 2295, 2334,
    2374, 2415, 2456, 2497, 2539, 2582, 2624, 2668,
    2712, 2756, 2801, 2846, 2892, 2938, 2985, 3032,
    3079, 3128, 3176, 3225, 3275, 3325, 3376, 3427,
    3479, 3531, 3584, 3637, 3691, 3745, 3799, 3855,
    3910, 3967, 4023, 4081, 4138, 4197, 4255, 4315,
    4374, 4435, 4495, 4557, 4619, 4681, 4744, 4807,
    4871, 4935, 5000, 5065, 5131, 5197, 5264, 5332,
    5399, 5468, 5536, 5606, 5676, 5746, 5817, 5888,
    5959, 6032, 6104, 6177, 6251, 6325, 6400, 6475,
    6550, 6626, 6702, 6779, 6856, 6934, 7012, 7091,
    7170, 7249, 7329, 7409, 7490, 7571, 7653, 7735,
    7817, 7900, 7983, 8066, 8150, 8234, 8319, 8404,
    8489, 8575, 8661, 8748, 8834, 8922, 9009, 9097,
    9185, 9273, 9362, 9451, 9541, 9630, 9720, 9811,
    9901, 9992, 10083, 10174, 10266, 10358, 10450, 10542,
    10635, 10727, 10820, 10913, 11007, 11100, 11194, 11288,
    11382, 11476, 11571, 11665, 11760, 11855, 11950, 12045,
    12140, 12236, 12331, 12427, 12522, 12618, 12714, 12809,
    12905, 13001, 13097, 13193, 13289, 13385, 13481, 13577,
    13673, 13769, 13865, 13961, 14056, 14152, 14248, 14343,
    14439, 14534, 14630, 14725, 14820, 14915, 15010, 15104,
    15199, 15293, 15387, 15481, 15575, 15669, 15762, 15855,
    15948, 16041, 16133, 16226, 16317, 16409, 16500, 16592,
    16682, 16773, 16863, 16953, 17042, 17131, 17220, 17308,
    17396, 17484, 17571, 17658, 17744, 17830, 17916, 18001,
    18086, 18170, 18254, 18337, 18420, 18502, 18584, 18665,
    18746, 18826, 18905, 18985, 19063, 19141, 19219, 19295,
    19372, 19447, 19522, 19597, 19671, 19744, 19816, 19888,
    19959, 20030, 20100, 20169, 20238, 20306, 20373, 20439,
    20505, 20570, 20634, 20698, 20760, 20822, 20884, 20944,
    21004, 21063, 21121, 21178, 21235, 21290, 21345, 21399,
    21452, 21505, 21556, 21607, 21657, 21706, 21754, 21801,
    21848, 21893, 21938, 21982, 22025, 22066, 22107, 22148,
    22187, 22225, 22262, 22299, 22334, 22369, 22402, 22435,
    22467, 22498, 22527, 22556, 22584, 22611, 22637, 22662,
    22686, 22709, 22731, 22752, 22772, 22791, 22809, 22826,
    22842, 22857, 22872, 22885, 22897, 22908, 22918, 22927,
    22935, 22942, 22948, 22953, 22957, 22960, 22962, 22963,
}};

static constexpr u32 kAllVoices = (1u << PsxSpu::kNumVoices) - 1;

static s16 Clamp16(s32 value)
{
    return static_cast<s16>(std::clamp(value, -0x8000, 0x7FFF));
}

// ---------------------------------------------------------------------------------------------
// Envelope

void PsxSpuEnvelope::Step(const Rate& rate, s32& level, u32& counter)
{
    // "Using a step value of all-ones causes the volume to never step, and additionally never
    // saturate"
    if (rate.mNeverSteps)
    {
        return;
    }

    const s32 shift = rate.mShift;
    s32 step = 7 - rate.mStep;
    if (rate.mDecreasing != rate.mPhaseNegative)
    {
        step = ~step; // +7,+6,+5,+4 => -8,-7,-6,-5
    }
    step *= 1 << std::max(0, 11 - shift);
    u32 counterIncrement = 0x8000u >> std::max(0, shift - 11);

    if (rate.mExponential && !rate.mDecreasing && level > 0x6000)
    {
        // The "exponential" increase is a slower linear increase above 0x6000
        if (shift < 10)
        {
            step >>= 2;
        }
        else if (shift >= 11)
        {
            counterIncrement >>= 2;
        }
        else
        {
            step >>= 1;
            counterIncrement >>= 1;
        }
    }
    else if (rate.mExponential && rate.mDecreasing)
    {
        // AdsrStep * AdsrLevel / 8000h. An arithmetic shift (rounding towards minus infinity), so
        // a small level still gets a step of -1 and an exponential release can reach 0.
        step = (step * level) >> 15;
    }

    counterIncrement = std::max(counterIncrement, 1u);

    counter += counterIncrement;
    if ((counter & 0x8000) == 0)
    {
        return; // No step this cycle
    }
    counter &= 0x7FFF;

    level += step;
    if (!rate.mDecreasing)
    {
        level = std::clamp(level, -0x8000, 0x7FFF);
    }
    else if (rate.mPhaseNegative)
    {
        level = std::clamp(level, -0x8000, 0);
    }
    else
    {
        level = std::max(level, 0);
    }
}

s32 PsxSpuEnvelope::SustainLevel(u16 adsr1)
{
    return ((adsr1 & 0xF) + 1) * 0x800;
}

PsxSpuEnvelope::Rate PsxSpuEnvelope::CurrentRate() const
{
    Rate rate;
    switch (mPhase)
    {
        case Phase::Attack:
            rate.mExponential = (mAdsr1 & 0x8000) != 0;
            rate.mShift = static_cast<u8>((mAdsr1 >> 10) & 0x1F);
            rate.mStep = static_cast<u8>((mAdsr1 >> 8) & 3);
            rate.mNeverSteps = ((rate.mShift << 2) | rate.mStep) == 0x7F;
            break;

        case Phase::Decay:
            // Always exponential, decreasing, step "-8"
            rate.mExponential = true;
            rate.mDecreasing = true;
            rate.mShift = static_cast<u8>((mAdsr1 >> 4) & 0xF);
            break;

        case Phase::Sustain:
            rate.mExponential = (mAdsr2 & 0x8000) != 0;
            rate.mDecreasing = (mAdsr2 & 0x4000) != 0;
            rate.mShift = static_cast<u8>((mAdsr2 >> 8) & 0x1F);
            rate.mStep = static_cast<u8>((mAdsr2 >> 6) & 3);
            rate.mNeverSteps = ((rate.mShift << 2) | rate.mStep) == 0x7F;
            break;

        case Phase::Release:
            // Always decreasing, step "-8". psx-spx: the release never steps when its shift is
            // all ones (1Fh).
            rate.mExponential = (mAdsr2 & 0x20) != 0;
            rate.mDecreasing = true;
            rate.mShift = static_cast<u8>(mAdsr2 & 0x1F);
            rate.mNeverSteps = rate.mShift == 0x1F;
            break;

        case Phase::Off:
            rate.mNeverSteps = true;
            break;
    }
    return rate;
}

void PsxSpuEnvelope::EnterPhase(Phase phase)
{
    mPhase = phase;
    mCounter = 0;
}

void PsxSpuEnvelope::KeyOn()
{
    mLevel = 0;
    EnterPhase(Phase::Attack);
}

void PsxSpuEnvelope::KeyOff()
{
    if (mPhase != Phase::Off)
    {
        EnterPhase(Phase::Release);
    }
}

void PsxSpuEnvelope::Mute()
{
    mLevel = 0;
    EnterPhase(Phase::Off);
}

void PsxSpuEnvelope::Tick()
{
    if (mPhase == Phase::Off)
    {
        return;
    }

    // A decay that starts at or below the sustain level (sustain level 0x8000) doesn't step
    if (mPhase == Phase::Decay && mLevel <= SustainLevel(mAdsr1))
    {
        EnterPhase(Phase::Sustain);
    }

    s32 level = mLevel;
    Step(CurrentRate(), level, mCounter);
    mLevel = static_cast<s16>(level);

    switch (mPhase)
    {
        case Phase::Attack:
            if (mLevel >= 0x7FFF)
            {
                EnterPhase(Phase::Decay);
            }
            break;

        case Phase::Decay:
            if (mLevel <= SustainLevel(mAdsr1))
            {
                EnterPhase(Phase::Sustain);
            }
            break;

        case Phase::Release:
            if (mLevel <= 0)
            {
                mLevel = 0;
                EnterPhase(Phase::Off);
            }
            break;

        case Phase::Sustain:
        case Phase::Off:
            break;
    }
}

// ---------------------------------------------------------------------------------------------
// Volume

void PsxSpuVolume::SetRegister(u16 reg)
{
    mRegister = reg;
    if ((reg & 0x8000) == 0)
    {
        // Bits 0-14 are the volume / 2, signed
        mCurrent = static_cast<s16>(static_cast<u16>(reg << 1));
    }
    // Sweep mode starts from the current volume
    mCounter = 0;
}

void PsxSpuVolume::Tick()
{
    if ((mRegister & 0x8000) == 0)
    {
        return;
    }

    PsxSpuEnvelope::Rate rate;
    rate.mExponential = (mRegister & 0x4000) != 0;
    rate.mDecreasing = (mRegister & 0x2000) != 0;
    rate.mPhaseNegative = (mRegister & 0x1000) != 0;
    rate.mShift = static_cast<u8>((mRegister >> 2) & 0x1F);
    rate.mStep = static_cast<u8>(mRegister & 3);
    rate.mNeverSteps = ((rate.mShift << 2) | rate.mStep) == 0x7F;

    s32 level = mCurrent;
    PsxSpuEnvelope::Step(rate, level, mCounter);
    mCurrent = static_cast<s16>(level);
}

// ---------------------------------------------------------------------------------------------
// SPU

PsxSpu::PsxSpu()
{
    Reset();
}

void PsxSpu::Reset()
{
    for (Voice& voice : mVoices)
    {
        voice = Voice();
    }
    mEndx = kAllVoices;
    mMasterLeft.SetRegister(0);
    mMasterRight.SetRegister(0);
    mReverb.SetEnabled(false);
    mReverb.SetOutputVolume(0, 0);
    mReverb.SetPreset(PsxSpuReverb::Preset::Off);
}

void PsxSpu::SetVoiceSample(s32 voice, const PsxSpuSample& sample)
{
    if (ValidVoice(voice))
    {
        mVoices[voice].mNextSample = sample;
    }
}

void PsxSpu::SetVoicePitch(s32 voice, u16 pitch)
{
    if (ValidVoice(voice))
    {
        mVoices[voice].mPitch = pitch;
    }
}

void PsxSpu::SetVoiceVolumeRegisters(s32 voice, u16 left, u16 right)
{
    if (ValidVoice(voice))
    {
        mVoices[voice].mVolumeLeft.SetRegister(left);
        mVoices[voice].mVolumeRight.SetRegister(right);
    }
}

void PsxSpu::SetVoiceVolume(s32 voice, s16 left, s16 right)
{
    if (ValidVoice(voice))
    {
        mVoices[voice].mVolumeLeft.SetFixed(left);
        mVoices[voice].mVolumeRight.SetFixed(right);
    }
}

void PsxSpu::SetVoiceAdsr(s32 voice, u16 adsr1, u16 adsr2)
{
    if (ValidVoice(voice))
    {
        mVoices[voice].mEnvelope.SetRegisters(adsr1, adsr2);
    }
}

void PsxSpu::SetVoiceReverb(s32 voice, bool enable)
{
    if (ValidVoice(voice))
    {
        mVoices[voice].mReverb = enable;
    }
}

void PsxSpu::KeyOn(s32 voice)
{
    if (!ValidVoice(voice))
    {
        return;
    }

    Voice& v = mVoices[voice];
    v.mSample = v.mNextSample;
    v.mEnvelope.KeyOn();
    v.mPlaying = true;
    v.mCounter = 0;
    v.mPosition = 0;
    v.mSamplesRead = 0;
    mEndx &= ~(1u << voice);

    // The interpolation starts with the first sample as the newest one and silence before it
    v.mHistory[0] = 0;
    v.mHistory[1] = 0;
    v.mHistory[2] = 0;
    v.mHistory[3] = ReadNextSample(voice, v);
}

void PsxSpu::KeyOff(s32 voice)
{
    if (ValidVoice(voice))
    {
        mVoices[voice].mEnvelope.KeyOff();
    }
}

void PsxSpu::StopVoice(s32 voice)
{
    if (ValidVoice(voice))
    {
        mVoices[voice].mEnvelope.Mute();
        mVoices[voice].mPlaying = false;
        mVoices[voice].mSample = {};
    }
}

void PsxSpu::KeyOnMask(u32 voices)
{
    for (s32 i = 0; i < kNumVoices; i++)
    {
        if (voices & (1u << i))
        {
            KeyOn(i);
        }
    }
}

void PsxSpu::KeyOffMask(u32 voices)
{
    for (s32 i = 0; i < kNumVoices; i++)
    {
        if (voices & (1u << i))
        {
            KeyOff(i);
        }
    }
}

s16 PsxSpu::VoiceEnvelope(s32 voice) const
{
    return ValidVoice(voice) ? mVoices[voice].mEnvelope.Level() : 0;
}

PsxSpuEnvelope::Phase PsxSpu::VoicePhase(s32 voice) const
{
    return ValidVoice(voice) ? mVoices[voice].mEnvelope.CurrentPhase() : PsxSpuEnvelope::Phase::Off;
}

bool PsxSpu::IsVoiceActive(s32 voice) const
{
    return VoicePhase(voice) != PsxSpuEnvelope::Phase::Off;
}

void PsxSpu::SetMasterVolumeRegisters(u16 left, u16 right)
{
    mMasterLeft.SetRegister(left);
    mMasterRight.SetRegister(right);
}

void PsxSpu::SetMasterVolume(s16 left, s16 right)
{
    mMasterLeft.SetFixed(left);
    mMasterRight.SetFixed(right);
}

const std::array<s16, 512>& PsxSpu::GaussTable()
{
    return kGaussTable;
}

s32 PsxSpu::Interpolate(const s16 samples[4], u32 i)
{
    i &= 0xFF;
    s32 out = (kGaussTable[0x0FF - i] * samples[0]) >> 15;
    out += (kGaussTable[0x1FF - i] * samples[1]) >> 15;
    out += (kGaussTable[0x100 + i] * samples[2]) >> 15;
    out += (kGaussTable[0x000 + i] * samples[3]) >> 15;
    return out;
}

u16 PsxSpu::PitchFromSampleRate(u32 sampleRate)
{
    const u64 pitch = (static_cast<u64>(sampleRate) * kPitch44100 + kSampleRate / 2) / kSampleRate;
    return static_cast<u16>(std::min<u64>(pitch, 0x3FFF));
}

// libsnd's pitch table: 4096 * 2^(i / 192) rounded down, a semitone (12 x 16 steps of 1/16 semitone)
// for each of the 12 notes of the octave above the root note. The same values as the one in the AE
// PS1 executable (SLES_014.80, 0x8009F090).
static const std::array<u16, 192> kNotePitchTable = {{
    4096, 4110, 4125, 4140, 4155, 4170, 4185, 4200, 4216, 4231, 4246, 4261, 4277, 4292, 4308, 4323,
    4339, 4355, 4371, 4386, 4402, 4418, 4434, 4450, 4466, 4482, 4499, 4515, 4531, 4548, 4564, 4581,
    4597, 4614, 4630, 4647, 4664, 4681, 4698, 4715, 4732, 4749, 4766, 4783, 4801, 4818, 4835, 4853,
    4870, 4888, 4906, 4924, 4941, 4959, 4977, 4995, 5013, 5031, 5050, 5068, 5086, 5105, 5123, 5142,
    5160, 5179, 5198, 5216, 5235, 5254, 5273, 5292, 5311, 5331, 5350, 5369, 5389, 5408, 5428, 5447,
    5467, 5487, 5507, 5527, 5547, 5567, 5587, 5607, 5627, 5648, 5668, 5688, 5709, 5730, 5750, 5771,
    5792, 5813, 5834, 5855, 5876, 5898, 5919, 5940, 5962, 5983, 6005, 6027, 6049, 6070, 6092, 6114,
    6137, 6159, 6181, 6203, 6226, 6248, 6271, 6294, 6316, 6339, 6362, 6385, 6408, 6431, 6455, 6478,
    6501, 6525, 6549, 6572, 6596, 6620, 6644, 6668, 6692, 6716, 6741, 6765, 6789, 6814, 6839, 6863,
    6888, 6913, 6938, 6963, 6988, 7014, 7039, 7064, 7090, 7116, 7141, 7167, 7193, 7219, 7245, 7271,
    7298, 7324, 7351, 7377, 7404, 7431, 7458, 7485, 7512, 7539, 7566, 7593, 7621, 7648, 7676, 7704,
    7732, 7760, 7788, 7816, 7844, 7873, 7901, 7930, 7958, 7987, 8016, 8045, 8074, 8103, 8133, 8162,
}};

u16 PsxSpu::PitchFromNote(s32 note, s32 fine, s32 centre, s32 shift)
{
    // libsnd's SsPitchFromNote, from the AE PS1 executable (0x80076620; the SEQ note on at
    // 0x80076518 does the same with the tone's centre/shift). The shift is added to the fine tune,
    // then both are rounded down to 1/16 semitones, carrying one semitone at most. Negative values
    // round towards zero, as the MIPS code does.
    const s32 fineShift = (fine + (shift & 0xFF)) / 8;
    const s32 carry = fineShift >= 16 ? 1 : 0;
    const s32 step = fineShift - carry * 16;
    const s32 semitones = note - (centre - 60) + carry;

    // The table is the octave above the root (semitones 60-71). A note more than 60 semitones below
    // the root makes libsnd read before its table (its division rounds towards zero), so round down
    // instead there, which gives the right pitch. A negative fine tune does the same, but callers
    // pass 0-127.
    const s32 octaveOfNote = semitones >= 0 ? semitones / 12 : -((-semitones + 11) / 12);
    const s32 idx = (semitones - octaveOfNote * 12) * 16 + step;
    const u32 pitch = kNotePitchTable[static_cast<size_t>(std::clamp(idx, 0, 191))];

    const s32 octave = octaveOfNote - 5;
    if (octave > 0)
    {
        return static_cast<u16>(pitch << octave);
    }
    return static_cast<u16>(pitch >> std::min(-octave, 31));
}

s16 PsxSpu::ReadNextSample(s32 voiceIdx, Voice& voice)
{
    const PsxSpuSample& sample = voice.mSample;
    const u32 length = sample.mPcm ? sample.mLength : 0;
    u32 end = length;
    bool loop = false;
    if (sample.mLoop)
    {
        end = sample.mLoopEnd == 0 ? length : std::min(sample.mLoopEnd, length);
        loop = sample.mLoopStart < end;
    }

    if (voice.mPosition >= end)
    {
        // The end of the sample: ENDX is set, and the voice either jumps to the loop start
        // ("End+Repeat") or is muted ("End+Mute": release, envelope level 0)
        mEndx |= 1u << voiceIdx;
        if (!loop)
        {
            voice.mPlaying = false;
            voice.mEnvelope.Mute();
            return 0;
        }
        voice.mPosition = sample.mLoopStart;
    }
    voice.mSamplesRead++;
    return sample.mPcm[voice.mPosition++];
}

s32 PsxSpu::SampleAt(const Voice& voice, s64 index)
{
    const PsxSpuSample& sample = voice.mSample;
    if (index < 0 || !sample.mPcm)
    {
        return 0;
    }

    const u32 length = sample.mLength;
    u32 end = length;
    bool loop = false;
    if (sample.mLoop)
    {
        end = sample.mLoopEnd == 0 ? length : std::min(sample.mLoopEnd, length);
        loop = sample.mLoopStart < end;
    }

    if (index < end)
    {
        return sample.mPcm[index];
    }
    if (!loop)
    {
        return 0;
    }
    const u32 loopLength = end - sample.mLoopStart;
    return sample.mPcm[sample.mLoopStart + static_cast<u32>((index - end) % loopLength)];
}

// The band-limited kernel: a sinc with its cutoff at kCutoff of the output Nyquist, windowed
// (Blackman) to kKernelHalfWidth output samples each side, in kKernelSteps steps per output sample.
// It's stretched by the pitch ratio, so the cutoff follows the output rate.
static constexpr s32 kKernelHalfWidth = 16;
static constexpr s32 kKernelSteps = 256;
static constexpr f64 kCutoff = 0.9;

static std::vector<s32> MakeBandLimitedKernel()
{
    const f64 pi = 3.14159265358979323846;
    std::vector<s32> kernel(kKernelHalfWidth * kKernelSteps + 1);
    for (s32 i = 0; i < static_cast<s32>(kernel.size()); i++)
    {
        const f64 u = static_cast<f64>(i) / kKernelSteps;
        const f64 x = pi * kCutoff * u;
        const f64 sinc = i == 0 ? 1.0 : std::sin(x) / x;
        const f64 w = u / kKernelHalfWidth;
        const f64 window = 0.42 + 0.5 * std::cos(pi * w) + 0.08 * std::cos(2.0 * pi * w);
        kernel[i] = static_cast<s32>(std::lround(kCutoff * sinc * window * 32768.0));
    }
    return kernel;
}

s32 PsxSpu::BandLimitedSample(const Voice& voice)
{
    static const std::vector<s32> kKernel = MakeBandLimitedKernel();

    // The same position the gaussian interpolates at: mHistory[1] plus the counter's fraction, in
    // 1/4096 samples
    const s64 position = (static_cast<s64>(voice.mSamplesRead) - 3) * 0x1000 + (voice.mCounter & 0xFFF);
    const s64 pitch = std::min(voice.mPitch, kMaxPitch);

    // Kernel steps per 1/4096 of a source sample, in 16.16: kKernelSteps / pitch
    const s64 stepScale = (static_cast<s64>(kKernelSteps) << 16) / pitch;
    const s64 halfWidth = (kKernelHalfWidth * pitch + 0xFFF) / 0x1000; // In source samples
    const s64 centre = position >> 12;

    s64 sum = 0;
    for (s64 j = centre - halfWidth; j <= centre + halfWidth + 1; j++)
    {
        const s64 distance = position - j * 0x1000;
        const s64 k = ((distance < 0 ? -distance : distance) * stepScale) >> 16;
        if (k >= static_cast<s64>(kKernel.size()))
        {
            continue;
        }
        sum += SampleAt(voice, j) * kKernel[static_cast<size_t>(k)];
    }

    // The kernel's gain is the pitch ratio (it's pitch / 1000h source samples per output sample)
    const s64 out = ((sum >> 15) * 0x1000) / pitch;
    return static_cast<s32>(std::clamp<s64>(out, -0x8000, 0x7FFF));
}

void PsxSpu::TickVoice(s32 voiceIdx, Voice& voice, s32& left, s32& right, s32& reverbLeft, s32& reverbRight)
{
    // Output: interpolate, then apply the envelope and the voice volume
    const bool bandLimited = mInterpolation == Interpolation::BandLimited && voice.mPitch > kPitch44100;
    const s32 sample = bandLimited ? BandLimitedSample(voice) : Interpolate(voice.mHistory, (voice.mCounter >> 4) & 0xFF);
    const s32 enveloped = (sample * voice.mEnvelope.Level()) >> 15;
    const s32 outLeft = (enveloped * voice.mVolumeLeft.Current()) >> 15;
    const s32 outRight = (enveloped * voice.mVolumeRight.Current()) >> 15;
    left += outLeft;
    right += outRight;
    if (voice.mReverb)
    {
        reverbLeft += outLeft;
        reverbRight += outRight;
    }

    voice.mEnvelope.Tick();
    voice.mVolumeLeft.Tick();
    voice.mVolumeRight.Tick();

    // Pitch counter: bit 12 and up are whole samples
    voice.mCounter += std::min(voice.mPitch, kMaxPitch);
    while (voice.mCounter >= 0x1000 && voice.mPlaying)
    {
        voice.mCounter -= 0x1000;
        voice.mHistory[0] = voice.mHistory[1];
        voice.mHistory[1] = voice.mHistory[2];
        voice.mHistory[2] = voice.mHistory[3];
        voice.mHistory[3] = ReadNextSample(voiceIdx, voice);
    }
}

void PsxSpu::Render(s16* out, u32 frames)
{
    for (u32 frame = 0; frame < frames; frame++)
    {
        s32 left = 0;
        s32 right = 0;
        s32 reverbLeft = 0;
        s32 reverbRight = 0;

        for (s32 i = 0; i < kNumVoices; i++)
        {
            Voice& voice = mVoices[i];
            if (voice.mEnvelope.CurrentPhase() != PsxSpuEnvelope::Phase::Off)
            {
                TickVoice(i, voice, left, right, reverbLeft, reverbRight);
            }
        }

        s32 wetLeft = 0;
        s32 wetRight = 0;
        mReverb.Process(reverbLeft, reverbRight, wetLeft, wetRight);

        const s32 mixLeft = Clamp16(left + wetLeft);
        const s32 mixRight = Clamp16(right + wetRight);
        out[frame * 2] = Clamp16((mixLeft * mMasterLeft.Current()) >> 15);
        out[frame * 2 + 1] = Clamp16((mixRight * mMasterRight.Current()) >> 15);

        mMasterLeft.Tick();
        mMasterRight.Tick();
    }
}
