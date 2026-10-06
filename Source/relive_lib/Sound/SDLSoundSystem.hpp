#pragma once

#include "AudioStream.hpp"
#include "PsxSoundEngine.hpp"
#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include "SDL3/SDL.h"

#define CI_DISABLE_ASSERTS
#include <cinder/audio/dsp/RingBuffer.h>

// The sound output: the SDL audio device, and the render thread that mixes the PS1 sound engine
// and the audio streams (movies) into it. Also the sound code's clock.
class SDLSoundSystem final
{
public:
    struct Options final
    {
        // How the SPU resamples voices (-spu_filter)
        PsxSpu::Interpolation mInterpolation = PsxSpu::Interpolation::BandLimited;
        // No audio device or render thread: audio is only made by RenderOffline, and the sound
        // clock (GetTicks) is the length of the audio rendered so far. Makes the sound code
        // deterministic, for the sound gold tests and tools.
        bool mOffline = false;
        // -dump_audio: everything rendered is written to this WAV file (empty: no dump). The dump
        // is made offline, one frame of audio a game frame, so it's locked to the game's time
        // whatever the audio driver does.
        std::string mDumpPath;
        // -dump_live_audio: the same, but with the audio device kept. mDumpPath wins.
        std::string mLiveDumpPath;
    };

    explicit SDLSoundSystem(const Options& options);
    ~SDLSoundSystem();

    SDLSoundSystem(const SDLSoundSystem&) = delete;
    SDLSoundSystem& operator=(const SDLSoundSystem&) = delete;

    // The running sound system, nullptr when there is none
    static SDLSoundSystem* Get();

    // The sound code's clock in ms: SYS_GetTicks, or the length of the audio rendered so far when
    // the sound system is offline
    static u32 GetTicks();
    // Busy waits for GetTicks to reach ticks. Offline, it renders audio until then instead.
    static void WaitUntilTicks(u32 ticks);
    // -dump_audio: renders one frame (1/30 s) of audio into the dump. The dumping sound system has no
    // audio device, so this is what moves its clock on: called once a frame by PSX_VSync.
    static void DumpFrame();

    PsxSoundEngine& Engine()
    {
        return *mPs1Sound;
    }

    // A stream the movie code writes to. The sound system mixes it until it is stopped.
    std::shared_ptr<AudioStream> CreateStream(u32 sampleRate, u32 channels, u32 capacityFrames);

    u32 OfflineTicks() const;
    // Mixes sampleCount more stereo samples onto the end of OfflineOutput()
    void RenderOffline(u32 sampleCount);
    // Renders until OfflineTicks() reaches ticks
    void RenderOfflineUntil(u32 ticks);
    std::vector<StereoSample_S16>& OfflineOutput()
    {
        return mOfflineOutput;
    }

    void Pause();
    void Resume();
    // Samples the device has taken (offline: rendered), at the device's rate
    u64 GetGeneratedAudioSamples() const;
    // Mixed but not yet taken by the device (0 offline): a sound started now reaches the device
    // after this many samples
    u32 GetQueuedAudioSamples() const;
    // The output rate GetGeneratedAudioSamples counts at: everything is resampled to it, so
    // callers converting that sample count to time divide by this, not by a stream's own rate.
    u32 GetDeviceSampleRate() const
    {
        return static_cast<u32>(mAudioDeviceSpec.freq);
    }

private:
    // Called by the audio thread - time critical
    static void AudioCallBackStatic(void* userdata, SDL_AudioStream* stream, s32 additionalAmount, s32 totalAmount);
    void AudioCallBack(SDL_AudioStream* stream, s32 additionalAmount);

    void RenderAudioThread();
    void RenderAudio(StereoSample_S16* pSampleBuffer, s32 sampleBufferCount);
    void RenderDumpFrame();

    void OpenAudioDump(const std::string& path);
    void CloseAudioDump();
    void WriteAudioDump(const StereoSample_S16* pSamples, s32 count);

    SDL_AudioSpec mAudioDeviceSpec = {};
    SDL_AudioStream* mAudioStream = nullptr;
    SDL_AudioDeviceID mAudioDevice = 0;

    cinder::audio::dsp::RingBufferT<StereoSample_S16> mAudioRingBuffer;
    std::atomic_bool mRenderAudioThreadQuit{false};
    std::atomic<u64> mGeneratedAudioSamples{0};
    std::unique_ptr<std::thread> mRenderAudioThread;
    // -dump_audio: the WAV being written, its header is finished by CloseAudioDump
    FILE* mAudioDump = nullptr;
    // With -dump_live_audio: <wav>.log, a line per render and per device callback with the
    // time, sizes and how much the ring buffer held, to see where the live path stalls
    FILE* mLiveLog = nullptr;
    std::vector<StereoSample_S16> mCallbackBuffer;
    std::vector<StereoSample_S16> mDumpFrameBuffer;
    u64 mAudioDumpFrames = 0;

    std::unique_ptr<PsxSoundEngine> mPs1Sound;

    std::mutex mStreamsMutex;
    std::vector<std::shared_ptr<AudioStream>> mStreams;

    bool mCreated = false;
    bool mOffline = false;
    std::vector<StereoSample_S16> mOfflineOutput;
};
