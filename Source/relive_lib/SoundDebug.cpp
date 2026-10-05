#include "SoundDebug.hpp"
#include "GameType.hpp"
#include "Sfx.hpp"
#include "GameObjects/BaseGameObject.hpp"
#include "BaseMap.hpp"
#include "Sys.hpp"
#include "Sound/Midi.hpp"
#include "../AliveLibAE/Abe.hpp"
#include "../AliveLibAE/MusicController.hpp"
#include "../AliveLibAO/Abe.hpp"
#include "../AliveLibAO/MusicController.hpp"
#include <imgui.h>
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

static const std::vector<SoundDebug::MusicName> kAeMusic = {
    {"none", 0},
    {"type1", 1},
    {"chime", 2},
    {"drum_ambience", 3},
    {"tension", 4},
    {"slog_tension", 5},
    {"slog_chase_tension", 6},
    {"intense_chase", 7},
    {"soft_chase", 8},
    {"possessed", 9},
    {"death_drum_short", 10},
    {"death_long", 11},
    {"secret_area_short", 12},
    {"secret_area_long", 13},
};

static const std::vector<SoundDebug::MusicName> kAoMusic = {
    {"none", 0},
    {"abe_on_elum", 1},
    {"chime", 2},
    {"drum_ambience", 3},
    {"chase", 4},
    {"slog_chase", 5},
    {"possessed", 6},
    {"slog_tension", 7},
    {"intense_chase", 8},
    {"slog_chase_tension", 9},
    {"type10", 10},
    {"type11", 11},
    {"type12", 12},
    {"death_drum_short", 13},
    {"death_long", 14},
    {"secret_area_short", 15},
    {"secret_area_long", 16},
};

const std::vector<SoundDebug::MusicName>& SoundDebug::MusicNames()
{
    return GetGameType() == GameType::eAo ? kAoMusic : kAeMusic;
}

static bool ParseNumber(const std::string& text, s32& value)
{
    try
    {
        std::size_t used = 0;
        value = std::stoi(text, &used, 0);
        return used == text.size();
    }
    catch (const std::exception&)
    {
        return false;
    }
}

static bool ParseMusic(const std::string& text, s32& value)
{
    for (const SoundDebug::MusicName& name : SoundDebug::MusicNames())
    {
        if (text == name.mName)
        {
            value = name.mValue;
            return true;
        }
    }
    s32 number = 0;
    if (ParseNumber(text, number) && number >= 0 && number <= SoundDebug::MusicNames().back().mValue)
    {
        value = number;
        return true;
    }
    return false;
}

// A game object that does nothing, to ask for music as a second owner
class SoundDebugOwner final : public BaseGameObject
{
public:
    SoundDebugOwner(ResourceManagerWrapper& resMan, BaseMap& map)
        : BaseGameObject(true, 0, resMan, map)
    {
        SetSurviveDeathReset(true);
    }
};

static BaseGameObject* sOtherOwner = nullptr;

void SoundDebug::ForgetOwners()
{
    sOtherOwner = nullptr;
}

static s32 SfxCount()
{
    return static_cast<s32>(relive::SoundEffects::SligBleh) + 1;
}

bool SoundDebug::IsSoundCommand(const std::string& name)
{
    return name == "music" || name == "kill_other" || name == "sfx" || name == "sfx_pitch" || name == "seq_play" || name == "seq_stop";
}

bool SoundDebug::Check(const std::string& name, const std::vector<std::string>& args, std::string& error)
{
    s32 value = 0;
    auto number = [&](std::size_t i, s32 minimum, s32 maximum)
    {
        if (!ParseNumber(args[i], value) || value < minimum || value > maximum)
        {
            error = "'" + args[i] + "' isn't a number from " + std::to_string(minimum) + " to " + std::to_string(maximum);
            return false;
        }
        return true;
    };

    if (name == "kill_other")
    {
        if (!args.empty())
        {
            error = "expected: kill_other";
            return false;
        }
        return true;
    }
    if (name == "music")
    {
        if (args.empty() || args.size() > 4)
        {
            error = "expected: music <type> [<owner> [<flag4> [<flag20>]]]";
            return false;
        }
        if (args.size() >= 2 && args[1] != "abe" && args[1] != "other" && args[1] != "none")
        {
            error = "the owner is abe, other or none";
            return false;
        }
        if ((args.size() >= 3 && !number(2, 0, 1)) || (args.size() >= 4 && !number(3, 0, 1)))
        {
            return false;
        }
        // The game's not known when the script is parsed, so accept either game's names
        for (const auto* names : {&kAeMusic, &kAoMusic})
        {
            for (const MusicName& n : *names)
            {
                if (args[0] == n.mName)
                {
                    return true;
                }
            }
        }
        return number(0, 0, 16);
    }
    if (name == "sfx")
    {
        if (args.empty() || args.size() > 3)
        {
            error = "expected: sfx <index> [<volume> [<right volume>]]";
            return false;
        }
        return number(0, 0, SfxCount() - 1) && (args.size() < 2 || number(1, 0, 127)) && (args.size() < 3 || number(2, 0, 127));
    }
    if (name == "sfx_pitch")
    {
        if (args.size() != 3)
        {
            error = "expected: sfx_pitch <index> <volume> <pitch>";
            return false;
        }
        return number(0, 0, SfxCount() - 1) && number(1, 0, 127) && number(2, -32000, 32000);
    }
    if (name == "seq_play")
    {
        if (args.empty() || args.size() > 3)
        {
            error = "expected: seq_play <index> [<volume> [<repeat>]]";
            return false;
        }
        return number(0, 0, 255) && (args.size() < 2 || number(1, 0, 127)) && (args.size() < 3 || number(2, 0, 1000));
    }
    if (name == "seq_stop")
    {
        if (args.size() != 1)
        {
            error = "expected: seq_stop <index>";
            return false;
        }
        return number(0, 0, 255);
    }
    error = "unknown sound command";
    return false;
}

static bool PlayMusic(s32 type, const std::string& owner, s16 flag4, s16 flag20, std::string& error, BaseMap* pMap, ResourceManagerWrapper* pResMan)
{
    BaseGameObject* pOwner = nullptr;
    if (owner == "abe")
    {
        pOwner = GetGameType() == GameType::eAo ? static_cast<BaseGameObject*>(AO::gAbe) : static_cast<BaseGameObject*>(gAbe);
        if (!pOwner)
        {
            error = "no Abe yet";
            return false;
        }
    }
    else if (owner == "other")
    {
        if (!sOtherOwner && pMap && pResMan)
        {
            sOtherOwner = relive_new SoundDebugOwner(*pResMan, *pMap);
        }
        pOwner = sOtherOwner;
        if (!pOwner)
        {
            error = "no second owner";
            return false;
        }
    }

    if (GetGameType() == GameType::eAo)
    {
        AO::MusicController::static_PlayMusic(static_cast<AO::MusicController::MusicTypes>(type), pOwner, flag4, flag20);
    }
    else
    {
        MusicController::static_PlayMusic(static_cast<MusicController::MusicTypes>(type), pOwner, flag4, static_cast<s8>(flag20));
    }
    return true;
}

bool SoundDebug::Run(const std::string& name, const std::vector<std::string>& args, std::string& error, BaseMap& map, ResourceManagerWrapper& resMan)
{
    s32 value = 0;
    if (name == "kill_other")
    {
        if (sOtherOwner)
        {
            sOtherOwner->SetDead(true);
            sOtherOwner = nullptr;
        }
        return true;
    }
    if (name == "music")
    {
        if (!ParseMusic(args[0], value))
        {
            error = "'" + args[0] + "' isn't a music type of this game";
            return false;
        }
        s32 flag4 = 1;
        s32 flag20 = 0;
        if (args.size() >= 3)
        {
            ParseNumber(args[2], flag4);
        }
        if (args.size() >= 4)
        {
            ParseNumber(args[3], flag20);
        }
        return PlayMusic(value, args.size() >= 2 ? args[1] : "abe", static_cast<s16>(flag4), static_cast<s16>(flag20), error, &map, &resMan);
    }
    if (name == "sfx")
    {
        ParseNumber(args[0], value);
        s32 volume = 0;
        if (args.size() == 2)
        {
            ParseNumber(args[1], volume);
        }
        if (args.size() == 3)
        {
            s32 right = 0;
            ParseNumber(args[2], right);
            relive::SFX_Play_Stereo(static_cast<relive::SoundEffects>(value), volume, right);
        }
        else
        {
            relive::SfxPlayMono(static_cast<relive::SoundEffects>(value), volume);
        }
        return true;
    }
    if (name == "sfx_pitch")
    {
        s32 volume = 0;
        s32 pitch = 0;
        ParseNumber(args[0], value);
        ParseNumber(args[1], volume);
        ParseNumber(args[2], pitch);
        relive::SFX_Play_Pitch(static_cast<relive::SoundEffects>(value), volume, pitch);
        return true;
    }
    if (name == "seq_play")
    {
        s32 volume = 100;
        s32 repeat = 1;
        ParseNumber(args[0], value);
        if (args.size() > 1)
        {
            ParseNumber(args[1], volume);
        }
        if (args.size() > 2)
        {
            ParseNumber(args[2], repeat);
        }
        return SND_SEQ_Play(static_cast<u16>(value), static_cast<s16>(repeat), static_cast<s16>(volume), static_cast<s16>(volume)) != 0;
    }
    if (name == "seq_stop")
    {
        ParseNumber(args[0], value);
        SND_SEQ_Stop(static_cast<u16>(value));
        return true;
    }
    error = "unknown sound command";
    return false;
}

static bool sAlwaysShown = false;
static bool sScriptRunning = false;
static u32 sScriptFrame = 0;

static s32 sMusicScopeDepth = 0;

static void VLog(const char* fmt, va_list args)
{
    static FILE* sFile = nullptr;
    static bool sOpened = false;
    if (!sOpened)
    {
        sOpened = true;
        if (const char* path = getenv("RELIVE_MUSIC_LOG"))
        {
            sFile = fopen(path, "w");
        }
    }
    if (!sFile)
    {
        return;
    }
    fprintf(sFile, "%u ", SoundDebug::MusicTicks(SYS_GetTicks()));
    vfprintf(sFile, fmt, args);
    fputc('\n', sFile);
    fflush(sFile);
}

void SoundDebug::LogSeqCall(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    VLog(fmt, args);
    va_end(args);
}

void SoundDebug::LogSeqOutput(const char* fmt, ...)
{
    if (sMusicScopeDepth <= 0)
    {
        return;
    }
    va_list args;
    va_start(args, fmt);
    VLog(fmt, args);
    va_end(args);
}

SoundDebug::MusicScope::MusicScope()
{
    sMusicScopeDepth++;
}

SoundDebug::MusicScope::~MusicScope()
{
    sMusicScopeDepth--;
}

static u32 sScriptBaseTicks = 0;

void SoundDebug::SetScriptFrame(bool running, u32 frame)
{
    if (running && !sScriptRunning)
    {
        // Carry on from the real clock, so a time taken before the script (the controller's base
        // time) is never ahead of one taken after it
        sScriptBaseTicks = SYS_GetTicks();
    }
    sScriptRunning = running;
    sScriptFrame = frame;
}

u32 SoundDebug::MusicTicks(u32 realTicks)
{
    return sScriptRunning ? sScriptBaseTicks + (sScriptFrame * 100) / 3 : realTicks;
}

void SoundDebug::SetAlwaysShown(bool shown)
{
    sAlwaysShown = shown;
}

bool SoundDebug::AlwaysShown()
{
    return sAlwaysShown;
}

void SoundDebug::DrawWindow()
{
    static s32 sMusic = 0;
    static s32 sSfx = 0;
    static s32 sSfxVolume = 0;
    static s32 sSeq = 0;
    static s32 sSeqVolume = 100;
    static s32 sSeqRepeat = 1;
    static std::string sResult;

    if (!ImGui::Begin("Sound"))
    {
        ImGui::End();
        return;
    }

    auto report = [&](bool ok, const std::string& error)
    {
        sResult = ok ? "" : error;
    };

    const std::vector<MusicName>& music = MusicNames();
    ImGui::TextUnformatted("Music (the game's MusicController)");
    if (ImGui::BeginCombo("Type", music[static_cast<std::size_t>(sMusic)].mName))
    {
        for (std::size_t i = 0; i < music.size(); i++)
        {
            if (ImGui::Selectable(music[i].mName, static_cast<s32>(i) == sMusic))
            {
                sMusic = static_cast<s32>(i);
            }
        }
        ImGui::EndCombo();
    }
    if (ImGui::Button("Play music"))
    {
        std::string error;
        report(PlayMusic(music[static_cast<std::size_t>(sMusic)].mValue, "abe", 1, 0, error, nullptr, nullptr), error);
    }

    ImGui::Separator();
    ImGui::TextUnformatted("Sound effect (relive::SoundEffects)");
    ImGui::SliderInt("Index", &sSfx, 0, SfxCount() - 1);
    ImGui::SliderInt("Volume (0 = default)", &sSfxVolume, 0, 127);
    if (ImGui::Button("Play sfx"))
    {
        relive::SfxPlayMono(static_cast<relive::SoundEffects>(sSfx), sSfxVolume);
    }

    ImGui::Separator();
    ImGui::TextUnformatted("SEQ (the game's SEQ table index)");
    ImGui::SliderInt("SEQ", &sSeq, 0, 170);
    ImGui::SliderInt("SEQ volume", &sSeqVolume, 0, 127);
    ImGui::SliderInt("Repeat (0 = forever)", &sSeqRepeat, 0, 20);
    if (ImGui::Button("Play SEQ"))
    {
        report(SND_SEQ_Play(static_cast<u16>(sSeq), static_cast<s16>(sSeqRepeat), static_cast<s16>(sSeqVolume), static_cast<s16>(sSeqVolume)) != 0, "no such SEQ loaded");
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop SEQ"))
    {
        SND_SEQ_Stop(static_cast<u16>(sSeq));
    }

    if (!sResult.empty())
    {
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", sResult.c_str());
    }
    ImGui::End();
}

s32 SoundDebug::ObjectOrdinal(const void* p)
{
    static std::vector<const void*> sSeen;
    if (!p)
    {
        return 0;
    }
    auto it = std::find(sSeen.begin(), sSeen.end(), p);
    if (it == sSeen.end())
    {
        sSeen.push_back(p);
        return static_cast<s32>(sSeen.size());
    }
    return static_cast<s32>(it - sSeen.begin()) + 1;
}
