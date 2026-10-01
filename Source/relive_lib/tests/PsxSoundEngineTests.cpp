#include <gtest/gtest.h>
#include <cmath>
#include "Sound/PsxSoundEngine.hpp"
#include "Sound/PsxSpuApi.hpp"
#include "Sound/Libsnd.hpp"
#include <cstring>

// A VH whose first program has one tone: the secret area jingle's (RFSNDFX.VH program 81), as the PC
// data has it
static std::vector<u8> JingleVh(s8 priority, s8 vol)
{
    std::vector<u8> vh(sizeof(VabHeader) + 16 * sizeof(VagAtr));
    VabHeader header = {};
    header.field_12_num_progs = 1;
    memcpy(vh.data(), &header, sizeof(header));

    VagAtr tone = {};
    tone.field_0_priority = priority;
    tone.field_2_vol = vol;
    tone.field_6_min = 0;
    tone.field_7_max = 127;
    tone.field_14_prog = 81;
    tone.field_16_vag = 44;
    memcpy(vh.data() + sizeof(VabHeader), &tone, sizeof(tone));
    return vh;
}

static VagAtr FirstTone(const std::vector<u8>& vh)
{
    VagAtr tone = {};
    memcpy(&tone, vh.data() + sizeof(VabHeader), sizeof(tone));
    return tone;
}

TEST(PsxSoundEngine, RestorePs1Tones)
{
    // The PC data's 70/70 becomes the PS1's 127/127
    std::vector<u8> vh = JingleVh(70, 70);
    PsxSoundEngine::RestorePs1Tones(GameType::eAo, "rfsndfx.vh", vh);
    EXPECT_EQ(FirstTone(vh).field_0_priority, 127);
    EXPECT_EQ(FirstTone(vh).field_2_vol, 127);

    // Only that VH of that game, and only a tone as the PC data has it
    vh = JingleVh(70, 70);
    PsxSoundEngine::RestorePs1Tones(GameType::eAe, "RFSNDFX.VH", vh);
    EXPECT_EQ(FirstTone(vh).field_0_priority, 70);
    PsxSoundEngine::RestorePs1Tones(GameType::eAo, "E1SNDFX.VH", vh);
    EXPECT_EQ(FirstTone(vh).field_0_priority, 70);
    vh = JingleVh(70, 60);
    PsxSoundEngine::RestorePs1Tones(GameType::eAo, "RFSNDFX.VH", vh);
    EXPECT_EQ(FirstTone(vh).field_0_priority, 70);
    EXPECT_EQ(FirstTone(vh).field_2_vol, 60);

    // Too short to hold its tones: left alone
    vh = JingleVh(70, 70);
    vh.resize(sizeof(VabHeader) + 8);
    PsxSoundEngine::RestorePs1Tones(GameType::eAo, "RFSNDFX.VH", vh);
}

// The register side of an SPU, for testing Libsnd
class FakeSpu final : public ILibsndSpu
{
public:
    struct Voice final
    {
        u16 mVolLeft = 0;
        u16 mVolRight = 0;
        u32 mPitch = 0;
        s32 mVabId = -1;
        s32 mVag = 0;
        u16 mAdsr1 = 0;
        u16 mAdsr2 = 0;
    };

    u16 VoiceEnvelope(s32 voice) override
    {
        return mEnvelope[voice];
    }
    void SetVoiceVolume(s32 voice, u16 left, u16 right) override
    {
        mVoices[voice].mVolLeft = left;
        mVoices[voice].mVolRight = right;
    }
    void SetVoicePitch(s32 voice, u32 pitch) override
    {
        mVoices[voice].mPitch = pitch;
    }
    void SetVoiceSample(s32 voice, s32 vabId, s32 vag) override
    {
        mVoices[voice].mVabId = vabId;
        mVoices[voice].mVag = vag;
    }
    void SetVoiceAdsr(s32 voice, u16 adsr1, u16 adsr2) override
    {
        mVoices[voice].mAdsr1 = adsr1;
        mVoices[voice].mAdsr2 = adsr2;
    }
    void KeyOff(u32 voices) override
    {
        mKeyOffs |= voices;
    }
    void KeyOn(u32 voices) override
    {
        mKeyOns |= voices;
        // A keyed on voice sounds at full volume until the test says otherwise
        for (s32 i = 0; i < Libsnd::kNumVoices; i++)
        {
            if (voices & (1u << i))
            {
                mEnvelope[i] = 0x7FFF;
            }
        }
    }
    void SetReverbVoices(u32 voices) override
    {
        mReverb = voices;
    }

    std::array<Voice, Libsnd::kNumVoices> mVoices = {};
    std::array<u16, Libsnd::kNumVoices> mEnvelope = {};
    u32 mKeyOns = 0;
    u32 mKeyOffs = 0;
    u32 mReverb = 0;
};

struct ToneParams final
{
    u8 mVabVol = 127;
    u8 mProgVol = 127;
    u8 mProgPan = 64;
    u8 mProgPriority = 0;
    u8 mToneVol = 127;
    u8 mTonePan = 64;
    u8 mPriority = 64;
    u8 mCentre = 60;
    u8 mShift = 0;
    u8 mMode = 0;
};

// A VH (format version 7: 128 programs) whose program 0 has one tone, on VAG 1
static std::vector<u8> OneToneVh(const ToneParams& p)
{
    std::vector<u8> vh(0x20 + 128 * 0x10 + 16 * 0x20);
    const u32 form = 'V' << 24 | 'A' << 16 | 'B' << 8 | 'p';
    memcpy(&vh[0], &form, 4);
    vh[4] = 7;
    vh[0x12] = 1; // Programs
    vh[0x18] = p.mVabVol;
    vh[0x19] = 64;
    u8* prog = &vh[0x20];
    prog[0] = 1; // Tones
    prog[1] = p.mProgVol;
    prog[2] = p.mProgPriority;
    prog[4] = p.mProgPan;
    u8* tone = &vh[0x20 + 128 * 0x10];
    tone[0] = p.mPriority;
    tone[1] = p.mMode;
    tone[2] = p.mToneVol;
    tone[3] = p.mTonePan;
    tone[4] = p.mCentre;
    tone[5] = p.mShift;
    tone[6] = 0;
    tone[7] = 127;
    tone[0x10] = 0xFF; // ADSR 0x80FF, 0x5FC8
    tone[0x11] = 0x80;
    tone[0x12] = 0xC8;
    tone[0x13] = 0x5F;
    tone[0x16] = 1; // VAG 1
    return vh;
}

// A SEQ at 480 ticks a quarter note and 120 beats a minute (960 ticks a second, 19.2 a libsnd
// tick), of events (each followed by its delta time, in the MIDI variable length format)
static std::vector<u8> Seq(const std::vector<u8>& events)
{
    std::vector<u8> seq = {'p', 'Q', 'E', 'S', 0, 0, 0, 1, 0x01, 0xE0, 0x07, 0xA1, 0x20, 4, 2, 0};
    seq.insert(seq.end(), events.begin(), events.end());
    seq.push_back(0xFF);
    seq.push_back(0x2F);
    seq.push_back(0);
    return seq;
}

static Libsnd::Config AoConfig()
{
    return Libsnd::Config{};
}

static Libsnd::Config AeConfig()
{
    Libsnd::Config config;
    config.mSquareSfxVolume = false;
    config.mSsSeqSetVol = Libsnd::Config::SsSeqSetVolMode::eWhilePlaying;
    config.mSetSeqVolAlwaysUpdates = true;
    config.mSeqVolZeroIsOne = false;
    config.mSeqVolNoteChannel = true;
    config.mNoteOnSkippedAtSeqVolZero = false;
    return config;
}

// A sound effect's left/right volume that libsnd turns into this velocity and pan, if there's one
static bool SfxVolumes(s32 velocity, s32 pan, u16& left, u16& right)
{
    for (s32 other = 0; other <= velocity; other++)
    {
        if (pan <= 64 && other * 64 / velocity == pan && (pan < 64 || other == velocity))
        {
            left = static_cast<u16>(velocity);
            right = static_cast<u16>(other);
            return true;
        }
        if (pan > 64 && other < velocity && 127 - other * 64 / velocity == pan)
        {
            left = static_cast<u16>(other);
            right = static_cast<u16>(velocity);
            return true;
        }
    }
    return false;
}

// libsnd's voice volume for a sound effect, as SsVoKeyOn sets it up
static FakeSpu::Voice SfxVoice(const Libsnd::Config& config, const ToneParams& params, u16 left, u16 right, s32 note = 60)
{
    const std::vector<u8> vh = OneToneVh(params);
    FakeSpu spu;
    Libsnd snd(spu, config);
    EXPECT_EQ(snd.VabOpenHead(vh.data(), static_cast<u32>(vh.size()), 0), 0);
    EXPECT_EQ(snd.VoKeyOn(0, note << 8, left, right), 1);
    snd.Tick();
    EXPECT_EQ(spu.mKeyOns, 1u);
    return spu.mVoices[0];
}

// The expected volumes come from running the games' PS1 executables' libsnd in a MIPS interpreter:
// the voice setup at 0x80076d94 in AE's (SLES_014.80) and 0x8007c73c in AO's (SLES_006.64)
TEST(Libsnd, SfxVolume)
{
    // AE doesn't square a sound effect's volume, AO does
    EXPECT_EQ(SfxVoice(AeConfig(), {}, 127, 127).mVolLeft, 0x3FFF);
    EXPECT_EQ(SfxVoice(AoConfig(), {}, 127, 127).mVolLeft, 0x3FFF);
    EXPECT_EQ(SfxVoice(AeConfig(), {}, 66, 66).mVolLeft, 8514);
    EXPECT_EQ(SfxVoice(AoConfig(), {}, 66, 66).mVolLeft, 4424);

    struct Case final
    {
        bool mAo;
        s32 mVelocity, mVabVol, mProgVol, mToneVol, mTonePan, mProgPan, mChannelPan;
        s32 mLeft, mRight;
    };
    const Case cases[] = {
        {false, 103, 106, 44, 93, 95, 22, 112, 340, 982},
        {false, 100, 94, 125, 7, 120, 11, 78, 44, 90},
        {false, 45, 46, 23, 65, 8, 18, 21, 194, 2},
        {false, 7, 101, 82, 109, 15, 76, 32, 321, 47},
        {false, 87, 29, 96, 19, 57, 20, 68, 270, 82},
        {false, 78, 50, 63, 92, 20, 71, 22, 1264, 157},
        {false, 86, 58, 99, 78, 10, 83, 47, 1693, 286},
        {false, 44, 22, 59, 124, 1, 45, 81, 324, 5},
        {false, 94, 40, 52, 79, 76, 76, 95, 323, 972},
        {true, 44, 71, 115, 93, 18, 24, 93, 78, 3},
        {true, 46, 88, 65, 111, 15, 31, 57, 206, 2},
        {true, 87, 72, 68, 112, 30, 78, 46, 333, 66},
        {true, 71, 83, 91, 50, 80, 119, 116, 0, 173},
        {true, 83, 79, 122, 93, 73, 18, 30, 983, 24},
        {true, 74, 120, 68, 47, 89, 127, 116, 0, 194},
        {true, 118, 74, 123, 47, 88, 5, 118, 4, 3},
        {true, 97, 111, 95, 57, 101, 100, 127, 0, 822},
    };
    s32 checked = 0;
    for (const Case& c : cases)
    {
        u16 left = 0;
        u16 right = 0;
        if (!SfxVolumes(c.mVelocity, c.mChannelPan, left, right))
        {
            continue;
        }
        ToneParams params;
        params.mVabVol = static_cast<u8>(c.mVabVol);
        params.mProgVol = static_cast<u8>(c.mProgVol);
        params.mProgPan = static_cast<u8>(c.mProgPan);
        params.mToneVol = static_cast<u8>(c.mToneVol);
        params.mTonePan = static_cast<u8>(c.mTonePan);
        const FakeSpu::Voice v = SfxVoice(c.mAo ? AoConfig() : AeConfig(), params, left, right);
        EXPECT_EQ(v.mVolLeft, c.mLeft) << "velocity " << c.mVelocity;
        EXPECT_EQ(v.mVolRight, c.mRight) << "velocity " << c.mVelocity;
        checked++;
    }
    EXPECT_GE(checked, 10);
}

TEST(Libsnd, SeqNoteVolume)
{
    // SEQ volume 12/18, channel pan 56, velocity 126: the MIPS interpreter's 6/32 (AE)
    ToneParams params;
    params.mVabVol = 122;
    params.mProgVol = 57;
    params.mToneVol = 105;
    params.mTonePan = 86;
    params.mProgPan = 70;
    const std::vector<u8> vh = OneToneVh(params);
    const std::vector<u8> seq = Seq({0xB0, 0x0A, 56, 0, 0x90, 60, 126, 0x60});
    FakeSpu spu;
    Libsnd snd(spu, AeConfig());
    snd.VabOpenHead(vh.data(), static_cast<u32>(vh.size()), 0);
    const s16 s = snd.SeqOpen(seq.data(), static_cast<u32>(seq.size()), 0);
    ASSERT_EQ(s, 0);
    snd.SeqSetVol(s, 12, 18);
    snd.SeqPlay(s, 1, 1);
    snd.Tick();
    snd.Tick();
    EXPECT_EQ(spu.mKeyOns, 1u);
    EXPECT_EQ(spu.mVoices[0].mVolLeft, 6);
    EXPECT_EQ(spu.mVoices[0].mVolRight, 32);
}

TEST(Libsnd, Pitch)
{
    // 1000h is the sample's rate, at the tone's centre note. It isn't cut to 16 bits.
    EXPECT_EQ(SfxVoice(AoConfig(), {}, 100, 100, 60).mPitch, 0x1000u);
    EXPECT_EQ(SfxVoice(AoConfig(), {}, 100, 100, 72).mPitch, 0x2000u);
    EXPECT_EQ(SfxVoice(AoConfig(), {}, 100, 100, 48).mPitch, 0x800u);
    EXPECT_EQ(SfxVoice(AoConfig(), {}, 100, 100, 108).mPitch, 0x10000u);
    // The shift raises it, in 1/16 semitones (libsnd's table): 64 / 8 = 8 steps
    ToneParams params;
    params.mShift = 64;
    EXPECT_EQ(SfxVoice(AoConfig(), params, 100, 100, 60).mPitch, 0x1078u);
    // More than 5 octaves below the centre still gets the right pitch (libsnd reads before its table)
    params = {};
    params.mCentre = 127;
    EXPECT_EQ(SfxVoice(AoConfig(), params, 100, 100, 127 - 72).mPitch, 0x1000u >> 6);
}

// libsnd's voice allocation (SpuVmAlloc, 0x8007bd30 in the AO PS1 executable)
TEST(Libsnd, Allocation)
{
    ToneParams low;
    low.mPriority = 20;
    ToneParams mid;
    mid.mPriority = 50;
    ToneParams high;
    high.mPriority = 60;
    std::vector<u8> vhLow = OneToneVh(low);
    std::vector<u8> vhMid = OneToneVh(mid);
    std::vector<u8> vhHigh = OneToneVh(high);
    ToneParams lowest;
    lowest.mPriority = 10;
    std::vector<u8> vhLowest = OneToneVh(lowest);

    FakeSpu spu;
    Libsnd snd(spu, AoConfig());
    snd.VabOpenHead(vhLow.data(), static_cast<u32>(vhLow.size()), 0);
    snd.VabOpenHead(vhMid.data(), static_cast<u32>(vhMid.size()), 1);
    snd.VabOpenHead(vhHigh.data(), static_cast<u32>(vhHigh.size()), 2);
    snd.VabOpenHead(vhLowest.data(), static_cast<u32>(vhLowest.size()), 3);

    // Free voices go in order. Voices 5 and 9 have the lower priority.
    for (s32 i = 0; i < Libsnd::kNumVoices; i++)
    {
        ASSERT_EQ(snd.VoKeyOn((i == 5 || i == 9 ? 0 : 1) << 8, 60 << 8, 100, 100), 1 << i);
    }
    snd.Tick();

    // Every voice has a higher priority: the note is dropped
    EXPECT_EQ(snd.VoKeyOn(3 << 8, 60 << 8, 100, 100), -1);

    // The lowest priority. Of the two, both at the same envelope, the oldest.
    EXPECT_EQ(snd.VoKeyOn(2 << 8, 60 << 8, 100, 100), 1 << 5);
    EXPECT_EQ(snd.VoKeyOn(2 << 8, 60 << 8, 100, 100), 1 << 9);

    // Of the same priority, the quietest
    spu.mEnvelope[17] = 0x100;
    snd.Tick();
    EXPECT_EQ(snd.VoKeyOn(1 << 8, 60 << 8, 100, 100), 1 << 17);

    // A released voice isn't free until it's silent, and has been for 15 ticks (16 at most, see
    // SilentOneShotFreedAfter15Ticks)
    snd.UtKeyOffV(12);
    spu.mEnvelope[12] = 0;
    for (s32 i = 0; i < 16; i++)
    {
        snd.Tick();
    }
    EXPECT_EQ(snd.VoKeyOn(3 << 8, 60 << 8, 100, 100), 1 << 12);
}

TEST(Libsnd, SilentOneShotFreedAfter15Ticks)
{
    const std::vector<u8> vh = OneToneVh({});
    FakeSpu spu;
    Libsnd snd(spu, AoConfig());
    snd.VabOpenHead(vh.data(), static_cast<u32>(vh.size()), 0);
    for (s32 i = 0; i < Libsnd::kNumVoices; i++)
    {
        snd.VoKeyOn(0, 60 << 8, 100, 100);
    }
    snd.Tick();
    EXPECT_EQ(snd.GetVoiceInfo(3).mKeyed, true);

    // A one shot sample ended without a key off: the envelope reads 0, but the voice is still keyed
    spu.mEnvelope[3] = 0;
    s32 ticks = 0;
    while (snd.GetVoiceInfo(3).mKeyed && ticks < 40)
    {
        snd.Tick();
        ticks++;
    }
    // The flush ANDs history slots 0-14 of 16, so it's freed 15 or 16 ticks after it went silent
    EXPECT_GE(ticks, 15);
    EXPECT_LE(ticks, 16);
}

TEST(Libsnd, SeqTimingAndEnd)
{
    const std::vector<u8> vh = OneToneVh({});
    // A note on, half a second (480 ticks), its note off (velocity 0), then the end
    const std::vector<u8> seq = Seq({0x90, 60, 100, 0x83, 0x60, 60, 0, 0});
    FakeSpu spu;
    Libsnd snd(spu, AoConfig());
    snd.VabOpenHead(vh.data(), static_cast<u32>(vh.size()), 0);
    const s16 s = snd.SeqOpen(seq.data(), static_cast<u32>(seq.size()), 0);
    ASSERT_EQ(s, 0);
    EXPECT_EQ(snd.IsEos(s, 0), 0);
    snd.SeqPlay(s, 1, 1);
    EXPECT_EQ(snd.IsEos(s, 0), 1);

    // The note on is read on the first tick, and keyed on by the second's flush
    snd.Tick();
    snd.Tick();
    EXPECT_EQ(spu.mKeyOns, 1u);

    // The note off half a second (25 ticks) later
    s32 ticks = 1;
    while (!(spu.mKeyOffs & 1) && ticks < 100)
    {
        snd.Tick();
        ticks++;
    }
    EXPECT_GE(ticks, 25);
    EXPECT_LE(ticks, 27);

    // Then it ends (played once)
    for (s32 i = 0; i < 5; i++)
    {
        snd.Tick();
    }
    EXPECT_EQ(snd.IsEos(s, 0), 0);
}

TEST(Libsnd, SeqLoopsForever)
{
    const std::vector<u8> vh = OneToneVh({});
    const std::vector<u8> seq = Seq({0x90, 60, 100, 0x83, 0x60, 60, 0, 0});
    FakeSpu spu;
    Libsnd snd(spu, AoConfig());
    snd.VabOpenHead(vh.data(), static_cast<u32>(vh.size()), 0);
    const s16 s = snd.SeqOpen(seq.data(), static_cast<u32>(seq.size()), 0);
    snd.SeqPlay(s, 1, 0); // 0 = forever
    s32 keyOns = 0;
    for (s32 i = 0; i < 200; i++)
    {
        spu.mKeyOns = 0;
        snd.Tick();
        keyOns += spu.mKeyOns ? 1 : 0;
    }
    EXPECT_GE(keyOns, 7);
    EXPECT_EQ(snd.IsEos(s, 0), 1);
    snd.SeqStop(s);
    snd.Tick();
    EXPECT_EQ(snd.IsEos(s, 0), 0);
}

// SsSeqSetVol changes a playing SEQ's notes too, as the games' libsnd do (SpuVmSetSeqVol at
// 0x8007cf10 in AO's PS1 executable, 0x800776c4 in AE's). AO's takes a volume of 0 as 1.
TEST(Libsnd, SeqSetVolOnPlayingNotes)
{
    const std::vector<u8> vh = OneToneVh({});
    const std::vector<u8> seq = Seq({0x90, 60, 127, 0x8F, 0x00});
    for (const bool ao : {true, false})
    {
        FakeSpu spu;
        Libsnd snd(spu, ao ? AoConfig() : AeConfig());
        snd.VabOpenHead(vh.data(), static_cast<u32>(vh.size()), 0);
        const s16 s = snd.SeqOpen(seq.data(), static_cast<u32>(seq.size()), 0);
        snd.SeqPlay(s, 1, 1);
        snd.Tick();
        snd.Tick();
        ASSERT_EQ(spu.mVoices[0].mVolLeft, 0x3FFF);

        snd.SeqSetVol(s, 64, 64);
        snd.Tick();
        // (0x3FFF * 64 / 127)^2 / 0x3FFF
        EXPECT_EQ(spu.mVoices[0].mVolLeft, 4160);

        snd.SeqSetVol(s, 0, 0);
        snd.Tick();
        EXPECT_EQ(spu.mVoices[0].mVolLeft, ao ? 1 : 0);
    }

    // AE's only does it while the SEQ plays: before, it's for the notes to come
    FakeSpu spu;
    Libsnd snd(spu, AeConfig());
    snd.VabOpenHead(vh.data(), static_cast<u32>(vh.size()), 0);
    const s16 s = snd.SeqOpen(seq.data(), static_cast<u32>(seq.size()), 0);
    snd.SeqSetVol(s, 64, 64);
    snd.SeqPlay(s, 1, 1);
    snd.Tick();
    snd.Tick();
    EXPECT_EQ(spu.mVoices[0].mVolLeft, 4160);
}

// A SEQ note off releases every voice that SEQ keyed on with the note, VAB and program: each strike
// of a repeated note gets its own voice
TEST(Libsnd, NoteOffReleasesEveryStrike)
{
    const std::vector<u8> vh = OneToneVh({});
    const std::vector<u8> seq = Seq({0x90, 60, 100, 0x10, 60, 100, 0x10, 60, 0, 0x10});
    FakeSpu spu;
    Libsnd snd(spu, AoConfig());
    snd.VabOpenHead(vh.data(), static_cast<u32>(vh.size()), 0);
    const s16 s = snd.SeqOpen(seq.data(), static_cast<u32>(seq.size()), 0);
    snd.SeqPlay(s, 1, 1);
    for (s32 i = 0; i < 20 && spu.mKeyOffs == 0; i++)
    {
        snd.Tick();
    }
    EXPECT_EQ(spu.mKeyOns, 3u);
    EXPECT_EQ(spu.mKeyOffs, 3u);
}

// A sound effect's random pitch variation: the game keys the note on, then SsUtChangePitch moves
// each voice to the note plus the offset (SND_SetChannelsPitch)
TEST(Libsnd, SfxChangePitch)
{
    const std::vector<u8> vh = OneToneVh({});
    FakeSpu spu;
    Libsnd snd(spu, AoConfig());
    snd.VabOpenHead(vh.data(), static_cast<u32>(vh.size()), 0);
    const s32 voices = snd.VoKeyOn(0, 53 << 8, 100, 100);
    ASSERT_EQ(voices, 1);
    snd.Tick();
    const u32 before = spu.mVoices[0].mPitch;
    ASSERT_GT(before, 0u);

    // +400/128 semitones: note 56, fine 16
    EXPECT_EQ(snd.UtChangePitch(0, 0, 0, 53, 0, 56, 16), 0);
    snd.Tick();
    const f64 ratio = static_cast<f64>(spu.mVoices[0].mPitch) / before;
    EXPECT_NEAR(ratio, std::pow(2.0, 3.125 / 12.0), 0.01) << "pitch " << before << " -> " << spu.mVoices[0].mPitch;
}
