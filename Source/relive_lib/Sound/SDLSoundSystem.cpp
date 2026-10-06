#include "../stdafx.h"
#include "SDLSoundSystem.hpp"
#include "../Sys.hpp"
#include <algorithm>
#include <functional>

static SDLSoundSystem* sInstance = nullptr;

static void WriteU32(FILE* f, u32 v)
{
    fwrite(&v, sizeof(v), 1, f);
}

static void WriteU16(FILE* f, u16 v)
{
    fwrite(&v, sizeof(v), 1, f);
}

void SDLSoundSystem::OpenAudioDump(const std::string& path)
{
    if (path.empty())
    {
        return;
    }
    mAudioDump = fopen(path.c_str(), "wb");
    if (!mAudioDump)
    {
        LOG_ERROR("Can't write the audio dump %s", path.c_str());
        return;
    }
    LOG_INFO("Dumping the audio to %s", path.c_str());
    mAudioDumpFrames = 0;
    mLiveLog = fopen((path + ".log").c_str(), "w");
    if (mLiveLog)
    {
        fprintf(mLiveLog, "# ms event samples ringAvail ok|durationUs\n");
    }
    // A 16-bit stereo PCM header, the sizes are filled in by CloseAudioDump
    fwrite("RIFF", 1, 4, mAudioDump);
    WriteU32(mAudioDump, 0);
    fwrite("WAVEfmt ", 1, 8, mAudioDump);
    WriteU32(mAudioDump, 16);
    WriteU16(mAudioDump, 1);
    WriteU16(mAudioDump, 2);
    const u32 rate = static_cast<u32>(mAudioDeviceSpec.freq);
    WriteU32(mAudioDump, rate);
    WriteU32(mAudioDump, rate * sizeof(StereoSample_S16));
    WriteU16(mAudioDump, sizeof(StereoSample_S16));
    WriteU16(mAudioDump, 16);
    fwrite("data", 1, 4, mAudioDump);
    WriteU32(mAudioDump, 0);
}

void SDLSoundSystem::WriteAudioDump(const StereoSample_S16* pSamples, s32 count)
{
    if (mAudioDump && count > 0)
    {
        fwrite(pSamples, sizeof(StereoSample_S16), static_cast<size_t>(count), mAudioDump);
        mAudioDumpFrames += static_cast<u64>(count);
    }
}

void SDLSoundSystem::CloseAudioDump()
{
    if (!mAudioDump)
    {
        return;
    }
    const u32 dataBytes = static_cast<u32>(mAudioDumpFrames * sizeof(StereoSample_S16));
    fseek(mAudioDump, 4, SEEK_SET);
    WriteU32(mAudioDump, 36 + dataBytes);
    fseek(mAudioDump, 40, SEEK_SET);
    WriteU32(mAudioDump, dataBytes);
    fclose(mAudioDump);
    mAudioDump = nullptr;
    if (mLiveLog)
    {
        fclose(mLiveLog);
        mLiveLog = nullptr;
    }
    LOG_INFO("Audio dump finished: %llu frames", static_cast<unsigned long long>(mAudioDumpFrames));
}


SDLSoundSystem::SDLSoundSystem(const Options& options)
{
    sInstance = this;

    // A dump to make offline has no audio device: the audio is rendered a frame at a time by DumpFrame
    const bool offlineDump = !options.mDumpPath.empty();
    const std::string dumpPath = offlineDump ? options.mDumpPath : options.mLiveDumpPath;
    mOffline = options.mOffline || offlineDump;

    mAudioDeviceSpec.format = SDL_AUDIO_S16;
    mAudioDeviceSpec.channels = 2;
    mAudioDeviceSpec.freq = 44100;

    if (!mOffline)
    {
        if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
        {
            LOG_ERROR("SDL_Init(SDL_INIT_AUDIO) failed %s", SDL_GetError());
        }
        else
        {
            for (s32 i = 0; i < SDL_GetNumAudioDrivers(); i++)
            {
                LOG_INFO("SDL Audio Driver %d %s", i, SDL_GetAudioDriver(i));
            }

            mAudioStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &mAudioDeviceSpec, SDLSoundSystem::AudioCallBackStatic, this);
            if (!mAudioStream)
            {
                LOG_ERROR("Couldn't open SDL audio: %s", SDL_GetError());
            }
        }
    }
    else if (offlineDump)
    {
        LOG_INFO("-dump_audio: rendering the audio offline, a frame at a time");
    }

    if (mAudioStream)
    {
        mAudioDevice = SDL_GetAudioStreamDevice(mAudioStream);

        LOG_INFO("-----------------------------");
        LOG_INFO("Audio Device opened, got specs:");
        LOG_INFO("Channels: %d nFormat: %d nFreq: %d", static_cast<s32>(mAudioDeviceSpec.channels), mAudioDeviceSpec.format, mAudioDeviceSpec.freq);
        LOG_INFO("Driver: %s", SDL_GetCurrentAudioDriver());
        LOG_INFO("-----------------------------");
    }

    mPs1Sound = std::make_unique<PsxSoundEngine>();
    mPs1Sound->SetInterpolation(options.mInterpolation);
    OpenAudioDump(dumpPath);
    mCreated = true;

    if (!mOffline)
    {
        // What the mixer may run ahead of the device: this much latency on every sound, against the
        // slack to absorb a slow chunk of mixing (a frame of 24 voices takes up to ~0.9x real time
        // in a Debug build). Sized on the main thread before any other threads start.
        mAudioRingBuffer.resize(2048);

        // The mix is made on its own thread, ahead of the device's callback, so a slow frame of mixing
        // is absorbed by the ring buffer instead of underrunning the device. Without a device (it
        // couldn't be opened) the thread plays the part of the device too, so the sound code's clock
        // and sequencer still run.
        mRenderAudioThread = std::make_unique<std::thread>(std::bind(&SDLSoundSystem::RenderAudioThread, this));

        if (mAudioDevice)
        {
            SDL_ResumeAudioDevice(mAudioDevice);
        }
    }
}

SDLSoundSystem::~SDLSoundSystem()
{
    TRACE_ENTRYEXIT;

    if (sInstance == this)
    {
        sInstance = nullptr;
    }

    if (!mOffline)
    {
        // Stop the audio call back
        if (mAudioStream)
        {
            SDL_PauseAudioStreamDevice(mAudioStream);
        }

        // Stop audio rendering thread
        mRenderAudioThreadQuit = true;
        if (mRenderAudioThread && mRenderAudioThread->joinable())
        {
            mRenderAudioThread->join();
        }
    }

    if (mAudioStream)
    {
        SDL_DestroyAudioStream(mAudioStream);
        mAudioStream = nullptr;
        mAudioDevice = 0;
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    }

    CloseAudioDump();
    mPs1Sound.reset();
}

SDLSoundSystem* SDLSoundSystem::Get()
{
    return sInstance;
}

u32 SDLSoundSystem::GetTicks()
{
    if (sInstance && sInstance->mOffline)
    {
        return sInstance->OfflineTicks();
    }
    return SYS_GetTicks();
}

void SDLSoundSystem::WaitUntilTicks(u32 ticks)
{
    if (sInstance && sInstance->mOffline)
    {
        sInstance->RenderOfflineUntil(ticks);
        return;
    }

    while (SYS_GetTicks() < ticks)
    {
    }
}

void SDLSoundSystem::DumpFrame()
{
    if (sInstance)
    {
        sInstance->RenderDumpFrame();
    }
}

std::shared_ptr<AudioStream> SDLSoundSystem::CreateStream(u32 sampleRate, u32 channels, u32 capacityFrames)
{
    auto pStream = std::make_shared<AudioStream>(sampleRate, channels, capacityFrames, GetDeviceSampleRate());
    std::lock_guard<std::mutex> lock(mStreamsMutex);
    mStreams.push_back(pStream);
    return pStream;
}

u32 SDLSoundSystem::OfflineTicks() const
{
    return static_cast<u32>(GetGeneratedAudioSamples() * 1000 / static_cast<u64>(mAudioDeviceSpec.freq));
}

void SDLSoundSystem::RenderOffline(u32 sampleCount)
{
    const size_t start = mOfflineOutput.size();
    mOfflineOutput.resize(start + sampleCount);
    RenderAudio(mOfflineOutput.data() + start, static_cast<s32>(sampleCount));
    mGeneratedAudioSamples.fetch_add(sampleCount, std::memory_order_release);
}

void SDLSoundSystem::RenderDumpFrame()
{
    if (!mOffline || !mAudioDump)
    {
        return;
    }
    static constexpr u32 kGameFps = 30;
    const u32 sampleCount = static_cast<u32>(mAudioDeviceSpec.freq) / kGameFps;
    mDumpFrameBuffer.assign(sampleCount, StereoSample_S16{});
    const u64 startNs = SDL_GetTicksNS();
    // RenderAudio writes the dump
    RenderAudio(mDumpFrameBuffer.data(), static_cast<s32>(sampleCount));
    mGeneratedAudioSamples.fetch_add(sampleCount, std::memory_order_release);
    if (mLiveLog)
    {
        fprintf(mLiveLog, "%.3f frame %u 0 %llu\n", static_cast<f64>(startNs) / 1e6, sampleCount, static_cast<unsigned long long>((SDL_GetTicksNS() - startNs) / 1000));
    }
}

void SDLSoundSystem::RenderOfflineUntil(u32 ticks)
{
    // The first sample count whose tick is >= ticks
    const u64 targetSamples = (static_cast<u64>(ticks) * static_cast<u64>(mAudioDeviceSpec.freq) + 999) / 1000;
    const u64 generated = GetGeneratedAudioSamples();
    if (targetSamples > generated)
    {
        RenderOffline(static_cast<u32>(targetSamples - generated));
    }
}

void SDLSoundSystem::Pause()
{
    if (mAudioDevice)
    {
        SDL_PauseAudioDevice(mAudioDevice);
    }
}

void SDLSoundSystem::Resume()
{
    if (mAudioDevice)
    {
        SDL_ResumeAudioDevice(mAudioDevice);
    }
}

u64 SDLSoundSystem::GetGeneratedAudioSamples() const
{
    return mGeneratedAudioSamples.load(std::memory_order_acquire);
}

u32 SDLSoundSystem::GetQueuedAudioSamples() const
{
    return mOffline ? 0 : static_cast<u32>(mAudioRingBuffer.getAvailableRead());
}

void SDLSoundSystem::AudioCallBack(SDL_AudioStream* stream, s32 additionalAmount)
{
    if (additionalAmount <= 0)
    {
        return;
    }

    const s32 bufferLenSamples = additionalAmount / sizeof(StereoSample_S16);
    // No allocation on the device thread: reuse the buffer
    mCallbackBuffer.assign(static_cast<size_t>(bufferLenSamples), StereoSample_S16{});
    StereoSample_S16* const buffer = mCallbackBuffer.data();

    const s32 readAvilSamples = static_cast<s32>(mAudioRingBuffer.getAvailableRead());
    const bool ok = mAudioRingBuffer.read(buffer, bufferLenSamples);
    if (!ok)
    {
        // The render thread is behind: this period is silence
        LOG_WARNING("Audio buffer underflow: %d of %d samples", readAvilSamples, bufferLenSamples);
    }
    if (mLiveLog)
    {
        fprintf(mLiveLog, "%.3f callback %d %d %d\n", static_cast<f64>(SDL_GetTicksNS()) / 1e6, bufferLenSamples, readAvilSamples, ok ? 1 : 0);
    }

    SDL_PutAudioStreamData(stream, buffer, additionalAmount);
    mGeneratedAudioSamples.fetch_add(static_cast<u64>(bufferLenSamples), std::memory_order_release);
}

void SDLSoundSystem::RenderAudioThread()
{
    std::vector<StereoSample_S16> tmpBuffer;
    while (!mRenderAudioThreadQuit)
    {
        if (!mAudioStream)
        {
            // No device: render 10 ms in real time and drop it
            tmpBuffer.assign(static_cast<size_t>(mAudioDeviceSpec.freq) / 100, StereoSample_S16{});
            RenderAudio(tmpBuffer.data(), static_cast<s32>(tmpBuffer.size()));
            mGeneratedAudioSamples.fetch_add(tmpBuffer.size(), std::memory_order_release);
            SDL_Delay(10);
            continue;
        }

        const size_t bufferSize = mAudioRingBuffer.getAvailableWrite();
        if (bufferSize > 0)
        {
            const u64 startNs = SDL_GetTicksNS();
            tmpBuffer.assign(bufferSize, StereoSample_S16{});
            RenderAudio(tmpBuffer.data(), static_cast<s32>(tmpBuffer.size()));
            if (!mAudioRingBuffer.write(tmpBuffer.data(), tmpBuffer.size()))
            {
                // Couldn't write all the data, should never happen ??
                LOG_ERROR("Ring buffer write failed");
            }
            if (mLiveLog)
            {
                fprintf(mLiveLog, "%.3f render %u %u %llu\n", static_cast<f64>(startNs) / 1e6, static_cast<u32>(bufferSize), static_cast<u32>(mAudioRingBuffer.getAvailableRead()), static_cast<unsigned long long>((SDL_GetTicksNS() - startNs) / 1000));
            }
        }
        else
        {
            // The buffer is full: wait for the device to take some rather than spinning on a core
            SDL_DelayNS(500 * 1000);
        }
    }
}

void SDLSoundSystem::RenderAudio(StereoSample_S16* pSampleBuffer, s32 sampleBufferCount)
{
    {
        std::lock_guard<std::mutex> lock(mStreamsMutex);
        for (const auto& pStream : mStreams)
        {
            pStream->Mix(pSampleBuffer, static_cast<u32>(sampleBufferCount));
        }
        mStreams.erase(std::remove_if(mStreams.begin(), mStreams.end(), [](const std::shared_ptr<AudioStream>& pStream)
                                      {
                                          return pStream->IsFinished();
                                      }),
                       mStreams.end());
    }

    mPs1Sound->Mix(pSampleBuffer, static_cast<u32>(sampleBufferCount));

    WriteAudioDump(pSampleBuffer, sampleBufferCount);
}

void SDLSoundSystem::AudioCallBackStatic(void* userdata, SDL_AudioStream* stream, s32 additionalAmount, s32 /*totalAmount*/)
{
    static_cast<SDLSoundSystem*>(userdata)->AudioCallBack(stream, additionalAmount);
}
