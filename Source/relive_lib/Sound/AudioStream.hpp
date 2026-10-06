#pragma once

#include "../Types.hpp"
#include <mutex>
#include <vector>

template <typename T>
struct StereoSample
{
    T left;
    T right;
};

using StereoSample_S16 = StereoSample<s16>;
using StereoSample_S32 = StereoSample<s32>;
using StereoSample_F32 = StereoSample<f32>;

// 16 bit PCM that the game feeds in as it decodes it (a movie's audio) and the sound system mixes
// with the SPU's output, resampled to the device's rate. Written by the game thread, mixed by the
// audio thread.
//
// It starts and stops with a short fade, so a movie that is skipped doesn't click.
class AudioStream final
{
public:
    // channels: 1 (played on both sides) or 2. capacityFrames: how much it holds before Write
    // takes no more. deviceRate: the rate Mix is called at.
    AudioStream(u32 sampleRate, u32 channels, u32 capacityFrames, u32 deviceRate);

    // Game thread: appends up to frames frames (interleaved, 16 bit) and returns how many fitted
    u32 Write(const s16* pPcm, u32 frames);
    // Frames written and not mixed yet
    u32 BufferedFrames() const;

    // Starts mixing what has been written (it fades in). Stop fades it out, and it can't be started again.
    void Start();
    void Stop();

    // Audio thread: adds frames frames to pOut. Nothing is added while it isn't playing, and
    // where the game hasn't written enough yet.
    void Mix(StereoSample_S16* pOut, u32 frames);
    // True once it was stopped and has faded out: the sound system can drop it
    bool IsFinished() const;

    // The gain a stream is mixed with, on top of the device's output: movie audio is quieter than
    // the SPU's
    static constexpr f32 kGain = 0.295f;
    // The frames a fade in or out takes
    static constexpr u32 kFadeFrames = 127;

private:
    const u32 mChannels;
    const u32 mCapacityFrames;
    const f32 mStep; // Source frames per output frame

    mutable std::mutex mMutex;
    std::vector<s16> mRing;
    u32 mReadFrame = 0;
    u32 mBufferedFrames = 0;
    // Where between mReadFrame and the next frame the output is, 0-1
    f32 mFraction = 0.0f;
    bool mPlaying = false;
    bool mStopped = false;
    u32 mFade = 0; // 0 = silent, kFadeFrames = full
};
