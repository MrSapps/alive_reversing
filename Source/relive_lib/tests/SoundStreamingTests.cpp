#include <gtest/gtest.h>
#include "../Sound/SoundGold.hpp"
#include "../Sound/Sound.hpp"
#include "../Sound/SDLSoundSystem.hpp"
#include "../GameType.hpp"
#include <algorithm>
#include <vector>

// Streams a known signal through a looping sound buffer the way the movie player feeds its
// audio (Movie.cpp FeedAudio: a 4 s ring the writer keeps up to size-1024 ahead of the read
// cursor), rendering in device sized chunks, and checks the output is that signal with no
// gaps, repeats or skips.
// chunkFrames: the device chunk size, 0 for a mix of sizes like the render thread's
static void StreamTest(u32 chunkFrames)
{
    SoundGoldSession session(GameType::eAe);

    const u32 rate = 37800;
    const u32 frames = rate * 4;
    SoundEntry entry = {};
    ASSERT_EQ(SND_New(&entry, static_cast<s32>(frames), static_cast<s32>(rate), 16, 7), 0);

    // The signal: a sawtooth of the source frame index, so every output sample says where it
    // came from
    const u32 maxBuffered = frames - 1024;
    u32 writeOffset = 0;
    u64 submitted = 0;
    bool started = false;
    auto feed = [&]() {
        for (;;)
        {
            const u32 readOffset = started ? GetSoundAPI().mSND_Get_Sound_Entry_Pos(&entry) : 0;
            const u32 buffered = started ? (writeOffset >= readOffset ? writeOffset - readOffset : frames - readOffset + writeOffset) : static_cast<u32>(submitted);
            if (buffered >= maxBuffered)
            {
                return;
            }
            const u32 n = std::min({maxBuffered - buffered, frames - writeOffset, 1024u});
            if (n == 0)
            {
                writeOffset = 0;
                continue;
            }
            std::vector<s16> chunk(n * 2);
            for (u32 i = 0; i < n; i++)
            {
                const s16 v = static_cast<s16>(((submitted + i) % 16384) - 8192);
                chunk[i * 2] = v;
                chunk[i * 2 + 1] = v;
            }
            ASSERT_GE(GetSoundAPI().mSND_LoadSamples(&entry, writeOffset, reinterpret_cast<u8*>(chunk.data()), n), 0);
            writeOffset = (writeOffset + n) % frames;
            submitted += n;
            if (!started && submitted >= rate / 5)
            {
                ASSERT_EQ(SND_PlayEx(&entry, 116, 116, 1.0f, nullptr, 1, 100), 0);
                started = true;
            }
        }
    };

    // 12 s: three laps of the ring, a frame of feeding then a frame's worth of device chunks
    for (u32 frame = 0; frame < 12 * 30; frame++)
    {
        feed();
        u32 left = 1470;
        u32 seed = frame * 7919u + 13u;
        while (left > 0)
        {
            seed = seed * 1103515245u + 12345u;
            const u32 n = chunkFrames ? std::min(chunkFrames, left) : std::min(left, 64u + (seed >> 16) % 2048u);
            sDSound_BBC344->RenderOffline(n);
            left -= n;
        }
    }

    const std::vector<StereoSample_S16>& out = sDSound_BBC344->OfflineOutput();
    ASSERT_GT(out.size(), 44100u * 10u);

    // The source index advances 37800/44100 a sample, so the output steps up by that times the
    // volume scale, and drops by 16384 times the scale once a sawtooth period
    std::vector<s32> diffs;
    for (size_t i = 44100; i + 1 < out.size(); i++)
    {
        diffs.push_back(out[i + 1].left - out[i].left);
    }
    std::vector<s32> sorted = diffs;
    std::sort(sorted.begin(), sorted.end());
    const s32 step = sorted[sorted.size() / 2]; // 0 or 1 mostly
    s32 anomalies = 0;
    std::string first;
    for (size_t i = 0; i < diffs.size(); i++)
    {
        const s32 d = diffs[i];
        const bool wrap = d < -3000; // the sawtooth's drop (16384 times the ~0.3 output scale)
        if (!wrap && (d < step - 1 || d > step + 2))
        {
            anomalies++;
            if (anomalies <= 8)
            {
                first += " t=" + std::to_string((44100 + i) / 44100.0) + "s d=" + std::to_string(d);
            }
        }
    }
    EXPECT_EQ(anomalies, 0) << "discontinuities in the streamed audio:" << first;

    GetSoundAPI().mSND_Free(&entry);
}

TEST(SoundStreaming, LoopingBufferStreamsContinuously)
{
    StreamTest(470);
}

TEST(SoundStreaming, LoopingBufferStreamsContinuouslyInBigChunks)
{
    StreamTest(2048);
}

TEST(SoundStreaming, LoopingBufferStreamsContinuouslyInMixedChunks)
{
    StreamTest(0);
}
