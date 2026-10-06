#include <gtest/gtest.h>
#include "../Sound/SoundGold.hpp"
#include "../Sound/GameSound.hpp"
#include "../Sound/SDLSoundSystem.hpp"
#include "../GameType.hpp"
#include <algorithm>
#include <vector>

static SDLSoundSystem& Sound()
{
    return GameSound::Get()->System();
}

// Streams a known signal through an audio stream the way the movie player feeds its audio
// (Movie.cpp FeedAudio: a 4 s ring the writer keeps up to size-1024 ahead of the reader),
// rendering in device sized chunks, and checks the output is that signal with no gaps, repeats or
// skips.
// chunkFrames: the device chunk size, 0 for a mix of sizes like the render thread's
static void StreamTest(u32 chunkFrames)
{
    SoundGoldSession session(GameType::eAe);

    const u32 rate = 37800;
    const u32 frames = rate * 4;
    const std::shared_ptr<AudioStream> stream = Sound().CreateStream(rate, 2, frames);

    // The signal: a sawtooth of the source frame index, so every output sample says where it
    // came from
    const u32 maxBuffered = frames - 1024;
    u64 submitted = 0;
    bool started = false;
    auto feed = [&]() {
        for (;;)
        {
            const u32 buffered = stream->BufferedFrames();
            if (buffered >= maxBuffered)
            {
                return;
            }
            const u32 n = std::min(maxBuffered - buffered, 1024u);
            std::vector<s16> chunk(n * 2);
            for (u32 i = 0; i < n; i++)
            {
                const s16 v = static_cast<s16>(((submitted + i) % 16384) - 8192);
                chunk[i * 2] = v;
                chunk[i * 2 + 1] = v;
            }
            ASSERT_EQ(stream->Write(chunk.data(), n), n);
            submitted += n;
            if (!started && submitted >= rate / 5)
            {
                stream->Start();
                started = true;
            }
        }
    };

    // 12 s: a frame of feeding then a frame's worth of device chunks
    for (u32 frame = 0; frame < 12 * 30; frame++)
    {
        feed();
        u32 left = 1470;
        u32 seed = frame * 7919u + 13u;
        while (left > 0)
        {
            seed = seed * 1103515245u + 12345u;
            const u32 n = chunkFrames ? std::min(chunkFrames, left) : std::min(left, 64u + (seed >> 16) % 2048u);
            Sound().RenderOffline(n);
            left -= n;
        }
    }

    const std::vector<StereoSample_S16>& out = Sound().OfflineOutput();
    ASSERT_GT(out.size(), 44100u * 10u);

    // The source index advances 37800/44100 a sample, so the output steps up by that times the
    // stream's gain, and drops by 16384 times the gain once a sawtooth period (over a sample or
    // two, as the output is interpolated between the source frames)
    std::vector<s32> diffs;
    for (size_t i = 44100; i + 1 < out.size(); i++)
    {
        diffs.push_back(out[i + 1].left - out[i].left);
    }
    std::vector<s32> sorted = diffs;
    std::sort(sorted.begin(), sorted.end());
    const s32 step = sorted[sorted.size() / 2]; // 0 or 1 mostly
    s32 anomalies = 0;
    s32 drops = 0;
    std::string first;
    for (size_t i = 0; i < diffs.size(); i++)
    {
        const s32 d = diffs[i];
        if (d < -300)
        {
            drops++;
        }
        else if (d < step - 1 || d > step + 2)
        {
            anomalies++;
            if (anomalies <= 8)
            {
                first += " t=" + std::to_string((44100 + i) / 44100.0) + "s d=" + std::to_string(d);
            }
        }
    }
    EXPECT_EQ(anomalies, 0) << "discontinuities in the streamed audio:" << first;
    // 11 s of the period 16384 / 37800 s, one or two samples each
    EXPECT_GE(drops, 20);
    EXPECT_LE(drops, 60);

    stream->Stop();
}

TEST(SoundStreaming, StreamsContinuously)
{
    StreamTest(470);
}

TEST(SoundStreaming, StreamsContinuouslyInBigChunks)
{
    StreamTest(2048);
}

TEST(SoundStreaming, StreamsContinuouslyInMixedChunks)
{
    StreamTest(0);
}

// A loud constant signal, started and rendered for 2000 frames
static std::shared_ptr<AudioStream> StartConstantStream(u32 rate, u32 channels)
{
    const std::shared_ptr<AudioStream> stream = Sound().CreateStream(rate, channels, rate);
    std::vector<s16> signal(static_cast<size_t>(rate) * channels, 8000);
    EXPECT_EQ(stream->Write(signal.data(), rate), rate);
    stream->Start();
    return stream;
}

static std::vector<s32> LeftSamples()
{
    std::vector<s32> left;
    for (const StereoSample_S16& s : Sound().OfflineOutput())
    {
        left.push_back(s.left);
    }
    return left;
}

// A stream that starts fades in instead of clicking
TEST(SoundStreaming, StartFadesIn)
{
    SoundGoldSession session(GameType::eAe);
    const std::shared_ptr<AudioStream> stream = StartConstantStream(44100, 2);
    Sound().RenderOffline(1000);

    const std::vector<s32> left = LeftSamples();
    ASSERT_GE(left.size(), 1000u);
    const s32 level = left[900];
    ASSERT_GT(level, 500);
    EXPECT_LT(left[0], level / 10);
    s32 maxStep = 0;
    for (size_t i = 1; i < 400; i++)
    {
        maxStep = std::max(maxStep, std::abs(left[i] - left[i - 1]));
    }
    EXPECT_LT(maxStep, level / 10);
}

// A playing stream that is stopped (a skipped movie) fades out instead of being cut off
TEST(SoundStreaming, StoppedStreamFadesOut)
{
    SoundGoldSession session(GameType::eAe);
    const std::shared_ptr<AudioStream> stream = StartConstantStream(44100, 2);
    Sound().RenderOffline(2000);
    stream->Stop();
    Sound().RenderOffline(1000);

    const std::vector<s32> left = LeftSamples();
    ASSERT_GE(left.size(), 3000u);
    const s32 level = left[1900];
    ASSERT_GT(std::abs(level), 500);

    // Steps down gently from the level it was at to nothing, and stays there
    s32 maxStep = 0;
    for (size_t i = 2000; i < 2400; i++)
    {
        maxStep = std::max(maxStep, std::abs(left[i] - left[i - 1]));
    }
    EXPECT_LT(maxStep, std::abs(level) / 10);
    EXPECT_EQ(left[2400], 0);
    EXPECT_EQ(left[2900], 0);
}

// A mono stream is played on both sides, and at its own rate
TEST(SoundStreaming, MonoStreamPlaysOnBothSides)
{
    SoundGoldSession session(GameType::eAe);
    const std::shared_ptr<AudioStream> stream = StartConstantStream(18900, 1);
    Sound().RenderOffline(2000);
    const std::vector<StereoSample_S16>& out = Sound().OfflineOutput();
    EXPECT_GT(out[1900].left, 500);
    EXPECT_EQ(out[1900].left, out[1900].right);
}

// Without data the stream is silent, and plays on when there is some again
TEST(SoundStreaming, StarvedStreamIsSilentThenResumes)
{
    SoundGoldSession session(GameType::eAe);
    const std::shared_ptr<AudioStream> stream = Sound().CreateStream(44100, 2, 44100);
    std::vector<s16> signal(2 * 1000, 8000);
    ASSERT_EQ(stream->Write(signal.data(), 1000), 1000u);
    stream->Start();
    Sound().RenderOffline(3000); // Plays the 1000 frames, then has none
    ASSERT_EQ(stream->Write(signal.data(), 1000), 1000u);
    Sound().RenderOffline(3000);

    const std::vector<s32> left = LeftSamples();
    EXPECT_GT(left[900], 500);
    EXPECT_EQ(left[2500], 0);
    EXPECT_GT(left[3500], 500);
}
