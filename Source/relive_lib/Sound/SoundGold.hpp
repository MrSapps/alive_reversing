#pragma once

#include "../../relive_lib/Types.hpp"
#include "../GameType.hpp"
#include "SoundSDL.hpp"
#include <memory>
#include <string>
#include <vector>

class FileSystem;
class ResourceManagerWrapper;
struct PathSoundInfo;
struct SoundApi;

// Runs the real sound code deterministically and records what it does, so a change to the sound
// code or its data formats can be checked against a "gold" recording made before the change.
//
// The sound system runs offline (see SDLSoundSystem::InitOffline): time only moves when Advance
// renders audio, so the same calls always give the same trace and audio. The trace lists the
// loaded tone table and a checksum of every sample, then every change to the 24 MIDI channels
// and the 32 voices, with the time it happened at.
//
// Used by the SoundGold unit tests (synthetic data) and relive_sound_gold (real game data).
class SoundGoldSession final
{
public:
    // Starts the sound system for game. Only one session may exist at a time.
    explicit SoundGoldSession(GameType game);
    ~SoundGoldSession();

    SoundGoldSession(const SoundGoldSession&) = delete;
    SoundGoldSession& operator=(const SoundGoldSession&) = delete;

    // Loads a sound block the way the game's map does (SND_Load_VABS + SND_Load_Seqs), then
    // records the tone tables and samples that got loaded. It also loads MONK.VH/VB from
    // monkTheme (the game hasn't got a theme for it yet, see sMonkVh_Vb), empty to skip it.
    void LoadSoundBlock(ResourceManagerWrapper& resMan, const std::shared_ptr<PathSoundInfo>& info, s32 reverb, const std::string& monkTheme);

    // Adds a line to the trace, e.g. to record which call the caller is about to make
    void Note(const std::string& text);

    // Moves time on by ms, in 10 ms steps. Each step runs the sequencer + ADSR update
    // (SsSeqCalledTbyT, as the game does once a frame), renders the step's audio and records
    // what changed.
    void Advance(u32 ms);

    // The sound block's VAB id, for SsVoKeyOn
    s32 VabId() const;

    // Every OpenSeqHandle index that has SEQ data loaded
    std::vector<u16> LoadedSeqs() const;

    // The program numbers that have at least one tone in the loaded VAB
    std::vector<s32> Programs() const;

    // Root key (centre) of the first tone of program, for playing a note at its natural pitch
    s32 RootKey(s32 program) const;

    const std::string& Trace() const
    {
        return mTrace;
    }

    const std::vector<StereoSample_S16>& Audio() const;

    // 44100 Hz stereo 16 bit WAV
    static std::vector<u8> ToWav(const std::vector<StereoSample_S16>& audio);

    static u32 Crc32(const u8* pData, size_t len);

private:
    void RecordLoadedVab(s32 vabId);
    void RecordChanges();

    GameType mGame;
    std::string mTrace;
    std::vector<std::string> mLastChannelState;
    std::vector<std::string> mLastVoiceState;
    std::shared_ptr<PathSoundInfo> mInfo;
    decltype(SoundApi::mSND_CreateDS) mOldCreateDS = nullptr;
};
