// Renders every sound theme of a converted game data dir through the real sound code (offline,
// see SoundGoldSession): each program's tones, and each SEQ. Writes a trace and a WAV of each,
// and with -baseline compares them with an earlier run's. See
// Source/relive_lib/Sound/SOUND_FORMATS.md.

#include "../../relive_lib/stdafx.h"
#include "../../relive_lib/CommandLineParser.hpp"
#include "../../relive_lib/GameType.hpp"
#include "../../relive_lib/BinaryPath.hpp"
#include "../../relive_lib/ResourceManagerWrapper.hpp"
#include "../../relive_lib/Sound/SoundGold.hpp"
#include "../../relive_lib/Sound/Midi.hpp"
#include "../../relive_lib/Sound/PsxSpuApi.hpp"
#include "../../relive_lib/data_conversion/file_system.hpp"
#include "../../AliveLibAE/GameAutoPlayer.hpp"
#include "../../AliveLibAE/PathData.hpp"
#include "../../AliveLibAO/GameAutoPlayer.hpp"
#include <SDL3/SDL_main.h>
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>

static const char* kUsage =
    "Usage: relive_sound_gold -data=<dir> [options]\n"
    "Plays every sound theme of converted game data through the sound code, offline, and saves\n"
    "a trace and a WAV of each program's tones and of each SEQ.\n"
    "\n"
    "  -data=<dir>       The game dir (holds relive_data/, and sounds.dat for AE)\n"
    "  -AO               Abe's Oddysee data (default: Abe's Exoddus)\n"
    "  -out=<dir>        Where to write (default sound_gold_out)\n"
    "  -baseline=<dir>   Compare with an earlier run's -out, exit code 1 if anything differs\n"
    "  -theme=<name>     Only this sound theme\n"
    "  -seq_ms=<ms>      Longest time a SEQ is played for (default 60000)\n"
    "  -ps1_sound        Play on the emulated PS1 SPU, as relive -ps1_sound does\n"
    "  -reverb=<depth>   The path reverb depth, 0-127 (default 0). Only the SPU has a reverb\n"
    "  -seq_vol=<vol>    The volume the SEQs are played at, 0-127 (default 100)\n"
    "  -spu_filter=<f>   With -ps1_sound: hq (default) or gaussian, as relive -spu_filter\n";

// relive_lib calls out to this for recording and playback, which this tool doesn't use
BaseGameAutoPlayer& GetGameAutoPlayer()
{
    static GameAutoPlayer autoPlayerAE;
    static AO::GameAutoPlayer autoPlayerAO;
    if (GetGameType() == GameType::eAo)
    {
        return autoPlayerAO;
    }
    return autoPlayerAE;
}

namespace fs = std::filesystem;

static bool WriteBytes(const fs::path& path, const void* pData, size_t size)
{
    fs::create_directories(path.parent_path());
    std::ofstream f(path, std::ios::binary);
    f.write(static_cast<const char*>(pData), static_cast<std::streamsize>(size));
    return f.good();
}

static std::string ReadAll(const fs::path& path)
{
    std::ifstream f(path, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

struct Options final
{
    GameType mGame = GameType::eAe;
    fs::path mOut = "sound_gold_out";
    std::string mTheme;
    u32 mSeqMs = 60000;
    s32 mReverb = 0;
    bool mPs1Sound = false;
    s16 mSeqVol = 100;
    PsxSpu::Interpolation mInterpolation = PsxSpu::Interpolation::BandLimited;
};

// Plays one item in a new session so each result stands alone
static void RenderItem(const Options& options, ResourceManagerWrapper& resMan, const std::string& theme, const std::string& monkTheme, const std::string& name,
    const std::function<void(SoundGoldSession&)>& play)
{
    const ResourceManagerWrapper::SoundThemeInfo& themeInfo = resMan.LoadSoundThemeInfo(theme);
    auto info = std::make_shared<PathSoundInfo>();
    info->mVhFile = themeInfo.mVhFile;
    info->mVbFile = themeInfo.mVbFile;
    info->mSeqFiles = themeInfo.mSeqFiles;
    info->mSoundTheme = theme;

    SoundGoldSession session(options.mGame, options.mPs1Sound, options.mInterpolation);
    session.LoadSoundBlock(resMan, info, options.mReverb, monkTheme);
    play(session);

    const fs::path base = options.mOut / theme / name;
    const std::string& trace = session.Trace();
    const std::vector<u8> wav = SoundGoldSession::ToWav(session.Audio());
    WriteBytes(fs::path(base.string() + ".txt"), trace.data(), trace.size());
    WriteBytes(fs::path(base.string() + ".wav"), wav.data(), wav.size());
    printf("  %s: %.1f s\n", name.c_str(), static_cast<f64>(session.Audio().size()) / 44100.0);
}

static void RenderTheme(const Options& options, ResourceManagerWrapper& resMan, const std::string& theme, const std::string& monkTheme)
{
    printf("%s\n", theme.c_str());

    // Every program's tones, at its root key and a fifth above
    RenderItem(options, resMan, theme, monkTheme, "tones", [](SoundGoldSession& session)
    {
        for (const s32 program : session.Programs())
        {
            for (const s32 offset : {0, 7})
            {
                const s32 note = session.RootKey(program) + offset;
                session.Note("key on prog=" + std::to_string(program) + " note=" + std::to_string(note));
                const s32 channels = SsVoKeyOn_4FCF10((session.VabId() << 8) | program, note << 8, 100, 100);
                session.Advance(400);
                session.Note("key off");
                SND_Stop_Channels_Mask(static_cast<u32>(channels));
                session.Advance(600);
            }
        }
    });

    // Each SEQ once, until it ends or -seq_ms
    const std::vector<std::string> seqNames = resMan.LoadSoundThemeInfo(theme).mSeqFiles;
    for (const std::string& seqName : seqNames)
    {
        RenderItem(options, resMan, theme, monkTheme, "seq_" + seqName, [&](SoundGoldSession& session)
        {
            for (const u16 idx : session.LoadedSeqs())
            {
                if (seqName != GetMidiVars()->sSeqDataTable()[idx].field_0_mBsqName)
                {
                    continue;
                }

                session.Note("play " + seqName);
                SND_SEQ_Play(idx, 1, options.mSeqVol, options.mSeqVol);
                u32 played = 0;
                while (played < options.mSeqMs && SND_SsIsEos_DeInlined(idx))
                {
                    session.Advance(100);
                    played += 100;
                }
                session.Note("stop");
                SND_Stop_All_Seqs();
                SsUtAllKeyOff(0);
                session.Advance(1000);
            }
        });
    }
}

// Every file in baseline must be the same in out
static bool CompareWithBaseline(const fs::path& out, const fs::path& baseline)
{
    s32 compared = 0;
    s32 differ = 0;
    for (const auto& entry : fs::recursive_directory_iterator(baseline))
    {
        if (!entry.is_regular_file())
        {
            continue;
        }
        const fs::path relative = fs::relative(entry.path(), baseline);
        const fs::path other = out / relative;
        compared++;
        if (!fs::exists(other))
        {
            printf("MISSING %s\n", relative.string().c_str());
            differ++;
        }
        else if (ReadAll(entry.path()) != ReadAll(other))
        {
            printf("DIFFERS %s\n", relative.string().c_str());
            differ++;
        }
    }
    printf("%d of %d files differ from %s\n", differ, compared, baseline.string().c_str());
    return differ == 0 && compared > 0;
}

s32 main(s32 argc, char_type** argv)
{
    const CommandLineParser args(argc, argv);
    const std::optional<std::string> data = args.GetValue("-data");
    if (!data || args.HasSwitch("-help") || args.HasSwitch("--help") || args.HasSwitch("-h"))
    {
        printf("%s", kUsage);
        return data ? 0 : 1;
    }

    Options options;
    options.mGame = args.HasSwitch("-AO") ? GameType::eAo : GameType::eAe;
    options.mOut = fs::absolute(args.GetValue("-out").value_or("sound_gold_out"));
    options.mTheme = args.GetValue("-theme").value_or("");
    if (const std::optional<std::string> seqMs = args.GetValue("-seq_ms"))
    {
        options.mSeqMs = static_cast<u32>(std::stoul(*seqMs));
    }
    if (const std::optional<std::string> reverb = args.GetValue("-reverb"))
    {
        options.mReverb = std::stoi(*reverb);
    }
    options.mPs1Sound = args.HasSwitch("-ps1_sound");
    if (args.GetValue("-spu_filter").value_or("hq") == "gaussian")
    {
        options.mInterpolation = PsxSpu::Interpolation::Gaussian;
    }
    if (const std::optional<std::string> seqVol = args.GetValue("-seq_vol"))
    {
        options.mSeqVol = static_cast<s16>(std::stoi(*seqVol));
    }
    const std::optional<std::string> baseline = args.GetValue("-baseline");
    const fs::path baselineDir = baseline ? fs::absolute(*baseline) : fs::path();

    // The engine loads everything relative to the game dir
    fs::current_path(*data);
    SetGameType(options.mGame);

    const fs::path soundsDir = fs::path("relive_data") / (options.mGame == GameType::eAo ? "ao" : "ae") / "sounds";
    if (!fs::is_directory(soundsDir))
    {
        printf("No %s in %s - convert the game data first (run relive there once)\n", soundsDir.string().c_str(), data->c_str());
        return 1;
    }

    std::vector<std::string> themes;
    std::string monkTheme;
    for (const auto& entry : fs::directory_iterator(soundsDir))
    {
        if (entry.is_directory())
        {
            themes.push_back(entry.path().filename().string());
        }
    }
    std::sort(themes.begin(), themes.end());

    // The game has no theme for MONK.VH/VB yet (see sMonkVh_Vb), use the first theme that has it
    for (const std::string& theme : themes)
    {
        if (fs::exists(soundsDir / theme / "MONK.VH"))
        {
            monkTheme = theme;
            break;
        }
    }
    printf("MONK.VH/VB theme: %s\n", monkTheme.empty() ? "<none>" : monkTheme.c_str());

    FileSystem fs;
    ResourceManagerWrapper resMan(fs, "");
    for (const std::string& theme : themes)
    {
        if (options.mTheme.empty() || options.mTheme == theme)
        {
            RenderTheme(options, resMan, theme, monkTheme);
        }
    }

    if (baseline)
    {
        return CompareWithBaseline(options.mOut, baselineDir) ? 0 : 1;
    }
    return 0;
}
