#include "AudioStream.hpp"
#include <algorithm>

AudioStream::AudioStream(u32 sampleRate, u32 channels, u32 capacityFrames, u32 deviceRate)
    : mChannels(channels > 1 ? 2 : 1)
    , mCapacityFrames(capacityFrames)
    , mStep(static_cast<f32>(sampleRate) / static_cast<f32>(deviceRate))
{
    mRing.resize(static_cast<size_t>(capacityFrames) * mChannels);
}

u32 AudioStream::Write(const s16* pPcm, u32 frames)
{
    std::lock_guard<std::mutex> lock(mMutex);
    const u32 count = std::min(frames, mCapacityFrames - mBufferedFrames);
    u32 writeFrame = (mReadFrame + mBufferedFrames) % mCapacityFrames;
    for (u32 i = 0; i < count; i++)
    {
        for (u32 c = 0; c < mChannels; c++)
        {
            mRing[static_cast<size_t>(writeFrame) * mChannels + c] = pPcm[static_cast<size_t>(i) * mChannels + c];
        }
        writeFrame = (writeFrame + 1) % mCapacityFrames;
    }
    mBufferedFrames += count;
    return count;
}

u32 AudioStream::BufferedFrames() const
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mBufferedFrames;
}

void AudioStream::Start()
{
    std::lock_guard<std::mutex> lock(mMutex);
    if (!mStopped)
    {
        mPlaying = true;
    }
}

void AudioStream::Stop()
{
    std::lock_guard<std::mutex> lock(mMutex);
    mStopped = true;
}

bool AudioStream::IsFinished() const
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mStopped && (mFade == 0 || !mPlaying);
}

void AudioStream::Mix(StereoSample_S16* pOut, u32 frames)
{
    std::lock_guard<std::mutex> lock(mMutex);
    if (!mPlaying)
    {
        return;
    }

    for (u32 i = 0; i < frames; i++)
    {
        // Interpolating needs this frame and the next. Without them (the game hasn't written
        // enough yet) it fades out, and in again once there is.
        const bool starved = mBufferedFrames < 2;
        if (mStopped || starved)
        {
            mFade = mFade > 0 ? mFade - 1 : 0;
        }
        else if (mFade < kFadeFrames)
        {
            mFade++;
        }
        if (starved || mFade == 0)
        {
            continue;
        }

        const u32 next = (mReadFrame + 1) % mCapacityFrames;
        const f32 a = static_cast<f32>(mRing[static_cast<size_t>(mReadFrame) * mChannels]);
        const f32 b = static_cast<f32>(mRing[static_cast<size_t>(next) * mChannels]);
        const f32 left = a + (b - a) * mFraction;
        f32 right = left;
        if (mChannels == 2)
        {
            const f32 ar = static_cast<f32>(mRing[static_cast<size_t>(mReadFrame) * 2 + 1]);
            const f32 br = static_cast<f32>(mRing[static_cast<size_t>(next) * 2 + 1]);
            right = ar + (br - ar) * mFraction;
        }

        const f32 gain = kGain * static_cast<f32>(mFade) / static_cast<f32>(kFadeFrames);
        pOut[i].left = static_cast<s16>(std::clamp<s32>(pOut[i].left + static_cast<s32>(left * gain), -0x8000, 0x7FFF));
        pOut[i].right = static_cast<s16>(std::clamp<s32>(pOut[i].right + static_cast<s32>(right * gain), -0x8000, 0x7FFF));

        mFraction += mStep;
        while (mFraction >= 1.0f && mBufferedFrames > 1)
        {
            mFraction -= 1.0f;
            mReadFrame = (mReadFrame + 1) % mCapacityFrames;
            mBufferedFrames--;
        }
    }
}
