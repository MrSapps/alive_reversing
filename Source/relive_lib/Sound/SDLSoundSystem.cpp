#include "../stdafx.h"
#include "SDLSoundSystem.hpp"
#include "SDLSoundBuffer.hpp"
#include "Reverb.hpp"
#include <functional>
#include <cmath>

static bool sPs1SoundOnCreate = false;
static PsxSpu::Interpolation sPs1SoundInterpolation = PsxSpu::Interpolation::BandLimited;

void SDLSoundSystem::SetPs1SoundOnCreate(bool enable, PsxSpu::Interpolation interpolation)
{
    sPs1SoundOnCreate = enable;
    sPs1SoundInterpolation = interpolation;
}

static std::string sAudioDumpOnCreate;
static bool sAudioDumpOffline = true;

void SDLSoundSystem::SetAudioDumpOnCreate(const std::string& wavPath, const std::string& liveWavPath)
{
    sAudioDumpOnCreate = wavPath.empty() ? liveWavPath : wavPath;
    sAudioDumpOffline = !wavPath.empty();
}

static void WriteU32(FILE* f, u32 v)
{
    fwrite(&v, sizeof(v), 1, f);
}

static void WriteU16(FILE* f, u16 v)
{
    fwrite(&v, sizeof(v), 1, f);
}

void SDLSoundSystem::OpenAudioDump()
{
    if (sAudioDumpOnCreate.empty())
    {
        return;
    }
    mAudioDump = fopen(sAudioDumpOnCreate.c_str(), "wb");
    if (!mAudioDump)
    {
        LOG_ERROR("Can't write the audio dump %s", sAudioDumpOnCreate.c_str());
        return;
    }
    LOG_INFO("Dumping the audio to %s", sAudioDumpOnCreate.c_str());
    mAudioDumpFrames = 0;
    mLiveLog = fopen((sAudioDumpOnCreate + ".log").c_str(), "w");
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

void SDLSoundSystem::Init(u32 /*sampleRate*/, s32 /*bitsPerSample*/, s32 /*isStereo*/)
{
    mCreated = false;

    if (!sAudioDumpOnCreate.empty() && sAudioDumpOffline)
    {
        // No audio device: the audio is rendered a frame at a time by RenderDumpFrame
        LOG_INFO("-dump_audio: rendering the audio offline, a frame at a time");
        InitOffline();
        return;
    }

    if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
    {
        LOG_ERROR("SDL_Init(SDL_INIT_AUDIO) failed %s", SDL_GetError());
        return;
    }

    for (s32 i = 0; i < SDL_GetNumAudioDrivers(); i++)
    {
        LOG_INFO("SDL Audio Driver %d %s", i, SDL_GetAudioDriver(i));
    }

    mAudioDeviceSpec.format = SDL_AUDIO_S16;
    mAudioDeviceSpec.channels = 2;
    mAudioDeviceSpec.freq = 44100;

    mAudioStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &mAudioDeviceSpec, SDLSoundSystem::AudioCallBackStatic, this);
    if (!mAudioStream)
    {
        LOG_ERROR("Couldn't open SDL audio: %s", SDL_GetError());
        return;
    }

    mAudioDevice = SDL_GetAudioStreamDevice(mAudioStream);

    //SDL_SetAudioStreamGain(mAudioStream, 0.0f);

    LOG_INFO("-----------------------------");
    LOG_INFO("Audio Device opened, got specs:");
    LOG_INFO("Channels: %d nFormat: %d nFreq: %d", static_cast<s32>(mAudioDeviceSpec.channels), mAudioDeviceSpec.format, mAudioDeviceSpec.freq);
    LOG_INFO("Driver: %s", SDL_GetCurrentAudioDriver());
    LOG_INFO("-----------------------------");

    InitMixer();
    sLastNotePlayTime_BBC33C = SND_GetTicks();
    mCreated = true;

    // Correctly size the lock free buffer on the main thread before any other threads start
    // What the mixer may run ahead of the device: this much latency on every sound, against the
    // slack to absorb a slow chunk of mixing (a frame of 24 voices takes up to ~0.9x real time
    // in a Debug build)
    mAudioRingBuffer.resize(2048);

    // The mix is made on its own thread, ahead of the device's callback, so a slow frame of mixing
    // is absorbed by the ring buffer instead of underrunning the device
    mRenderAudioThread.reset(new std::thread(std::bind(&SDLSoundSystem::RenderAudioThread, this)));

    SDL_ResumeAudioDevice(mAudioDevice);
}

void SDLSoundSystem::InitMixer()
{
    if (sPs1SoundOnCreate)
    {
        LOG_INFO("PS1 sound: the VAB tones play on the emulated SPU");
        mPs1Sound = std::make_unique<PsxSoundEngine>();
        mPs1Sound->SetInterpolation(sPs1SoundInterpolation);
    }

    Reverb_Init(mAudioDeviceSpec.freq);
    OpenAudioDump();

    GetSoundAPI().SND_InitVolumeTable();

    if (sLoadedSoundsCount_BBC394)
    {
        for (s32 i = 0; i < 256; i++)
        {
            if (sSoundSamples_BBBF38[i])
            {
                GetSoundAPI().mSND_Renew(sSoundSamples_BBBF38[i]);
                GetSoundAPI().mSND_LoadSamples(sSoundSamples_BBBF38[i], 0, sSoundSamples_BBBF38[i]->field_8_pSoundBuffer, sSoundSamples_BBBF38[i]->field_C_buffer_size_bytes / (u8) sSoundSamples_BBBF38[i]->field_1D_blockAlign);
                if ((i + 1) == sLoadedSoundsCount_BBC394)
                    break;
            }
        }
    }
}

void SDLSoundSystem::InitOffline()
{
    mOffline = true;
    mAudioDeviceSpec.format = SDL_AUDIO_S16;
    mAudioDeviceSpec.channels = 2;
    mAudioDeviceSpec.freq = 44100;

    InitMixer();

    sLastNotePlayTime_BBC33C = SND_GetTicks();
    mCreated = true;
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


HRESULT SDLSoundSystem::DuplicateSoundBuffer(TSoundBufferType* pDSBufferOriginal, TSoundBufferType** ppDSBufferDuplicate)
{
    pDSBufferOriginal->Duplicate(ppDSBufferDuplicate);
    return S_OK;
}

HRESULT SDLSoundSystem::CreateSoundBuffer(LPCDSBUFFERDESC pcDSBufferDesc, TSoundBufferType** ppDSBuffer, void* /*pUnkOuter*/)
{
    *ppDSBuffer = new SDLSoundBuffer(*pcDSBufferDesc, mAudioDeviceSpec.freq);
    return S_OK;
}


HRESULT SDLSoundSystem::Release()
{
    TRACE_ENTRYEXIT;

    if (mOffline)
    {
        // There is no render thread to destroy the released voices, and SND_SsQuit has released
        // all of them by now
        for (s32 i = 0; i < MAX_VOICE_COUNT; i++)
        {
            SDLSoundBuffer* pVoice = sAE_ActiveVoices[i];
            if (pVoice)
            {
                pVoice->Destroy();
            }
        }
        delete this;
        return S_OK;
    }

    if (mCreated)
    {
        // Stop the audio call back
        SDL_PauseAudioStreamDevice(mAudioStream);

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
    }

    // Shutdown the sound system
    SDL_QuitSubSystem(SDL_INIT_AUDIO);

    delete this;
    return S_OK;
}


SDLSoundSystem::~SDLSoundSystem()
{
    TRACE_ENTRYEXIT;

    CloseAudioDump();
    Reverb_DeInit();

    // TODO: Clean up outstanding samples in sAE_ActiveVoices
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
        const size_t bufferSize = mAudioRingBuffer.getAvailableWrite();
        if (bufferSize > 0)
        {
            const u64 startNs = SDL_GetTicksNS();
            tmpBuffer.resize(bufferSize);
            memset(tmpBuffer.data(), 0, tmpBuffer.size() * sizeof(StereoSample_S16));
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
    // Check if our buffer size changes, and if its buffer, then resize the array
    if (sampleBufferCount > mCurrentSoundBufferSize)
    {
        mTempSoundBuffer.resize(sampleBufferCount);
        mNoReverbBuffer.resize(sampleBufferCount);
        mCurrentSoundBufferSize = sampleBufferCount;
    }

    if (mTempSoundBuffer.empty())
    {
        mTempSoundBuffer.resize(sampleBufferCount);
        mNoReverbBuffer.resize(sampleBufferCount);
    }

    memset(mNoReverbBuffer.data(), 0, sampleBufferCount * sizeof(StereoSample_S16));

    for (s32 vi = 0; vi < MAX_VOICE_COUNT; vi++)
    {
        SDLSoundBuffer* pVoice = sAE_ActiveVoices[vi];
        if (pVoice)
        {
            RenderSoundBuffer(*pVoice, pSampleBuffer, sampleBufferCount);
        }
    }

    // Do Reverb Pass
    if (gReverbEnabled)
    {
        Reverb_Mix(pSampleBuffer, SDL_AUDIO_S16, sampleBufferCount * sizeof(StereoSample_S16), kMixVolume);

        // Mix our no reverb buffer
        SDL_MixAudio(reinterpret_cast<Uint8*>(pSampleBuffer), reinterpret_cast<Uint8*>(mNoReverbBuffer.data()), SDL_AUDIO_S16, sampleBufferCount * sizeof(StereoSample_S16), kMixVolume);
    }

    // After the generic reverb: the SPU has its own
    if (mPs1Sound)
    {
        mPs1Sound->Mix(pSampleBuffer, static_cast<u32>(sampleBufferCount));
    }

    WriteAudioDump(pSampleBuffer, sampleBufferCount);
}


void SDLSoundSystem::RenderSoundBuffer(SDLSoundBuffer& entry, StereoSample_S16* pSampleBuffer, s32 sampleBufferCount)
{
    bool reverbPass = false;

    SDLSoundBuffer* pVoice = &entry;

    if (pVoice == nullptr || !pVoice->mBuffer || pVoice->mBuffer->empty())
    {
        return;
    }

    if (pVoice->mState.bIsReleased)
    {
        pVoice->Destroy(); // TODO: Still correct ??
        return;
    }

    // Clear Temp Sample Buffer
    memset(mTempSoundBuffer.data(), 0, sampleBufferCount * sizeof(StereoSample_S16));

    Sint16* pVoiceBufferPtr = reinterpret_cast<Sint16*>(pVoice->GetBuffer()->data());

    for (s32 i = 0; i < sampleBufferCount; i++)
    {
        if (pVoice->mBuffer->empty() || pVoice->mState.eStatus != SDLSoundBufferStatus::Playing || pVoice->mState.iSampleCount == 0)
        {
            break;
        }

        if (pVoice->mState.iVolume < pVoice->mState.iVolumeTarget)
        {
            pVoice->mState.iVolume++;
        }
        else if (pVoice->mState.iVolume > pVoice->mState.iVolumeTarget)
        {
            pVoice->mState.iVolume--;
        }

        if (pVoice->mState.iChannels == 2)
        {
            reverbPass = true; // Todo: determine this with flags in the sound object itself.
                               // For Stereo buffers. The only time this is played is for FMV's.
                               // Right now, unless the playback device is at 44100 hz, it sounds awful.
                               // TODO: Resampling for stereo

            RenderStereoSample(pVoiceBufferPtr, pVoice, i);
        }
        else
        {
            reverbPass = true;

            RenderMonoSample(pVoiceBufferPtr, pVoice, i);
        }

        if (pVoice->mState.fPlaybackPosition >= pVoice->mState.iSampleCount / pVoice->mState.iChannels)
        {
            pVoice->mState.fPlaybackPosition = 0;
            if (!pVoice->mState.bLoop)
            {
                pVoice->mState.eStatus = SDLSoundBufferStatus::Stopped;
            }
        }
    }

    if (reverbPass)
    {
        SDL_MixAudio(reinterpret_cast<Uint8*>(pSampleBuffer), reinterpret_cast<Uint8*>(mTempSoundBuffer.data()), SDL_AUDIO_S16, sampleBufferCount * sizeof(StereoSample_S16), 0.35f);
    }
    else
    {
        SDL_MixAudio(reinterpret_cast<Uint8*>(mNoReverbBuffer.data()), reinterpret_cast<Uint8*>(mTempSoundBuffer.data()), SDL_AUDIO_S16, sampleBufferCount * sizeof(StereoSample_S16), 0.35f);
    }
}


void SDLSoundSystem::RenderMonoSample(Sint16* pVoiceBufferPtr, SDLSoundBuffer* pVoice, s32 i)
{
    s32 s = 0;

    switch (mAudioFilterMode)
    {
        case AudioFilterMode::NoFilter:
            s = pVoiceBufferPtr[static_cast<s32>(pVoice->mState.fPlaybackPosition)];
            break;
        case AudioFilterMode::Linear:
            const s16 s1 = pVoiceBufferPtr[static_cast<s32>(pVoice->mState.fPlaybackPosition)];
            const s16 s2 = pVoiceBufferPtr[(static_cast<s32>(pVoice->mState.fPlaybackPosition) + 1) % pVoice->mState.iSampleCount];

            s = static_cast<s32>((s1 + ((s2 - s1) * (pVoice->mState.fPlaybackPosition - floorf(pVoice->mState.fPlaybackPosition)))));
            break;
    }

    s32 leftPan = 10000;
    s32 rightPan = 10000;

    if (gAudioStereo)
    {
        if (pVoice->mState.iPan < 0)
        {
            rightPan = 10000 - abs(pVoice->mState.iPan);
        }
        else if (pVoice->mState.iPan > 0)
        {
            leftPan = 10000 - abs(pVoice->mState.iPan);
        }
    }
    else
    {
        s32 r = (leftPan + rightPan) / 2;
        leftPan = r;
        rightPan = r;
    }

    mTempSoundBuffer[i].left = static_cast<s16>((((s * leftPan) / 10000) * pVoice->mState.iVolume) / 127);
    mTempSoundBuffer[i].right = static_cast<s16>((((s * rightPan) / 10000) * pVoice->mState.iVolume) / 127);

    pVoice->mState.fPlaybackPosition += pVoice->mState.fFrequency;
}

void SDLSoundSystem::RenderStereoSample(Sint16* pVoiceBufferPtr, SDLSoundBuffer* pVoice, s32 i)
{
    StereoSample_S16 pSample = reinterpret_cast<StereoSample_S16*>(pVoiceBufferPtr)[static_cast<s32>(pVoice->mState.fPlaybackPosition)];
    mTempSoundBuffer[i].left = static_cast<s16>((pSample.left * pVoice->mState.iVolume) / 127);
    mTempSoundBuffer[i].right = static_cast<s16>((pSample.right * pVoice->mState.iVolume) / 127);

    pVoice->mState.fPlaybackPosition += pVoice->mState.fFrequency;
}

void SDLSoundSystem::AudioCallBackStatic(void* userdata, SDL_AudioStream* stream, s32 additionalAmount, s32 /*totalAmount*/)
{
    static_cast<SDLSoundSystem*>(userdata)->AudioCallBack(stream, additionalAmount);
}
