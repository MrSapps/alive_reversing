// Gold tests for the sound code: each scenario plays synthetic VAB/SEQ data through the real
// sound code (offline, see SoundGoldSession) and compares the trace with a committed gold file
// in tests/sound_gold. They pin the current behaviour so the sound code and its data formats can
// be refactored without changing what the games sound like.
//
// The scenarios are played on the emulated PS1 SPU, and the traces list its voices.
//
// A deliberate behaviour change updates the gold files: run the tests with
// RELIVE_UPDATE_SOUND_GOLD=1 and review the diff. A failing test saves its trace and a WAV of its
// audio to <temp>/relive_sound_gold/ to compare with.

#include <gtest/gtest.h>
#include "ScratchDir.hpp"
#include "SoundTestData.hpp"
#include "Sound/SoundGold.hpp"
#include "Sound/Midi.hpp"
#include "Sound/GameSound.hpp"
#include "Sound/PsxSoundEngine.hpp"
#include "BinaryPath.hpp"
#include "ResourceManagerWrapper.hpp"
#include "Sfx.hpp"
#include "data_conversion/file_system.hpp"
#include "../AliveLibAE/PathData.hpp"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>

namespace AO {
extern OpenSeqHandle g_SeqTable_4C9E70[165];
}

static const char_type* kTheme = "TEST";

static OpenSeqHandle* SeqTable(GameType game)
{
    return game == GameType::eAo ? AO::g_SeqTable_4C9E70 : gSeqData.mSeqs;
}

// SEQs that exist in the game's table, so SND_Load_Seqs can find them
static std::vector<u16> TestSeqIndices(GameType game, size_t count)
{
    std::vector<u16> ret;
    for (u16 i = 0; ret.size() < count; i++)
    {
        if (SeqTable(game)[i].field_0_mBsqName)
        {
            ret.push_back(i);
        }
    }
    return ret;
}

static SoundTestData::Vab LevelVab()
{
    using namespace SoundTestData;
    Vab vab;
    vab.mId = 1;
    vab.mSamples = {
        {Sine(6000, 100.0, 12000), false},
        {Saw(3000, 80, 10000), false}, // Shorter than 4000, which AE pads
        {Sine(2205, 49.0, 9000), true},
        {Noise(1500, 1234, 8000), false},
    };

    Tone plain;
    plain.mVag = 1;

    Tone low;
    low.mVag = 2;
    low.mMax = 59;
    low.mCentre = 48;
    low.mShift = 64;
    low.mAdsr1 = 0x3A8F;
    low.mAdsr2 = 0x5FC5;

    Tone high;
    high.mVag = 3;
    high.mMin = 60;
    high.mCentre = 72;
    high.mAdsr1 = 0x7F00;
    high.mAdsr2 = 0x1FDF;
    high.mVol = 90;
    high.mPriority = 5;

    Tone looped;
    looped.mVag = 3;
    looped.mAdsr1 = 0x2A4F;
    looped.mAdsr2 = 0x0006;
    looped.mMode = 4;
    looped.mPan = 30;

    Tone quiet = plain;
    quiet.mVol = 80;

    Tone noise;
    noise.mVag = 4;
    noise.mPriority = 10;
    noise.mAdsr1 = 0x00FF;
    noise.mAdsr2 = 0x1FC0;

    Tone layer1 = plain;
    layer1.mVol = 60;
    Tone layer2;
    layer2.mVag = 2;
    layer2.mCentre = 67;
    layer2.mVol = 50;
    layer2.mAdsr1 = 0x4A3F;
    layer2.mAdsr2 = 0x3FD3;

    vab.mPrograms = {
        {0, {plain}},
        {1, {low, high}},
        {2, {looped}},
        {4, {quiet}}, // AE waits 10 ms after programs 4, 5, 8, 23, 24 and 25
        {5, {noise}},
        {8, {layer1, layer2}},
    };
    return vab;
}

static std::vector<u8> MelodySeq()
{
    SoundTestData::SeqBuilder seq(480, 500000);
    seq.ProgramChange(0, 1)
        .ProgramChange(1, 8)
        .NoteOn(0, 55, 100)
        .Wait(240)
        .RunningNoteOn(55, 0) // note off as note on with velocity 0, running status
        .NoteOn(0, 64, 110)
        .Wait(120)
        .PitchBend(0, 2048)
        .Wait(120)
        .PitchBend(0, 0)
        .NoteOff(0, 64)
        .LoopStart(1, 2)
        .NoteOn(1, 60, 90)
        .Wait(240)
        .NoteOff(1, 60)
        .LoopEnd(1)
        .Controller(0, 7, 80)
        .NoteOn(0, 70, 127)
        .Wait(480)
        .NoteOff(0, 70);
    return seq.End();
}

static std::vector<u8> DrumSeq()
{
    SoundTestData::SeqBuilder seq(96, 400000);
    seq.ProgramChange(9, 5);
    for (u8 i = 0; i < 6; i++)
    {
        seq.NoteOn(9, static_cast<u8>(50 + i * 3), 100).Wait(24).NoteOff(9, static_cast<u8>(50 + i * 3)).Wait(24);
    }
    return seq.End();
}

// A looping tone held for 4 s
static std::vector<u8> HeldSeq()
{
    SoundTestData::SeqBuilder seq(480, 500000);
    seq.ProgramChange(0, 2).NoteOn(0, 60, 100).Wait(480 * 8).NoteOff(0, 60);
    return seq.End();
}

// The held note's program and key struck twice, then one note off
static std::vector<u8> RepeatSeq()
{
    SoundTestData::SeqBuilder seq(480, 500000);
    seq.ProgramChange(0, 2).NoteOn(0, 60, 100).Wait(240).NoteOn(0, 60, 100).Wait(240).NoteOff(0, 60).Wait(960);
    return seq.End();
}

// The held note faded by the channel volume (CC 7) while it plays: to half, then to 0. Then a note
// at channel volume 0.
static std::vector<u8> FadeSeq()
{
    SoundTestData::SeqBuilder seq(480, 500000);
    seq.ProgramChange(0, 2)
        .NoteOn(0, 60, 100)
        .Wait(480)
        .Controller(0, 7, 64)
        .Wait(480)
        .Controller(0, 7, 0)
        .Wait(480)
        .NoteOff(0, 60)
        .NoteOn(0, 62, 100)
        .Wait(480)
        .NoteOff(0, 62);
    return seq.End();
}

// Writes the synthetic data where ResourceManagerWrapper looks for it. extraSeqs: also load HeldSeq
// (1), RepeatSeq (2) and FadeSeq (3). Only some scenarios load them, so the others' traces don't change.
static std::shared_ptr<PathSoundInfo> WriteTestData(const ScratchDir& dir, GameType game, s32 extraSeqs = 0)
{
    namespace fs = std::filesystem;
    const fs::path themeDir = fs::path(dir.Path()) / "relive_data" / (game == GameType::eAo ? "ao" : "ae") / "sounds" / kTheme;
    fs::create_directories(themeDir);

    auto save = [](const fs::path& path, const std::vector<u8>& data)
    {
        std::ofstream f(path, std::ios::binary);
        f.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    };

    std::vector<u8> soundsDat;
    const SoundTestData::Vab level = LevelVab();
    save(themeDir / "TEST.VH", SoundTestData::BuildVh(level));
    if (game == GameType::eAo)
    {
        save(themeDir / "TEST.VB", SoundTestData::BuildVbAo(level));
    }
    else
    {
        save(themeDir / "TEST.VB", SoundTestData::BuildVbAe(level, soundsDat));
        save(fs::path(dir.Path()) / "sounds.dat", soundsDat);
    }

    auto info = std::make_shared<PathSoundInfo>();
    info->mVhFile = "TEST.VH";
    info->mVbFile = "TEST.VB";
    info->mSoundTheme = kTheme;

    const std::vector<u16> seqs = TestSeqIndices(game, 2 + static_cast<size_t>(extraSeqs));
    const std::vector<u8> seqData[] = {MelodySeq(), DrumSeq(), HeldSeq(), RepeatSeq(), FadeSeq()};
    for (size_t i = 0; i < seqs.size(); i++)
    {
        const char_type* pName = SeqTable(game)[seqs[i]].field_0_mBsqName;
        save(themeDir / pName, seqData[i]);
        info->mSeqFiles.push_back(pName);
    }
    return info;
}

static std::string ReadText(const std::filesystem::path& path)
{
    std::ifstream f(path, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    std::string text = ss.str();
    // Git may have given the gold files CRLF line endings
    text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
    return text;
}

static void WriteFile(const std::filesystem::path& path, const std::string& text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream f(path, std::ios::binary);
    f.write(text.data(), static_cast<std::streamsize>(text.size()));
}

static void CompareWithGold(const std::string& name, const std::string& trace, const std::vector<StereoSample_S16>& audio)
{
    namespace fs = std::filesystem;
    const fs::path goldPath = fs::path(RELIVE_SOUND_GOLD_DIR) / (name + ".txt");
    const fs::path actualDir = fs::temp_directory_path() / "relive_sound_gold";

    const std::vector<u8> wav = SoundGoldSession::ToWav(audio);
    const std::string wavText(wav.begin(), wav.end());

    const char* pUpdate = std::getenv("RELIVE_UPDATE_SOUND_GOLD");
    if (pUpdate && *pUpdate && std::string(pUpdate) != "0")
    {
        WriteFile(goldPath, trace);
        WriteFile(actualDir / (name + ".wav"), wavText);
        return;
    }

    ASSERT_TRUE(fs::exists(goldPath)) << "Missing gold file " << goldPath.string() << ", make it with RELIVE_UPDATE_SOUND_GOLD=1";
    const std::string gold = ReadText(goldPath);
    if (gold == trace)
    {
        return;
    }

    WriteFile(actualDir / (name + ".txt"), trace);
    WriteFile(actualDir / (name + ".wav"), wavText);

    // Point at the first line that differs
    std::istringstream goldLines(gold);
    std::istringstream actualLines(trace);
    std::string goldLine;
    std::string actualLine;
    s32 lineNum = 1;
    while (true)
    {
        const bool haveGold = static_cast<bool>(std::getline(goldLines, goldLine));
        const bool haveActual = static_cast<bool>(std::getline(actualLines, actualLine));
        if (!haveGold && !haveActual)
        {
            break;
        }
        if (!haveGold || !haveActual || goldLine != actualLine)
        {
            ADD_FAILURE() << name << " differs from " << goldPath.string() << " at line " << lineNum << "\n  gold:   " << (haveGold ? goldLine : "<end>")
                          << "\n  actual: " << (haveActual ? actualLine : "<end>") << "\nThe trace and audio are in " << actualDir.string();
            return;
        }
        lineNum++;
    }
}

static void RunGold(GameType game, const std::string& scenario, const std::function<void(SoundGoldSession&)>& play, s32 extraSeqs = 0)
{
    SetGameType(game);
    ScratchDir dir;
    const std::shared_ptr<PathSoundInfo> info = WriteTestData(dir, game, extraSeqs);

    std::string trace;
    std::vector<StereoSample_S16> audio;
    {
        ScopedCurrentDir cwd(dir.Path());
        FileSystem fs;
        ResourceManagerWrapper resMan(fs, "");

        SoundGoldSession session(game);
        session.LoadSoundBlock(resMan, info, 40);
        play(session);
        trace = session.Trace();
        audio = session.Audio();
    }

    EXPECT_TRUE(std::any_of(audio.begin(), audio.end(), [](const StereoSample_S16& s)
    {
        return s.left != 0 || s.right != 0;
    })) << "The SPU played nothing";

    CompareWithGold(std::string(game == GameType::eAo ? "ao_" : "ae_") + scenario, trace, audio);
}

static relive::SfxDefinition Sfx(s32 program, s32 note, s16 volume, s16 pitchMin, s16 pitchMax)
{
    return relive::SfxDefinition{0, program, note, volume, pitchMin, pitchMax};
}

static s32 KeyOn(SoundGoldSession& session, s32 program, s32 note, u16 vol)
{
    session.Note("key on prog=" + std::to_string(program) + " note=" + std::to_string(note));
    return GameSound::Get()->Engine().VoKeyOn((session.VabId() << 8) | program, note << 8, vol, vol);
}

static void Sfx(GameType game)
{
    RunGold(game, "sfx", [](SoundGoldSession& session)
    {
        session.Note("sfx stereo prog 0");
        SFX_SfxDefinition_Play_Stereo(Sfx(0, 60, 100, 0, 0), 100, 60, 0, 0);
        session.Advance(300);

        session.Note("sfx mono prog 1 low, pitch variation");
        SFX_SfxDefinition_Play_Mono(Sfx(1, 50, 90, -127, 127), 0, 0x7FFF, 0x7FFF);
        session.Advance(200);

        session.Note("sfx mono prog 1 high");
        SFX_SfxDefinition_Play_Mono(Sfx(1, 72, 90, 0, 0), 120, 0x7FFF, 0x7FFF);
        session.Advance(200);

        session.Note("sfx stereo prog 4 twice");
        SFX_SfxDefinition_Play_Stereo(Sfx(4, 60, 100, 0, 0), 127, 127, 0, 0);
        SFX_SfxDefinition_Play_Stereo(Sfx(4, 62, 100, 0, 0), 127, 127, 0, 0);
        session.Advance(300);

        session.Note("sfx mono prog 5 and 8");
        SFX_SfxDefinition_Play_Mono(Sfx(5, 40, 100, -300, 300), 0, 0x7FFF, 0x7FFF);
        SFX_SfxDefinition_Play_Mono(Sfx(8, 60, 100, 0, 0), 0, 0x7FFF, 0x7FFF);
        session.Advance(600);
    });
}

static void Adsr(GameType game)
{
    RunGold(game, "adsr", [](SoundGoldSession& session)
    {
        const s32 attack = KeyOn(session, 1, 64, 100);
        const s32 looped = KeyOn(session, 2, 60, 110);
        const s32 layered = KeyOn(session, 8, 60, 90);
        const s32 noise = KeyOn(session, 5, 45, 127);
        session.Advance(400);

        session.Note("key off all but the loop");
        SND_Stop_Channels_Mask(static_cast<u32>(attack | layered | noise));
        session.Advance(700);

        session.Note("key off the loop");
        SND_Stop_Channels_Mask(static_cast<u32>(looped));
        session.Advance(1500);

        session.Note("key on the same note twice");
        KeyOn(session, 1, 70, 100);
        KeyOn(session, 1, 70, 100);
        session.Advance(300);
        session.Note("all key off");
        SND_AllKeyOff();
        session.Advance(800);
    });
}

static void Seq(GameType game)
{
    RunGold(game, "seq", [](SoundGoldSession& session)
    {
        const std::vector<u16> seqs = session.LoadedSeqs();
        ASSERT_EQ(seqs.size(), 2u);

        session.Note("play melody twice");
        SND_SEQ_Play(seqs[0], 2, 100, 90);
        session.Advance(1000);

        session.Note("play drums once");
        SND_SEQ_PlaySeq(seqs[1], 1, 1);
        session.Advance(2000);

        session.Note("set melody volume");
        SND_SEQ_SetVol(seqs[0], 60, 60);
        session.Advance(2000);

        session.Note("restart drums then stop them");
        SND_SEQ_PlaySeq(seqs[1], 0, 1);
        session.Advance(500);
        SND_SEQ_Stop(seqs[1]);
        session.Advance(1500);
        session.Note("stop all");
        SND_Stop_All_Seqs();
        session.Advance(1000);
    });
}

static void Steal(GameType game)
{
    RunGold(game, "steal", [](SoundGoldSession& session)
    {
        // More notes than the 24 channels, with different priorities
        const s32 programs[] = {0, 1, 2, 5, 8};
        for (s32 i = 0; i < 30; i++)
        {
            KeyOn(session, programs[i % 5], 40 + i, static_cast<u16>(60 + i));
            session.Advance(10);
        }
        session.Advance(1500);
        session.Note("key off every channel");
        SND_Stop_Channels_Mask(0xFFFFFF);
        session.Advance(1000);
    });
}

// A SEQ stopped while it holds a note (e.g. the game stopping the music when Abe dies) must key its
// voices off: its note offs will never come
static void StopMidNote(GameType game)
{
    RunGold(game, "stop_mid_note", [](SoundGoldSession& session)
    {
        const std::vector<u16> seqs = session.LoadedSeqs();
        ASSERT_EQ(seqs.size(), 3u);

        // Another SEQ first, so the held one isn't SEQ slot 0 (as in the game, where the ambience
        // and music are open too)
        session.Note("play the melody, then the held note");
        SND_SEQ_Play(seqs[0], 1, 100, 100);
        SND_SEQ_Play(seqs[2], 1, 100, 100);
        session.Advance(500);
        session.Note("stop the SEQ");
        SND_SEQ_Stop(seqs[2]);
        session.Advance(100);

        // The held note (program 2) must be keyed off straight away. The melody is still playing,
        // so its end can't release it by accident.
        for (s32 i = 0; i < PsxSpu::kNumVoices; i++)
        {
            const Libsnd::VoiceInfo info = GameSound::Get()->Engine().GetVoiceState(i).mLibsnd;
            EXPECT_FALSE(info.mProgram == 2 && info.mKeyed) << "Voice " << i << " is still held";
        }
        session.Advance(1500);
    }, 1);
}

// With the PS1 sound, SEQ note offs follow libsnd: a note off releases every voice of its SEQ playing
// that program and note (no reference counts), and never another SEQ's
static void NoteOffRules(GameType game)
{
    RunGold(game, "note_off_rules", [game](SoundGoldSession& session)
    {
        const std::vector<u16> seqs = session.LoadedSeqs();
        ASSERT_EQ(seqs.size(), 4u);

        session.Note("play the held note, then the repeated one");
        SND_SEQ_Play(seqs[2], 1, 100, 100);
        SND_SEQ_Play(seqs[3], 1, 100, 100);
        const s16 heldSlot = SeqTable(game)[seqs[2]].field_A_id_seqOpenId;
        const s16 repeatSlot = SeqTable(game)[seqs[3]].field_A_id_seqOpenId;
        // Sounding (keyed on, not released) voices of a SEQ slot
        auto sounding = [](s16 slot)
        {
            s32 count = 0;
            for (s32 i = 0; i < PsxSpu::kNumVoices; i++)
            {
                const Libsnd::VoiceInfo info = PsxSoundEngine::Get()->GetVoiceState(i).mLibsnd;
                if (info.mSeq == slot && info.mKeyed)
                {
                    count++;
                }
            }
            return count;
        };

        session.Advance(400); // Both strikes of the repeated note are playing
        EXPECT_EQ(sounding(heldSlot), 1);
        EXPECT_EQ(sounding(repeatSlot), 2) << "Each strike should get its own voice";

        session.Advance(250); // Past the repeated note's single note off at 500 ms
        EXPECT_EQ(sounding(repeatSlot), 0) << "One note off should release both strikes";
        EXPECT_EQ(sounding(heldSlot), 1) << "Another SEQ's note off released the held note";

        session.Note("stop");
        SND_Stop_All_Seqs();
        session.Advance(1000);
    }, 2);
}

// With the PS1 sound, a SEQ's channel volume (CC 7) changes its playing notes too, as libsnd's
// _SsContMainVol does, and SsSeqStop sets it back to 127
static void ChannelVolume(GameType game)
{
    RunGold(game, "channel_volume", [](SoundGoldSession& session)
    {
        const std::vector<u16> seqs = session.LoadedSeqs();
        ASSERT_EQ(seqs.size(), 5u);

        // The left volume register of the sounding note of program 2, or -1
        auto heldVolume = []()
        {
            for (s32 i = 0; i < PsxSpu::kNumVoices; i++)
            {
                const PsxSoundEngine::VoiceState state = PsxSoundEngine::Get()->GetVoiceState(i);
                if (state.mLibsnd.mProgram == 2 && state.mLibsnd.mKeyed && state.mLibsnd.mSeq >= 0 && state.mLibsnd.mSeq < Libsnd::kMaxSeqs)
                {
                    return static_cast<s32>(state.mVolLeft);
                }
            }
            return -1;
        };

        session.Note("play the fade");
        SND_SEQ_Play(seqs[4], 1, 100, 100);
        session.Advance(250);
        const s32 full = heldVolume();
        EXPECT_GT(full, 0);

        session.Advance(500); // Past the channel volume 64 at 500 ms
        const s32 half = heldVolume();
        EXPECT_GT(half, 0);
        EXPECT_LT(half, full / 3) << "The channel volume should turn the playing note down (squared)";

        session.Advance(500); // Past the channel volume 0 at 1000 ms
        EXPECT_EQ(heldVolume(), 0);

        session.Note("stop and play again");
        SND_SEQ_Stop(seqs[4]);
        session.Advance(100);
        SND_SEQ_Play(seqs[4], 1, 100, 100);
        session.Advance(250);
        EXPECT_EQ(heldVolume(), full) << "SsSeqStop should set the channel volume back to 127";

        session.Note("stop");
        SND_Stop_All_Seqs();
        session.Advance(500);
    }, 3);
}

TEST(SoundGold, AE_Sfx) { Sfx(GameType::eAe); }
TEST(SoundGold, AO_Sfx) { Sfx(GameType::eAo); }
TEST(SoundGold, AE_Adsr) { Adsr(GameType::eAe); }
TEST(SoundGold, AO_Adsr) { Adsr(GameType::eAo); }
TEST(SoundGold, AE_Seq) { Seq(GameType::eAe); }
TEST(SoundGold, AO_Seq) { Seq(GameType::eAo); }
TEST(SoundGold, AE_Steal) { Steal(GameType::eAe); }
TEST(SoundGold, AO_Steal) { Steal(GameType::eAo); }
TEST(SoundGold, AE_StopMidNote) { StopMidNote(GameType::eAe); }
TEST(SoundGold, AO_StopMidNote) { StopMidNote(GameType::eAo); }
TEST(SoundGold, AE_NoteOffRules) { NoteOffRules(GameType::eAe); }
TEST(SoundGold, AO_NoteOffRules) { NoteOffRules(GameType::eAo); }
TEST(SoundGold, AE_ChannelVolume) { ChannelVolume(GameType::eAe); }
TEST(SoundGold, AO_ChannelVolume) { ChannelVolume(GameType::eAo); }

// The same calls must give the same trace and audio every time
TEST(SoundGold, Deterministic)
{
    std::string traces[2];
    for (std::string& trace : traces)
    {
        SetGameType(GameType::eAe);
        ScratchDir dir;
        const std::shared_ptr<PathSoundInfo> info = WriteTestData(dir, GameType::eAe);
        ScopedCurrentDir cwd(dir.Path());
        FileSystem fs;
        ResourceManagerWrapper resMan(fs, "");
        SoundGoldSession session(GameType::eAe);
        session.LoadSoundBlock(resMan, info, 0);
        SND_SEQ_Play(session.LoadedSeqs()[0], 1, 100, 100);
        KeyOn(session, 2, 60, 100);
        session.Advance(1500);
        const std::vector<u8> wav = SoundGoldSession::ToWav(session.Audio());
        trace = session.Trace() + std::to_string(SoundGoldSession::Crc32(wav.data(), wav.size()));
    }
    EXPECT_EQ(traces[0], traces[1]);
}
