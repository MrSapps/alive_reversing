// Gold tests for the sound code: each scenario plays synthetic VAB/SEQ data through the real
// sound code (offline, see SoundGoldSession) and compares the trace with a committed gold file
// in tests/sound_gold. They pin the current behaviour so the sound code and its data formats can
// be refactored without changing what the games sound like.
//
// The ps1_ gold files are the same scenarios played on the emulated PS1 SPU (-ps1_sound).
//
// A deliberate behaviour change updates the gold files: run the tests with
// RELIVE_UPDATE_SOUND_GOLD=1 and review the diff. A failing test saves its trace and a WAV of its
// audio to <temp>/relive_sound_gold/ to compare with.

#include <gtest/gtest.h>
#include "ScratchDir.hpp"
#include "SoundTestData.hpp"
#include "Sound/SoundGold.hpp"
#include "Sound/Midi.hpp"
#include "Sound/PsxSpuApi.hpp"
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

// Two SEQs that exist in the game's table, so SND_Load_Seqs can find them
static std::vector<u16> TestSeqIndices(GameType game)
{
    std::vector<u16> ret;
    for (u16 i = 0; ret.size() < 2; i++)
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

static SoundTestData::Vab MonkVab()
{
    using namespace SoundTestData;
    Vab vab;
    vab.mId = 0;
    vab.mSamples = {{Sine(3000, 70.0, 8000), false}};
    Tone tone;
    vab.mPrograms = {{0, {tone}}};
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

// Writes the synthetic data where ResourceManagerWrapper looks for it
static std::shared_ptr<PathSoundInfo> WriteTestData(const ScratchDir& dir, GameType game)
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
    const SoundTestData::Vab monk = MonkVab();
    save(themeDir / "TEST.VH", SoundTestData::BuildVh(level));
    save(themeDir / "MONK.VH", SoundTestData::BuildVh(monk));
    if (game == GameType::eAo)
    {
        save(themeDir / "TEST.VB", SoundTestData::BuildVbAo(level));
        save(themeDir / "MONK.VB", SoundTestData::BuildVbAo(monk));
    }
    else
    {
        save(themeDir / "TEST.VB", SoundTestData::BuildVbAe(level, soundsDat));
        save(themeDir / "MONK.VB", SoundTestData::BuildVbAe(monk, soundsDat));
        save(fs::path(dir.Path()) / "sounds.dat", soundsDat);
    }

    auto info = std::make_shared<PathSoundInfo>();
    info->mVhFile = "TEST.VH";
    info->mVbFile = "TEST.VB";
    info->mSoundTheme = kTheme;

    const std::vector<u16> seqs = TestSeqIndices(game);
    const std::vector<u8> seqData[] = {MelodySeq(), DrumSeq()};
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

static void RunGold(GameType game, bool ps1Sound, const std::string& scenario, const std::function<void(SoundGoldSession&)>& play)
{
    SetGameType(game);
    ScratchDir dir;
    const std::shared_ptr<PathSoundInfo> info = WriteTestData(dir, game);

    std::string trace;
    std::vector<StereoSample_S16> audio;
    {
        ScopedCurrentDir cwd(dir.Path());
        FileSystem fs;
        ResourceManagerWrapper resMan(fs, "");

        SoundGoldSession session(game, ps1Sound);
        // Only the SPU has a reverb
        session.LoadSoundBlock(resMan, info, ps1Sound ? 40 : 0, kTheme);
        play(session);
        trace = session.Trace();
        audio = session.Audio();
    }

    if (ps1Sound)
    {
        EXPECT_TRUE(std::any_of(audio.begin(), audio.end(), [](const StereoSample_S16& s)
        {
            return s.left != 0 || s.right != 0;
        })) << "The SPU played nothing";
    }

    CompareWithGold(std::string(ps1Sound ? "ps1_" : "") + (game == GameType::eAo ? "ao_" : "ae_") + scenario, trace, audio);
}

static relive::SfxDefinition Sfx(s32 program, s32 note, s16 volume, s16 pitchMin, s16 pitchMax)
{
    return relive::SfxDefinition{0, program, note, volume, pitchMin, pitchMax};
}

static s32 KeyOn(SoundGoldSession& session, s32 program, s32 note, u16 vol)
{
    session.Note("key on prog=" + std::to_string(program) + " note=" + std::to_string(note));
    return SsVoKeyOn_4FCF10((session.VabId() << 8) | program, note << 8, vol, vol);
}

static void Sfx(GameType game, bool ps1Sound)
{
    RunGold(game, ps1Sound, "sfx", [](SoundGoldSession& session)
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

static void Adsr(GameType game, bool ps1Sound)
{
    RunGold(game, ps1Sound, "adsr", [](SoundGoldSession& session)
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
        SsUtAllKeyOff(0);
        session.Advance(800);
    });
}

static void Seq(GameType game, bool ps1Sound)
{
    RunGold(game, ps1Sound, "seq", [](SoundGoldSession& session)
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

static void Steal(GameType game, bool ps1Sound)
{
    RunGold(game, ps1Sound, "steal", [](SoundGoldSession& session)
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

TEST(SoundGold, AE_Sfx) { Sfx(GameType::eAe, false); }
TEST(SoundGold, AO_Sfx) { Sfx(GameType::eAo, false); }
TEST(SoundGold, AE_Adsr) { Adsr(GameType::eAe, false); }
TEST(SoundGold, AO_Adsr) { Adsr(GameType::eAo, false); }
TEST(SoundGold, AE_Seq) { Seq(GameType::eAe, false); }
TEST(SoundGold, AO_Seq) { Seq(GameType::eAo, false); }
TEST(SoundGold, AE_Steal) { Steal(GameType::eAe, false); }
TEST(SoundGold, AO_Steal) { Steal(GameType::eAo, false); }
TEST(SoundGold, PS1_AE_Sfx) { Sfx(GameType::eAe, true); }
TEST(SoundGold, PS1_AO_Sfx) { Sfx(GameType::eAo, true); }
TEST(SoundGold, PS1_AE_Adsr) { Adsr(GameType::eAe, true); }
TEST(SoundGold, PS1_AO_Adsr) { Adsr(GameType::eAo, true); }
TEST(SoundGold, PS1_AE_Seq) { Seq(GameType::eAe, true); }
TEST(SoundGold, PS1_AO_Seq) { Seq(GameType::eAo, true); }
TEST(SoundGold, PS1_AE_Steal) { Steal(GameType::eAe, true); }
TEST(SoundGold, PS1_AO_Steal) { Steal(GameType::eAo, true); }

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
        session.LoadSoundBlock(resMan, info, 0, kTheme);
        SND_SEQ_Play(session.LoadedSeqs()[0], 1, 100, 100);
        KeyOn(session, 2, 60, 100);
        session.Advance(1500);
        const std::vector<u8> wav = SoundGoldSession::ToWav(session.Audio());
        trace = session.Trace() + std::to_string(SoundGoldSession::Crc32(wav.data(), wav.size()));
    }
    EXPECT_EQ(traces[0], traces[1]);
}
