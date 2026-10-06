#pragma once

#include "Libsnd.hpp"
#include "Midi.hpp"
#include "SDLSoundSystem.hpp"
#include <memory>

struct OpenSeqHandle;
struct PathSoundInfo;
class ResourceManagerWrapper;

// The game side of the sound: the sound blocks (VAB tones and samples, SEQs) of the current path,
// the SEQs and sound effects the game plays, all on the PS1 sound engine (PsxSoundEngine) that the
// sound system (SDLSoundSystem) mixes. One for the running game, made by SND_Init.
//
// The game calls the SND_ and SFX_ functions in Midi.hpp, which are on the running GameSound.
class GameSound final
{
public:
    // SND_Init: starts the sound system with reverb on and the game's master volume
    explicit GameSound(const GameSoundConfig& config);
    ~GameSound();

    GameSound(const GameSound&) = delete;
    GameSound& operator=(const GameSound&) = delete;

    // nullptr when SND_Init hasn't been called, or after SND_SsQuit
    static GameSound* Get();

    SDLSoundSystem& System()
    {
        return *mSystem;
    }
    PsxSoundEngine& Engine()
    {
        return mSystem->Engine();
    }
    GameType Game() const
    {
        return mConfig.mGame;
    }

    // The game's SEQ table, set by LoadSeqs
    OpenSeqHandle* SeqTable()
    {
        return mSeqTable;
    }
    s32 SeqTableSize() const;

    // Pends the sound files a new path's LoadVabs and LoadSeqs will load
    void PendSoundFiles(const PathSoundInfo& info, ResourceManagerWrapper& resMan);
    // Loads the path's VAB, unless it is the one already loaded, and sets the reverb for it
    void LoadVabs(std::shared_ptr<PathSoundInfo>& info, s32 reverb, ResourceManagerWrapper& resMan);
    void LoadSeqs(OpenSeqHandle* pSeqTable, const PathSoundInfo& info, ResourceManagerWrapper& resMan);

    // Stops the SEQs, frees the VAB and the SEQ data and sets the master volume back
    void Reset();
    // Reset and close everything, before the sound system goes
    void Shutdown();

    void StopAll();
    void Restart(BaseMap& map);
    void StopAllSeqs();
    void StopChannelsMask(u32 bitMask);

    s16 SeqPlaySeq(u16 idx, s16 repeatCount, s16 bDontStop);
    s16 SeqPlay(u16 idx, s16 repeatCount, s16 volLeft, s16 volRight);
    void SeqStop(u16 idx);
    void SeqSetVol(s32 idx, s16 volLeft, s16 volRight);
    s32 SeqIsPlaying(u16 idx);

    s32 SfxPlayMono(const relive::SfxDefinition& sfxDef, s32 volume, s32 pitch_min, s32 pitch_max);
    s32 SfxPlayStereo(const relive::SfxDefinition& sfxDef, s16 volLeft, s16 volRight, s16 pitch_min, s16 pitch_max);
    s32 PlayMidi(s32 program, s32 vabId, s32 note, s16 vol, s16 min, s16 max);

    // The id of the VAB the last LoadVabs loaded. A path's sounds are only played after its sound block
    // has been loaded.
    s32 LastLoadedVabId() const;

    // Opens the VAB of a VH file's data and loads its samples, then names the VAB in info
    void LoadVab(PathSoundInfo& info, ResourceManagerWrapper& resMan);
    // SsVabClose: the voices and samples of the VAB
    void CloseVab(s32 vabId);

private:
    s32 PlayNote(s32 vabIdAndProgram, s32 pitch, s32 leftVol, s32 rightVol);
    void SetChannelsPitch(s32 channelsBits, s32 vabId, s32 program, s32 note, s16 pitch);
    s32 ApplyPitchVariation(const relive::SfxDefinition& sfxDef, s32 voices, s16 pitch_min, s16 pitch_max);
    // Opens the SEQ of table entry idx if it isn't, making room by closing the finished ones. False
    // if there is no room or the entry has no data
    bool OpenSeq(u16 idx);
    void StopFinishedSeqs();
    void FreeAllVabs();
    void FreeAllSeqs();
    void LoadVabBody(const PathSoundInfo& info, const std::vector<u8>& vbData, s32 vabId, ResourceManagerWrapper& resMan);
    void SetMasterVolume(s32 volume);

    // The games open at most this many SEQs at once
    static constexpr s32 kMaxPlayingSeqs = 16;
    static constexpr s32 kMaxVabs = 4;

    GameSoundConfig mConfig;
    std::unique_ptr<SDLSoundSystem> mSystem;
    OpenSeqHandle* mSeqTable = nullptr;
    bool mNeedToHashSeqNames = true;
    // The SEQ table index of each open SEQ, indexed by the number libsnd gave it. -1 for none.
    s16 mOpenSeqs[Libsnd::kMaxSeqs];
    s16 mSeqsPlayingCount = 0;
    std::weak_ptr<PathSoundInfo> mLastLoadedSoundBlockInfo;
    // Bit per VAB id that is open
    u32 mOpenVabs = 0;
};

