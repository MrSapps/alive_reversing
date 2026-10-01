#pragma once

#include "Spu/PsxSpu.hpp"
#include "SoundSDL.hpp"
#include "../GameType.hpp"
#include <array>
#include <mutex>
#include <vector>

struct VabHeader;

// The PS1 sound output (-ps1_sound): VAB tones played on the emulated SPU, the way libsnd does it,
// instead of on the SDL voices.
//
// The libsnd code (PsxSpuApi.cpp and each game's SEQ parser) still does the MIDI channel
// allocation and the SEQ timing. Where it would start, stop or re-pitch an SDL voice, it calls this
// instead. SPU voice n is MIDI channel n. What this adds over the SDL voices: the tone's raw ADSR
// registers stepped per sample, key off starting the SPU release, the tone's pan, the program
// and VAB volume and pan, the SPU's gaussian interpolation, the pitch bend range and the SPU
// reverb (tone mode bit 2 + the path's reverb depth).
//
// SDLSoundSystem owns it and mixes it into the SDL output. The game thread calls everything but
// Mix, which the audio thread calls; a mutex serialises them, and is only held briefly.
class PsxSoundEngine final
{
public:
    PsxSoundEngine();

    // The engine of the running sound system, or nullptr when -ps1_sound is off
    static PsxSoundEngine* Get();

    // The PC data lowered some of AO's tone priorities and volumes: the secret area jingle's from 127
    // to 70, so sound effects (an electric wall's buzz) took its voices. Puts the PS1 values back in
    // VH data, before SsVabOpenHead reads it. vhFile: the VH's name, e.g. RFSNDFX.VH. A tone only
    // changes when it is as the PC data has it.
    static void RestorePs1Tones(GameType game, const std::string& vhFile, std::vector<u8>& vhData);

    // Reads the VAB's programs and tones. Stops any voice playing the VAB's old samples.
    void OpenVab(s32 vabId, const VabHeader& header);
    // Copies one of the VAB's samples, 16 bit mono PCM. loop: loop the whole sample.
    void SetSample(s32 vabId, s32 vagIdx, const s16* pPcm, u32 sampleCount, bool loop);
    // Stops the voices playing the VAB and frees its samples
    void CloseVab(s32 vabId);

    // Keys on the voice with the program's tone (0-15). note is the key << 8 plus the fine tune in
    // 1/256 semitones, as libsnd's note parameter. All volumes are 0-127.
    // A note of SEQ seq (0-31, as SsSeqOpen numbers them) on its MIDI channel (0-15), at the SEQ's
    // volume (SetSeqVolume) and the channel's (SetChannelVolume)
    void NoteOnSeq(s32 voice, s32 vabId, s32 program, s32 tone, s32 note, s32 velocity, s32 seq, s32 channel);
    // A sound effect (SsVoKeyOn): its left and right volume. squared: the game's libsnd squares a
    // sound effect's volume too (see LibsndVoiceVolume).
    void NoteOnSfx(s32 voice, s32 vabId, s32 program, s32 tone, s32 note, s32 volLeft, s32 volRight, bool squared);

    // libsnd's voice volume (the voice setup at 0x80076d94 in the AE PS1 executable), as the left
    // and right VOLL/VOLR registers (0-0x3FFF). All inputs are 0-127, the pans 64 = centre. A SEQ's
    // note is scaled by the SEQ volume. squared: the volume is then SQUARED, which AE's libsnd does
    // for SEQ notes only and AO's (0x8007c73c in its PS1 executable) for every voice.
    struct VoiceVolume final
    {
        s32 mLeft = 0;
        s32 mRight = 0;
    };
    static VoiceVolume LibsndVoiceVolume(bool seqNote, bool squared, s32 velocity, s32 vabVol, s32 progVol, s32 toneVol, s32 tonePan, s32 progPan, s32 channelPan, s32 seqVolLeft,
        s32 seqVolRight);
    // How libsnd's SsVoKeyOn (0x80076080) turns a sound effect's left and right volume into a
    // velocity and a channel pan
    static void SfxVelocityAndPan(s32 volLeft, s32 volRight, s32& velocity, s32& pan);
    // Starts the voice's release
    void KeyOff(s32 voice);
    // Silences the voice at once
    void Stop(s32 voice);
    // False once the voice has released, or played a one shot sample to its end
    bool IsVoiceActive(s32 voice);

    // Re-pitches the voice by offset 1/128 semitones from the note it was keyed on with
    // (SsUtChangePitch)
    void SetPitchOffset(s32 voice, s32 offset);
    // libsnd's SEQ pitch bend for every voice playing the program: bend is the MIDI bend's high 7
    // bits (0-127, 64 = none; libsnd ignores the low 7), scaled by each tone's bend range (VagAtr
    // pbmin/pbmax, in semitones)
    void PitchBend(s32 program, s32 bend);

    // SsSeqOpen: the SEQ and each of its channels start at volume 127
    void OpenSeq(s32 seq);
    // SsSeqStop sets the SEQ's channel volumes back to 127 (0x8007885c in the AO PS1 executable).
    // Looping at the end of the SEQ keeps them.
    void ResetChannelVolumes(s32 seq);
    // SsSeqSetVol, 0-127: libsnd keeps the values as they are (127 from SsSeqOpen), and changes
    // the volume of the SEQ's playing notes too
    void SetSeqVolume(s32 seq, s32 left, s32 right);
    // A SEQ's channel volume (MIDI CC 7), 0-127, for the channel's later notes. Like libsnd
    // (_SsContMainVol, 0x80076058 in the AO PS1 executable) it also changes the SEQ's playing notes
    // of the channel's program, whatever channel played them.
    void SetChannelVolume(s32 seq, s32 channel, s32 vabId, s32 program, s32 volume);

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

    // A voice's registers and envelope phase, for the sound gold traces
    struct VoiceState final
    {
        PsxSpuEnvelope::Phase mPhase = PsxSpuEnvelope::Phase::Off;
        u16 mPitch = 0;
        s16 mVolLeft = 0;
        s16 mVolRight = 0;
        u16 mAdsr1 = 0;
        u16 mAdsr2 = 0;
        bool mReverb = false;
    };
    VoiceState GetVoiceState(s32 voice);

    // Audio thread: renders frames 44100 Hz stereo samples and adds them to out, saturating
    void Mix(StereoSample_S16* pOut, u32 frames);

private:
    static constexpr s32 kMaxVabs = 4;
    static constexpr s32 kMaxPrograms = 128;
    static constexpr s32 kTonesPerProgram = 16;
    static constexpr s32 kMaxSamples = 256;
    static constexpr s32 kMaxSeqs = 32;

    // The VagAtr fields the SPU uses
    struct Tone final
    {
        u8 mVol = 0; // 0 = no tone
        u8 mPan = 64;
        u8 mMode = 0;
        u8 mCentre = 60;
        u8 mShift = 0;
        u8 mBendMin = 0;
        u8 mBendMax = 0;
        u16 mAdsr1 = 0;
        u16 mAdsr2 = 0;
        s16 mVag = -1;
    };

    struct Program final
    {
        u8 mVol = 127;
        u8 mPan = 64;
    };

    struct Vab final
    {
        u8 mMasterVol = 127;
        u8 mMasterPan = 64;
        std::array<Program, kMaxPrograms> mPrograms = {};
        std::vector<Tone> mTones; // kMaxPrograms * kTonesPerProgram
        std::array<std::vector<s16>, kMaxSamples> mSamples;
        std::array<bool, kMaxSamples> mLoops = {};
    };

    // What the voice was keyed on with
    struct VoiceInfo final
    {
        s32 mVabId = -1;
        s32 mProgram = -1;
        s32 mNote128 = 0; // In 1/128 semitones
        u8 mCentre = 60;
        u8 mShift = 0;
        u8 mBendMin = 0;
        u8 mBendMax = 0;
        VoiceState mState; // mPhase isn't kept up to date

        // What its volume came from, so SetSeqVolume can work it out again
        s32 mSeq = -1; // -1: a sound effect
        bool mSquared = false;
        s32 mVelocity = 0;
        s32 mChannelVolume = 127; // A SEQ note's, scales the velocity
        s32 mChannelPan = 64;
        u8 mVabVol = 0;
        u8 mProgVol = 0;
        u8 mToneVol = 0;
        u8 mTonePan = 64;
        u8 mProgPan = 64;
    };

    static bool ValidVab(s32 vabId)
    {
        return vabId >= 0 && vabId < kMaxVabs;
    }

    static bool ValidVoice(s32 voice)
    {
        return voice >= 0 && voice < PsxSpu::kNumVoices;
    }

    // seq -1 is a sound effect
    void NoteOn(s32 voice, s32 vabId, s32 program, s32 tone, s32 note, s32 seq, bool squared, s32 velocity, s32 channelVolume, s32 channelPan);
    // Sets the voice's VOLL/VOLR from info again
    void UpdateVoiceVolume(s32 voice);
    VoiceVolume CurrentVolume(const VoiceInfo& info) const;
    void StopVabVoices(s32 vabId);
    void SetVoicePitch(s32 voice, s32 note128);

    std::mutex mMutex;
    PsxSpu mSpu;
    std::array<Vab, kMaxVabs> mVabs;
    std::array<VoiceInfo, PsxSpu::kNumVoices> mVoices;
    // SsSeqSetVol's left/right volume of each SEQ
    std::array<std::array<u8, 2>, kMaxSeqs> mSeqVolumes = {};
    static constexpr s32 kMidiChannels = 16;
    // Each SEQ's channel volumes (CC 7)
    std::array<std::array<u8, kMidiChannels>, kMaxSeqs> mChannelVolumes = {};
    std::vector<s16> mRenderBuffer;
};
