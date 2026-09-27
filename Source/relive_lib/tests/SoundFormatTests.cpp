// VAB <-> SF2 and SEQ <-> SMF conversion (see Sound/SOUND_FORMATS.md)

#include <gtest/gtest.h>
#include "SoundTestData.hpp"
#include "Sound/SoundFont.hpp"
#include "Sound/VabSoundFont.hpp"
#include "Sound/SeqMidi.hpp"
#include "Sound/PsxSpuApi.hpp"
#include <cstring>

using namespace SoundTestData;

static std::vector<VabSoundFont::Sample> ToSamples(const Vab& vab)
{
    std::vector<VabSoundFont::Sample> ret;
    for (const Sample& s : vab.mSamples)
    {
        ret.push_back({s.mPcm, s.mLoop});
    }
    return ret;
}

// VAB -> SF2 file bytes -> VAB
static VabSoundFont::Vab RoundTrip(const VabSoundFont::Vab& vab)
{
    const std::vector<u8> file = VabSoundFont::ToSoundFont(vab, "test").Write();
    SoundFont sf2;
    std::string error;
    EXPECT_TRUE(SoundFont::Read(file, sf2, error)) << error;
    VabSoundFont::Vab out;
    EXPECT_TRUE(VabSoundFont::FromSoundFont(sf2, out, error)) << error;
    return out;
}

// The tone table the engine builds from a VH
struct LoadedVab final
{
    u8 mProgCount = 0;
    u8 mVagCount = 0;
    Converted_Vag mTable[128][16] = {};
};

static std::unique_ptr<LoadedVab> OpenHead(std::vector<u8> vh)
{
    const s16 vabId = SsVabOpenHead(reinterpret_cast<VabHeader*>(vh.data()));
    EXPECT_GE(vabId, 0);
    auto loaded = std::make_unique<LoadedVab>();
    if (vabId >= 0)
    {
        IPsxSpuApiVars* pVars = GetSpuApiVars();
        loaded->mProgCount = pVars->sProgCounts()[vabId];
        loaded->mVagCount = pVars->sVagCounts()[vabId];
        memcpy(loaded->mTable, pVars->sConvertedVagTable().table[vabId], sizeof(loaded->mTable));
        pVars->sVagCounts()[vabId] = 0;
        pVars->sProgCounts()[vabId] = 0;
    }
    return loaded;
}

TEST(VabSoundFont, EngineLoadsTheSameTones)
{
    for (const Vab& testVab : {LevelVab(), MonkVab()})
    {
        VabSoundFont::Vab vab;
        vab.mVh = BuildVh(testVab);
        vab.mSamples = ToSamples(testVab);

        const VabSoundFont::Vab converted = RoundTrip(vab);
        const auto before = OpenHead(vab.mVh);
        const auto after = OpenHead(converted.mVh);
        EXPECT_EQ(before->mProgCount, after->mProgCount);
        EXPECT_EQ(before->mVagCount, after->mVagCount);
        EXPECT_EQ(0, memcmp(before->mTable, after->mTable, sizeof(before->mTable)));

        ASSERT_EQ(vab.mSamples.size(), converted.mSamples.size());
        for (size_t i = 0; i < vab.mSamples.size(); i++)
        {
            EXPECT_EQ(vab.mSamples[i].mPcm, converted.mSamples[i].mPcm) << i;
            EXPECT_EQ(vab.mSamples[i].mLoop, converted.mSamples[i].mLoop) << i;
        }
    }
}

TEST(VabSoundFont, KeepsEveryRawToneValue)
{
    Vab testVab = LevelVab();
    // Values the engine doesn't use yet, but a PS1 accurate one will
    Tone& tone = testVab.mPrograms[1].mTones[1];
    tone.mPan = 12;
    tone.mMode = 4;
    testVab.mPrograms.push_back({100, {tone}});

    VabSoundFont::Vab vab;
    vab.mVh = BuildVh(testVab);
    vab.mSamples = ToSamples(testVab);
    const VabSoundFont::Vab converted = RoundTrip(vab);

    // Every tone of every program block, found by program + tone index
    auto tones = [](const std::vector<u8>& vh)
    {
        std::map<std::pair<s32, s32>, std::vector<u8>> ret;
        const VabHeader* pHeader = reinterpret_cast<const VabHeader*>(vh.data());
        const VagAtr* pAtr = reinterpret_cast<const VagAtr*>(vh.data() + sizeof(VabHeader));
        for (s32 i = 0; i < pHeader->field_12_num_progs * 16; i++)
        {
            if (pAtr[i].field_2_vol > 0)
            {
                const u8* p = reinterpret_cast<const u8*>(&pAtr[i]);
                // Up to the reserved words
                ret[{pAtr[i].field_14_prog, i % 16}] = std::vector<u8>(p, p + offsetof(VagAtr, field_18_reserved));
            }
        }
        return ret;
    };
    EXPECT_EQ(tones(vab.mVh), tones(converted.mVh));

    const VabHeader* pBefore = reinterpret_cast<const VabHeader*>(vab.mVh.data());
    const VabHeader* pAfter = reinterpret_cast<const VabHeader*>(converted.mVh.data());
    EXPECT_EQ(pBefore->field_8_id, pAfter->field_8_id);
    EXPECT_EQ(pBefore->field_12_num_progs, pAfter->field_12_num_progs);
    EXPECT_EQ(pBefore->field_14_num_tones, pAfter->field_14_num_tones);
    EXPECT_EQ(pBefore->field_16_num_vags, pAfter->field_16_num_vags);
    EXPECT_EQ(pBefore->field_18_master_vol, pAfter->field_18_master_vol);
    for (s32 prog : {0, 1, 2, 4, 5, 8, 100})
    {
        EXPECT_EQ(0, memcmp(&pBefore->field_20_progs[prog], &pAfter->field_20_progs[prog], sizeof(ProgAtr))) << prog;
    }
}

TEST(VabSoundFont, StandardGenerators)
{
    Vab testVab = LevelVab();
    VabSoundFont::Vab vab;
    vab.mVh = BuildVh(testVab);
    vab.mSamples = ToSamples(testVab);
    const SoundFont sf2 = VabSoundFont::ToSoundFont(vab, "level");

    // Program 1: two tones split at key 60
    const SoundFont::Preset* pPreset = nullptr;
    for (const SoundFont::Preset& p : sf2.mPresets)
    {
        if (p.mProgram == 1)
        {
            pPreset = &p;
        }
    }
    ASSERT_NE(pPreset, nullptr);
    ASSERT_EQ(pPreset->mZones.size(), 2u);
    EXPECT_TRUE(pPreset->mZones[0].IsGlobal(SoundFont::eInstrument));
    const SoundFont::Instrument& inst = sf2.mInstruments[pPreset->mZones[1].Find(SoundFont::eInstrument)->mAmount];
    ASSERT_EQ(inst.mZones.size(), 2u);

    const SoundFont::Zone& low = inst.mZones[0];
    EXPECT_EQ(low.mGens.front().mOper, SoundFont::eKeyRange); // keyRange must come first
    EXPECT_EQ(low.mGens.back().mOper, SoundFont::eSampleId);  // and sampleID last
    EXPECT_EQ(low.Find(SoundFont::eKeyRange)->Lo(), 0);
    EXPECT_EQ(low.Find(SoundFont::eKeyRange)->Hi(), 59);
    EXPECT_EQ(low.Find(SoundFont::eOverridingRootKey)->mAmount, 48);
    EXPECT_EQ(low.Find(SoundFont::eFineTune)->Signed(), -50); // shift 64 = half a semitone up
    EXPECT_EQ(low.Find(SoundFont::eSampleId)->mAmount, 1);
    EXPECT_EQ(low.Find(SoundFont::eSampleModes)->mAmount, 0);

    const SoundFont::Zone& high = inst.mZones[1];
    EXPECT_EQ(high.Find(SoundFont::eKeyRange)->Lo(), 60);
    EXPECT_EQ(high.Find(SoundFont::eSampleModes)->mAmount, 1); // sample 3 loops
    EXPECT_EQ(high.Find(SoundFont::eInitialAttenuation)->mAmount, 30); // vol 90 of 127 = -3 dB

    const SoundFont::Sample& looped = sf2.mSamples[2];
    EXPECT_EQ(looped.mLoopStart, 0u);
    EXPECT_EQ(looped.mLoopEnd, looped.mPcm.size());
    EXPECT_EQ(sf2.mSamples[0].mLoopEnd, 0u);
}

// An SF2 made elsewhere: no private generators
TEST(VabSoundFont, FromPlainSoundFont)
{
    SoundFont sf2;
    SoundFont::Sample sample;
    sample.mPcm = Sine(1000, 50.0, 1000);
    sample.mOriginalPitch = 69;
    sample.mLoopStart = 100;
    sample.mLoopEnd = 900;
    sf2.mSamples.push_back(sample);

    SoundFont::Instrument inst;
    SoundFont::Zone zone;
    zone.AddRange(SoundFont::eKeyRange, 40, 80);
    zone.Add(SoundFont::eFineTune, static_cast<u16>(-25));
    zone.Add(SoundFont::eInitialAttenuation, 60); // -6 dB
    zone.Add(SoundFont::ePan, static_cast<u16>(-500));
    zone.Add(SoundFont::eReverbEffectsSend, 500);
    zone.Add(SoundFont::eSampleId, 0);
    inst.mZones.push_back(zone);
    sf2.mInstruments.push_back(inst);

    SoundFont::Preset preset;
    preset.mProgram = 3;
    SoundFont::Zone presetZone;
    presetZone.Add(SoundFont::eInstrument, 0);
    preset.mZones.push_back(presetZone);
    sf2.mPresets.push_back(preset);

    VabSoundFont::Vab vab;
    std::string error;
    ASSERT_TRUE(VabSoundFont::FromSoundFont(sf2, vab, error)) << error;

    const VabHeader* pHeader = reinterpret_cast<const VabHeader*>(vab.mVh.data());
    EXPECT_EQ(pHeader->field_12_num_progs, 1);
    EXPECT_EQ(pHeader->field_16_num_vags, 1);
    const VagAtr& atr = *reinterpret_cast<const VagAtr*>(vab.mVh.data() + sizeof(VabHeader));
    EXPECT_EQ(atr.field_14_prog, 3);
    EXPECT_EQ(atr.field_16_vag, 1);
    EXPECT_EQ(atr.field_6_min, 40);
    EXPECT_EQ(atr.field_7_max, 80);
    // Root 69 tuned 25 cents down plays at its own pitch at key 69.25: centre 69, shift 32
    EXPECT_EQ(atr.field_4_centre, 69);
    EXPECT_EQ(atr.field_5_shift, 32);
    EXPECT_EQ(atr.field_2_vol, 64);
    EXPECT_EQ(atr.field_3_pan, 0);
    EXPECT_EQ(atr.field_1_mode, 4);
    ASSERT_EQ(vab.mSamples.size(), 1u);
    EXPECT_TRUE(vab.mSamples[0].mLoop);
}

TEST(VabSoundFont, ReadVb)
{
    const Vab testVab = LevelVab();
    const std::vector<u8> vh = BuildVh(testVab);

    std::vector<u8> soundsDat = {1, 2, 3, 4}; // Samples needn't start at 0
    const std::vector<u8> vbAe = BuildVbAe(testVab, soundsDat);
    std::vector<VabSoundFont::Sample> ae;
    ASSERT_TRUE(VabSoundFont::ReadVbAe(vh, vbAe, soundsDat, ae));

    std::vector<VabSoundFont::Sample> ao;
    ASSERT_TRUE(VabSoundFont::ReadVbAo(vh, BuildVbAo(testVab), ao));

    const std::vector<VabSoundFont::Sample> expected = ToSamples(testVab);
    ASSERT_EQ(ae.size(), expected.size());
    ASSERT_EQ(ao.size(), expected.size());
    for (size_t i = 0; i < expected.size(); i++)
    {
        EXPECT_EQ(ae[i].mPcm, expected[i].mPcm) << i;
        EXPECT_EQ(ae[i].mLoop, expected[i].mLoop) << i;
        EXPECT_EQ(ao[i].mPcm, expected[i].mPcm) << i;
        EXPECT_EQ(ao[i].mLoop, expected[i].mLoop) << i;
    }

    // A truncated sounds.dat reads as zeros, as the game's short read leaves them
    soundsDat.resize(soundsDat.size() - 100);
    ASSERT_TRUE(VabSoundFont::ReadVbAe(vh, vbAe, soundsDat, ae));
    EXPECT_EQ(ae.back().mPcm.size(), expected.back().mPcm.size());
    EXPECT_EQ(ae.back().mPcm.back(), 0);
}

TEST(SoundFont, WriteReadRoundTrip)
{
    SoundFont sf2;
    sf2.mName = "name";
    sf2.mComment = "comment";
    for (s32 i = 0; i < 3; i++)
    {
        SoundFont::Sample s;
        s.mName = "sample " + std::to_string(i);
        s.mPcm = Noise(100 + i * 33, i, 5000);
        s.mSampleRate = 22050 + i;
        s.mOriginalPitch = static_cast<u8>(50 + i);
        s.mPitchCorrection = static_cast<s8>(-i);
        s.mLoopStart = static_cast<u32>(i);
        s.mLoopEnd = static_cast<u32>(50 + i);
        sf2.mSamples.push_back(s);

        SoundFont::Instrument inst;
        inst.mName = "a long instrument name " + std::to_string(i); // cut to 19 chars
        SoundFont::Zone global;
        global.Add(SoundFont::ePan, 10);
        inst.mZones.push_back(global);
        SoundFont::Zone zone;
        zone.AddRange(SoundFont::eKeyRange, 1, static_cast<u8>(100 + i));
        zone.Add(0x5005, 0xABCD); // private generators survive
        zone.Add(SoundFont::eSampleId, static_cast<u16>(i));
        inst.mZones.push_back(zone);
        sf2.mInstruments.push_back(inst);

        SoundFont::Preset preset;
        preset.mName = "preset";
        preset.mProgram = static_cast<u16>(i * 2);
        preset.mBank = static_cast<u16>(i);
        SoundFont::Zone presetZone;
        presetZone.Add(SoundFont::eInstrument, static_cast<u16>(i));
        preset.mZones.push_back(presetZone);
        sf2.mPresets.push_back(preset);
    }

    const std::vector<u8> file = sf2.Write();
    SoundFont read;
    std::string error;
    ASSERT_TRUE(SoundFont::Read(file, read, error)) << error;

    EXPECT_EQ(read.mName, sf2.mName);
    EXPECT_EQ(read.mComment, sf2.mComment);
    ASSERT_EQ(read.mSamples.size(), 3u);
    ASSERT_EQ(read.mInstruments.size(), 3u);
    ASSERT_EQ(read.mPresets.size(), 3u);
    for (size_t i = 0; i < 3; i++)
    {
        EXPECT_EQ(read.mSamples[i].mName, sf2.mSamples[i].mName);
        EXPECT_EQ(read.mSamples[i].mPcm, sf2.mSamples[i].mPcm);
        EXPECT_EQ(read.mSamples[i].mSampleRate, sf2.mSamples[i].mSampleRate);
        EXPECT_EQ(read.mSamples[i].mOriginalPitch, sf2.mSamples[i].mOriginalPitch);
        EXPECT_EQ(read.mSamples[i].mPitchCorrection, sf2.mSamples[i].mPitchCorrection);
        EXPECT_EQ(read.mSamples[i].mLoopStart, sf2.mSamples[i].mLoopStart);
        EXPECT_EQ(read.mSamples[i].mLoopEnd, sf2.mSamples[i].mLoopEnd);

        EXPECT_EQ(read.mInstruments[i].mName, sf2.mInstruments[i].mName.substr(0, 19));
        ASSERT_EQ(read.mInstruments[i].mZones.size(), 2u);
        for (size_t z = 0; z < 2; z++)
        {
            const auto& a = read.mInstruments[i].mZones[z].mGens;
            const auto& b = sf2.mInstruments[i].mZones[z].mGens;
            ASSERT_EQ(a.size(), b.size());
            for (size_t g = 0; g < a.size(); g++)
            {
                EXPECT_EQ(a[g].mOper, b[g].mOper);
                EXPECT_EQ(a[g].mAmount, b[g].mAmount);
            }
        }

        EXPECT_EQ(read.mPresets[i].mProgram, sf2.mPresets[i].mProgram);
        EXPECT_EQ(read.mPresets[i].mBank, sf2.mPresets[i].mBank);
        ASSERT_EQ(read.mPresets[i].mZones.size(), 1u);
        EXPECT_EQ(read.mPresets[i].mZones[0].Find(SoundFont::eInstrument)->mAmount, i);
    }
}

TEST(SoundFont, RejectsBadFiles)
{
    SoundFont sf2;
    std::string error;
    EXPECT_FALSE(SoundFont::Read({}, sf2, error));
    EXPECT_FALSE(SoundFont::Read({'R', 'I', 'F', 'F', 4, 0, 0, 0, 'W', 'A', 'V', 'E'}, sf2, error));

    // A zone pointing at a sample that doesn't exist
    SoundFont bad;
    SoundFont::Instrument inst;
    SoundFont::Zone zone;
    zone.Add(SoundFont::eSampleId, 3);
    inst.mZones.push_back(zone);
    bad.mInstruments.push_back(inst);
    EXPECT_FALSE(SoundFont::Read(bad.Write(), sf2, error));

    // Truncated
    std::vector<u8> file = VabSoundFont::ToSoundFont({BuildVh(MonkVab()), {{Sine(100, 10.0, 100), false}}}, "monk").Write();
    file.resize(file.size() / 2);
    EXPECT_FALSE(SoundFont::Read(file, sf2, error));
}

static std::vector<u8> TestSeq()
{
    SeqBuilder seq(480, 612345);
    seq.ProgramChange(0, 3).NoteOn(0, 60, 100).Wait(1000).RunningNoteOn(60, 0).Controller(0, 99, 20).PitchBend(0, -100).Wait(20000).NoteOff(0, 60);
    return seq.End();
}

TEST(SeqMidi, RoundTrip)
{
    const std::vector<u8> seq = TestSeq();
    std::vector<u8> smf;
    std::string error;
    ASSERT_TRUE(SeqMidi::SeqToSmf(seq, smf, error)) << error;

    // A valid type 0 SMF: MThd, then one MTrk as long as the rest of the file
    ASSERT_GE(smf.size(), 22u);
    EXPECT_EQ(0, memcmp(smf.data(), "MThd\0\0\0\x06\0\0\0\x01", 12));
    EXPECT_EQ(smf[12], 480 >> 8);
    EXPECT_EQ(smf[13], 480 & 0xFF);
    EXPECT_EQ(0, memcmp(smf.data() + 14, "MTrk", 4));
    const u32 trackLen = static_cast<u32>((smf[18] << 24) | (smf[19] << 16) | (smf[20] << 8) | smf[21]);
    EXPECT_EQ(trackLen, smf.size() - 22);
    // Tempo first
    EXPECT_EQ(0, memcmp(smf.data() + 22, "\0\xFF\x51\x03", 4));
    // Ends with end of track
    EXPECT_EQ(0, memcmp(smf.data() + smf.size() - 3, "\xFF\x2F\x00", 3));

    std::vector<u8> back;
    ASSERT_TRUE(SeqMidi::SmfToSeq(smf, back, error)) << error;
    EXPECT_EQ(back, seq);
}

TEST(SeqMidi, DropsBytesAfterEndOfTrack)
{
    std::vector<u8> seq = TestSeq();
    const std::vector<u8> original = seq;
    seq.insert(seq.end(), {0x12, 0x34, 0x56});

    std::vector<u8> smf;
    std::string error;
    ASSERT_TRUE(SeqMidi::SeqToSmf(seq, smf, error)) << error;
    std::vector<u8> back;
    ASSERT_TRUE(SeqMidi::SmfToSeq(smf, back, error)) << error;
    EXPECT_EQ(back, original);
}

TEST(SeqMidi, AddsMissingEndOfTrack)
{
    std::vector<u8> seq = TestSeq();
    seq.resize(seq.size() - 4); // delta + FF 2F 00

    std::vector<u8> smf;
    std::string error;
    ASSERT_TRUE(SeqMidi::SeqToSmf(seq, smf, error)) << error;
    EXPECT_EQ(0, memcmp(smf.data() + smf.size() - 4, "\0\xFF\x2F\x00", 4));
}

// A MIDI file made elsewhere: a track name before the tempo, default time signature
TEST(SeqMidi, FromOtherMidiFile)
{
    const u8 smf[] = {
        'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 1, 0, 96,
        'M', 'T', 'r', 'k', 0, 0, 0, 21,
        0x00, 0xFF, 0x03, 0x02, 'h', 'i',          // track name
        0x00, 0xFF, 0x51, 0x03, 0x07, 0xA1, 0x20,  // tempo 500000
        0x00, 0x90, 60, 100,                       // note on
        0x60, 0x80, 60, 0,                         // note off after 96 ticks
    };
    std::vector<u8> seq;
    std::string error;
    ASSERT_TRUE(SeqMidi::SmfToSeq(std::vector<u8>(smf, smf + sizeof(smf)), seq, error)) << error;

    const u8 expected[] = {
        'p', 'Q', 'E', 'S', 0, 0, 0, 1, 0, 96, 0x07, 0xA1, 0x20, 4, 2,
        0x00, 0x90, 60, 100,
        0x60, 0x80, 60, 0,
    };
    EXPECT_EQ(seq, std::vector<u8>(expected, expected + sizeof(expected)));
}

TEST(SeqMidi, RejectsOtherFiles)
{
    std::vector<u8> out;
    std::string error;
    EXPECT_FALSE(SeqMidi::SeqToSmf({'M', 'T', 'h', 'd'}, out, error));
    EXPECT_FALSE(SeqMidi::SmfToSeq(TestSeq(), out, error));

    // Two tracks
    const u8 type1[] = {'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 1, 0, 2, 0, 96};
    EXPECT_FALSE(SeqMidi::SmfToSeq(std::vector<u8>(type1, type1 + sizeof(type1)), out, error));
}
