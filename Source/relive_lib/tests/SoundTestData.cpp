#include "SoundTestData.hpp"
#include "Sound/PsxSpuApi.hpp"
#include <cmath>
#include <cstring>

namespace SoundTestData {

static void Put(std::vector<u8>& out, const void* pData, size_t len)
{
    const u8* p = static_cast<const u8*>(pData);
    out.insert(out.end(), p, p + len);
}

template <typename T>
static void PutLE(std::vector<u8>& out, T value)
{
    for (size_t i = 0; i < sizeof(T); i++)
    {
        out.push_back(static_cast<u8>(static_cast<u64>(value) >> (8 * i)));
    }
}

std::vector<u8> BuildVh(const Vab& vab)
{
    VabHeader header = {};
    header.field_0_form = 0x56414270; // "pBAV"
    header.field_4_version = 7;
    header.field_8_id = vab.mId;
    header.field_12_num_progs = static_cast<s16>(vab.mPrograms.size());
    s32 numTones = 0;
    for (const Program& prog : vab.mPrograms)
    {
        numTones += static_cast<s32>(prog.mTones.size());
        ProgAtr& atr = header.field_20_progs[prog.mNumber];
        atr.field_0_num_tones = static_cast<u8>(prog.mTones.size());
        atr.field_1_vol = 127;
        atr.field_4_pan = 64;
    }
    header.field_14_num_tones = static_cast<s16>(numTones);
    header.field_16_num_vags = static_cast<u16>(vab.mSamples.size());
    header.field_18_master_vol = 127;
    header.field_19_master_pan = 64;

    std::vector<u8> out;
    Put(out, &header, sizeof(header));

    // 16 VagAtr (0x20 bytes each) per program, unused ones zeroed
    for (const Program& prog : vab.mPrograms)
    {
        for (size_t t = 0; t < 16; t++)
        {
            Tone tone = {};
            tone.mVol = 0;
            tone.mVag = 0;
            if (t < prog.mTones.size())
            {
                tone = prog.mTones[t];
            }

            PutLE<s8>(out, tone.mPriority);
            PutLE<s8>(out, tone.mMode);
            PutLE<s8>(out, tone.mVol);
            PutLE<s8>(out, tone.mPan);
            PutLE<u8>(out, tone.mCentre);
            PutLE<u8>(out, tone.mShift);
            PutLE<s8>(out, tone.mMin);
            PutLE<s8>(out, tone.mMax);
            PutLE<u32>(out, 0);                                        // vibW, vibT, porW, porT
            PutLE<s8>(out, 2);                                         // pitch bend min
            PutLE<s8>(out, 2);                                         // pitch bend max
            PutLE<u16>(out, 0);                                        // reserved
            PutLE<s16>(out, tone.mAdsr1);
            PutLE<s16>(out, tone.mAdsr2);
            PutLE<s16>(out, static_cast<s16>(prog.mNumber));
            PutLE<s16>(out, tone.mVag);
            PutLE<u64>(out, 0);                                        // reserved[4]
        }
    }

    // VAG size table (size / 8), the engine doesn't read it
    for (size_t i = 0; i < 256; i++)
    {
        const size_t size = (i >= 1 && i <= vab.mSamples.size()) ? vab.mSamples[i - 1].mPcm.size() * 2 : 0;
        PutLE<u16>(out, static_cast<u16>(size / 8));
    }

    // file size
    const s32 fileSize = static_cast<s32>(out.size());
    memcpy(out.data() + offsetof(VabHeader, field_C_file_size), &fileSize, sizeof(fileSize));
    return out;
}

std::vector<u8> BuildVbAe(const Vab& vab, std::vector<u8>& soundsDat)
{
    std::vector<u8> out;
    for (const Sample& sample : vab.mSamples)
    {
        PutLE<s32>(out, static_cast<s32>(sample.mPcm.size() * 2));
        PutLE<s32>(out, sample.mLoop ? -1 : 0);
        PutLE<u32>(out, static_cast<u32>(soundsDat.size()));
        for (s16 s : sample.mPcm)
        {
            PutLE<s16>(soundsDat, s);
        }
    }
    return out;
}

std::vector<u8> BuildVbAo(const Vab& vab)
{
    std::vector<u8> out;
    for (const Sample& sample : vab.mSamples)
    {
        PutLE<s32>(out, static_cast<s32>(sample.mPcm.size() * 2));
        PutLE<s32>(out, sample.mLoop ? -1 : 0);
        for (s16 s : sample.mPcm)
        {
            PutLE<s16>(out, s);
        }
    }
    return out;
}

std::vector<s16> Sine(s32 length, f64 period, s16 amplitude)
{
    std::vector<s16> ret(length);
    for (s32 i = 0; i < length; i++)
    {
        ret[i] = static_cast<s16>(std::lround(amplitude * std::sin(2.0 * 3.14159265358979323846 * i / period)));
    }
    return ret;
}

std::vector<s16> Saw(s32 length, s32 period, s16 amplitude)
{
    std::vector<s16> ret(length);
    for (s32 i = 0; i < length; i++)
    {
        ret[i] = static_cast<s16>(((i % period) * 2 * amplitude) / period - amplitude);
    }
    return ret;
}

std::vector<s16> Noise(s32 length, u32 seed, s16 amplitude)
{
    std::vector<s16> ret(length);
    u32 state = seed;
    for (s32 i = 0; i < length; i++)
    {
        state = state * 1664525u + 1013904223u;
        ret[i] = static_cast<s16>(static_cast<s32>(state >> 16) % (amplitude + 1) * ((state & 1) ? 1 : -1));
    }
    return ret;
}

SeqBuilder::SeqBuilder(u16 resolution, u32 tempoUsPerQuarter)
{
    Put(mData, "pQES", 4);
    const u8 header[] = {
        0, 0, 0, 1, // version, big endian
        static_cast<u8>(resolution >> 8), static_cast<u8>(resolution),
        static_cast<u8>(tempoUsPerQuarter >> 16), static_cast<u8>(tempoUsPerQuarter >> 8), static_cast<u8>(tempoUsPerQuarter),
        4, 2 // time signature
    };
    Put(mData, header, sizeof(header));
}

void SeqBuilder::PutVarLen(u32 value)
{
    u8 bytes[4] = {};
    s32 count = 0;
    bytes[count++] = value & 0x7F;
    while ((value >>= 7) != 0)
    {
        bytes[count++] = static_cast<u8>((value & 0x7F) | 0x80);
    }
    while (count > 0)
    {
        mData.push_back(bytes[--count]);
    }
}

SeqBuilder& SeqBuilder::Wait(u32 ticks)
{
    mDelta += ticks;
    return *this;
}

SeqBuilder& SeqBuilder::NoteOn(u8 channel, u8 note, u8 velocity)
{
    PutVarLen(mDelta);
    mDelta = 0;
    const u8 event[] = {static_cast<u8>(0x90 | channel), note, velocity};
    Put(mData, event, sizeof(event));
    return *this;
}

SeqBuilder& SeqBuilder::NoteOff(u8 channel, u8 note)
{
    PutVarLen(mDelta);
    mDelta = 0;
    const u8 event[] = {static_cast<u8>(0x80 | channel), note, 0};
    Put(mData, event, sizeof(event));
    return *this;
}

SeqBuilder& SeqBuilder::RunningNoteOn(u8 note, u8 velocity)
{
    PutVarLen(mDelta);
    mDelta = 0;
    const u8 event[] = {note, velocity};
    Put(mData, event, sizeof(event));
    return *this;
}

SeqBuilder& SeqBuilder::ProgramChange(u8 channel, u8 program)
{
    PutVarLen(mDelta);
    mDelta = 0;
    const u8 event[] = {static_cast<u8>(0xC0 | channel), program};
    Put(mData, event, sizeof(event));
    return *this;
}

SeqBuilder& SeqBuilder::Controller(u8 channel, u8 controller, u8 value)
{
    PutVarLen(mDelta);
    mDelta = 0;
    const u8 event[] = {static_cast<u8>(0xB0 | channel), controller, value};
    Put(mData, event, sizeof(event));
    return *this;
}

SeqBuilder& SeqBuilder::PitchBend(u8 channel, s32 bend)
{
    PutVarLen(mDelta);
    mDelta = 0;
    const u32 value = static_cast<u32>(bend + 8192);
    const u8 event[] = {static_cast<u8>(0xE0 | channel), static_cast<u8>(value & 0x7F), static_cast<u8>((value >> 7) & 0x7F)};
    Put(mData, event, sizeof(event));
    return *this;
}

SeqBuilder& SeqBuilder::LoopStart(u8 channel, u8 count)
{
    Controller(channel, 99, 20);
    return Controller(channel, 6, count);
}

SeqBuilder& SeqBuilder::LoopEnd(u8 channel)
{
    Controller(channel, 99, 30);
    return Controller(channel, 6, 0);
}

std::vector<u8> SeqBuilder::End()
{
    PutVarLen(mDelta);
    mDelta = 0;
    const u8 event[] = {0xFF, 0x2F, 0x00};
    Put(mData, event, sizeof(event));
    return mData;
}

} // namespace SoundTestData
