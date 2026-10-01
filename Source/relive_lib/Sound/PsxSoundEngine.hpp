#pragma once

#include "Spu/PsxSpu.hpp"
#include "Libsnd.hpp"
#include "SoundSDL.hpp"
#include "../GameType.hpp"
#include <array>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

// The PS1 sound output (-ps1_sound), the same for both games: the game's libsnd calls go to the
// libsnd port (Libsnd), which drives the emulated SPU (PsxSpu) playing the PC samples.
//
// Like the PS1, where the vsync interrupt ran libsnd's tick, libsnd ticks 50 times a second of
// audio (the PAL rate) between the SPU's samples: the audio thread does it in Mix. Each tick writes
// the registers the game's calls changed since the last one, reads the voices' envelopes for the
// voice allocation, and plays the SEQs. The game thread calls everything but Mix. A mutex
// serialises them, and is only held briefly.
//
// SDLSoundSystem owns it and mixes it into the SDL output.
class PsxSoundEngine final : private ILibsndSpu
{
public:
    PsxSoundEngine();
    ~PsxSoundEngine();

    // The engine of the running sound system, or nullptr when -ps1_sound is off
    static PsxSoundEngine* Get();

    // The PC data lowered some of AO's tone priorities and volumes: the secret area jingle's from 127
    // to 70, so sound effects (an electric wall's buzz) took its voices. Puts the PS1 values back in
    // VH data, before SsVabOpenHead reads it. vhFile: the VH's name, e.g. RFSNDFX.VH. A tone only
    // changes when it is as the PC data has it.
    static void RestorePs1Tones(GameType game, const std::string& vhFile, std::vector<u8>& vhData);

    // SsVabOpenHead: reads the VAB's programs and tones. Stops any voice playing the VAB's old
    // samples.
    void OpenVab(s32 vabId, const u8* pVh, u32 size);
    // Copies one of the VAB's samples, 16 bit mono PCM. vagIdx is the VAG number - 1. loop: loop
    // the whole sample.
    void SetSample(s32 vabId, s32 vagIdx, const s16* pPcm, u32 sampleCount, bool loop);
    // SsVabClose: stops the voices playing the VAB and frees its samples
    void CloseVab(s32 vabId);

    // libsnd, see Libsnd for each
    s16 SeqOpen(const u8* pSeq, u32 size, s16 vabId);
    void SeqClose(s16 seq);
    void SeqPlay(s16 seq, s8 mode, s16 count);
    void SeqStop(s16 seq);
    void SeqSetVol(s16 seq, s16 left, s16 right);
    s16 IsEos(s16 seq);
    // Unlike libsnd it returns 0, not -1, when there was no voice: the games take the result as a
    // voice mask, and would stop or re-pitch every voice with it
    s32 VoKeyOn(s32 vabProg, s32 pitch, u16 left, u16 right);
    void UtKeyOffV(s16 voice);
    void UtChangePitch(s16 voice, s16 vabId, s16 prog, s16 oldNote, s16 oldFine, s16 newNote, s16 newFine);
    void UtAllKeyOff();

    // Silences every voice at once (quitting)
    void StopAll();

    // How the SPU resamples the voices
    void SetInterpolation(PsxSpu::Interpolation interpolation);

    // SsSetMVol, 0-127
    void SetMasterVolume(s32 left, s32 right);
    // SsUtSetReverbType: the libsnd reverb type, 0-9
    void SetReverbType(s32 type);
    // SsUtSetReverbDepth, 0-127
    void SetReverbDepth(s32 left, s32 right);
    // SsUtReverbOn/Off
    void SetReverbEnabled(bool enable);
    // SpuClearReverbWorkArea
    void ClearReverb();

    // A voice's registers, envelope phase and what libsnd is playing on it, for the sound gold
    // traces
    struct VoiceState final
    {
        PsxSpuEnvelope::Phase mPhase = PsxSpuEnvelope::Phase::Off;
        u32 mPitch = 0;
        s16 mVolLeft = 0; // The VOLL/VOLR registers, 0x3FFF = full
        s16 mVolRight = 0;
        u16 mAdsr1 = 0;
        u16 mAdsr2 = 0;
        bool mReverb = false;
        Libsnd::VoiceInfo mLibsnd;
    };
    VoiceState GetVoiceState(s32 voice);

    // The sound call recorder (RELIVE_SOUND_RECORD=<file>): logs the libsnd calls the game makes,
    // with the sound clock in ms, one a line. relive_sound_gold -replay plays them on the engine,
    // and build-ps1re's ps1snd on the games' real libsnd (see SOUND_FORMATS.md).
    void Record(const char* fmt, ...);

    // Audio thread: renders frames 44100 Hz stereo samples and adds them to out, saturating
    void Mix(StereoSample_S16* pOut, u32 frames);

    // Audio samples a libsnd tick (the PAL vsync)
    static constexpr u32 kSamplesPerTick = PsxSpu::kSampleRate / 50;

private:
    static constexpr s32 kMaxVabs = 4;
    static constexpr s32 kMaxSamples = 256;

    struct Vab final
    {
        std::array<std::vector<s16>, kMaxSamples> mSamples;
        std::array<bool, kMaxSamples> mLoops = {};
        // Low-passed copies of the samples the tones play fast, see PsxSpuSample::mFiltered
        std::array<std::array<std::vector<s16>, kPsxSpuFilterLevels>, kMaxSamples> mFiltered;
    };

    static bool ValidVab(s32 vabId)
    {
        return vabId >= 0 && vabId < kMaxVabs;
    }

    // ILibsndSpu, called with mMutex held
    u16 VoiceEnvelope(s32 voice) override;
    void SetVoiceVolume(s32 voice, u16 left, u16 right) override;
    void SetVoicePitch(s32 voice, u32 pitch) override;
    void SetVoiceSample(s32 voice, s32 vabId, s32 vag) override;
    void SetVoiceAdsr(s32 voice, u16 adsr1, u16 adsr2) override;
    void KeyOff(u32 voices) override;
    void KeyOn(u32 voices) override;
    void SetReverbVoices(u32 voices) override;

    void StopVabVoices(s32 vabId);
    // Stops the voices playing, or about to play, this VAG of the VAB (its memory is going away)
    void StopVagVoices(s32 vabId, s32 vag);
    // Makes the low-passed copies the samples need for these pitches (maxPitch[vag]) that they
    // haven't got yet. Called without mMutex: it's slow, and the audio thread mustn't wait for it.
    void FilterSamples(s32 vabId, const std::array<u32, 256>& maxPitch);

    std::mutex mMutex;
    PsxSpu mSpu;
    Libsnd mLibsnd;
    std::array<Vab, kMaxVabs> mVabs;

    // What each voice's registers hold, for the traces, and which VAB its sample is from
    struct VoiceRegs final
    {
        u32 mPitch = 0;
        u16 mVolLeft = 0;
        u16 mVolRight = 0;
        u16 mAdsr1 = 0;
        u16 mAdsr2 = 0;
        bool mReverb = false;
        s32 mNextVabId = -1; // SSA: the sample the next key on plays
        s32 mNextVag = 0;    // Its VAG number (1 based), 0 for none
        s32 mPlayingVabId = -1;
        s32 mPlayingVag = 0;
    };
    std::array<VoiceRegs, PsxSpu::kNumVoices> mRegs;

    u32 mSamplesToTick = 0;
    std::vector<s16> mRenderBuffer;
    FILE* mRecordFile = nullptr;
};
