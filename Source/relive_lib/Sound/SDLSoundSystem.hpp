#pragma once

#include "Sound.hpp"
#include "SoundSDL.hpp"
#include "PsxSoundEngine.hpp"
#include <memory>
#include <thread>
#include "SDL3/SDL.h"

#define CI_DISABLE_ASSERTS
#include <cinder/audio/dsp/RingBuffer.h>

enum AudioFilterMode
{
    NoFilter = 0,
    Linear = 1,
};

// An SDL implementation of used IDirectSound API's
class SDLSoundSystem final
{
public:
    void Init(u32 sampleRate, s32 bitsPerSample, s32 isStereo);

    // No audio device or render thread: audio is only made by RenderOffline, and the sound clock
    // (SND_GetTicks) is the length of the audio rendered so far. Makes the sound code
    // deterministic, for the sound gold tests and tools.
    void InitOffline();
    bool IsOffline() const { return mOffline; }
    u32 OfflineTicks() const;
    // Mixes sampleCount more stereo samples onto the end of OfflineOutput()
    void RenderOffline(u32 sampleCount);
    // Renders until OfflineTicks() reaches ticks
    void RenderOfflineUntil(u32 ticks);
    std::vector<StereoSample_S16>& OfflineOutput() { return mOfflineOutput; }

    // -ps1_sound: sound systems created after this play the VAB tones on the emulated PS1 SPU
    // (PsxSoundEngine) instead of the SDL voices
    // interpolation: how the SPU resamples voices (-spu_filter)
    static void SetPs1SoundOnCreate(bool enable, PsxSpu::Interpolation interpolation = PsxSpu::Interpolation::BandLimited);
    // nullptr when the PS1 sound is off
    PsxSoundEngine* Ps1Sound() { return mPs1Sound.get(); }

    void Pause();
    void Resume();
    u64 GetGeneratedAudioSamples() const;
    u32 GetDeviceSampleRate() const { return static_cast<u32>(mAudioDeviceSpec.freq); }

    HRESULT DuplicateSoundBuffer(TSoundBufferType* pDSBufferOriginal, TSoundBufferType** ppDSBufferDuplicate);

    HRESULT CreateSoundBuffer(LPCDSBUFFERDESC pcDSBufferDesc, TSoundBufferType** ppDSBuffer, void* /*pUnkOuter*/);

    HRESULT Release();

    // Called by audio thread - time critical
    static void AudioCallBackStatic(void* userdata, SDL_AudioStream* stream, s32 additionalAmount, s32 totalAmount);

private:
    ~SDLSoundSystem();

    void AudioCallBack(SDL_AudioStream* stream, s32 additionalAmount);

    // The part of Init that doesn't need an audio device
    void InitMixer();

    void RenderAudioThread();


    void RenderAudio(StereoSample_S16* pSampleBuffer, s32 sampleBufferCount);

    void RenderSoundBuffer(SDLSoundBuffer& entry, StereoSample_S16* pSampleBuffer, s32 sampleBufferCount);

    void RenderMonoSample(Sint16* pVoiceBufferPtr, SDLSoundBuffer* pVoice, s32 i);

    void RenderStereoSample(Sint16* pVoiceBufferPtr, SDLSoundBuffer* pVoice, s32 i);

private:
    SDL_AudioSpec mAudioDeviceSpec = {};
    SDL_AudioStream* mAudioStream = nullptr;
    SDL_AudioDeviceID mAudioDevice = 0;
    static constexpr f32 kMixVolume = 1.0f;

    s32 mCurrentSoundBufferSize = 0;
    AudioFilterMode mAudioFilterMode = AudioFilterMode::Linear;
    std::vector<StereoSample_S16> mTempSoundBuffer;
    std::vector<StereoSample_S16> mNoReverbBuffer;
    cinder::audio::dsp::RingBufferT<StereoSample_S16> mAudioRingBuffer;
    std::atomic_bool mRenderAudioThreadQuit{false};
    std::atomic<u64> mGeneratedAudioSamples{0};
    std::unique_ptr<std::thread> mRenderAudioThread;


    std::unique_ptr<PsxSoundEngine> mPs1Sound;

    bool mCreated = false;
    bool mOffline = false;
    std::vector<StereoSample_S16> mOfflineOutput;
};
