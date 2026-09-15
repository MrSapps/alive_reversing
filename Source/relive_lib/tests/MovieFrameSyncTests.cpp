// Unit tests for ProcessMovieFrameSync (MovieFrameSync.hpp) - the drop/wait/render decision
// DDV_Play_Impl (Movie.cpp) makes for each decoded video frame against the movie's audio clock.
// Exercised here against a fake IMovieSyncClock instead of real audio/video/threads - frames are
// just plain integers/timestamps from a fake sequence, not real decoded pixels, per the same
// idea FileSystemTests.cpp already uses for pure-logic coverage elsewhere in this repo.

#include "MovieFrameSync.hpp"

#include <gtest/gtest.h>
#include <functional>
#include <vector>

namespace
{
    // A scriptable IMovieSyncClock: tests set the fields directly, and can hook OnPumpIdle to
    // simulate the audio clock advancing while ProcessMovieFrameSync's wait loop spins - exactly
    // what happens for real once SYS_EventsPump()/PSX_VSync() let more audio play out.
    class FakeMovieSyncClock final : public IMovieSyncClock
    {
    public:
        bool mAudioStarted = true;
        u64 mAudioClockMs = 0;
        bool mSkipRequested = false;
        u32 mPumpIdleCallCount = 0;
        std::function<void()> mOnPumpIdle;

        bool AudioStarted() const override
        {
            return mAudioStarted;
        }

        u64 AudioClockMs() const override
        {
            return mAudioClockMs;
        }

        bool SkipRequested() const override
        {
            return mSkipRequested;
        }

        void PumpIdle() override
        {
            ++mPumpIdleCallCount;
            if (mOnPumpIdle)
            {
                mOnPumpIdle();
            }
        }
    };
} // namespace

TEST(MovieFrameSync, RendersImmediatelyWhenAudioHasNotStartedYet)
{
    FakeMovieSyncClock clock;
    clock.mAudioStarted = false;
    // Wildly "behind" by every other rule, but with no audio clock to sync against yet, video
    // free-runs - matches a movie with no audio track at all.
    clock.mAudioClockMs = 999999;

    EXPECT_EQ(ProcessMovieFrameSync(0, clock), MovieFrameOutcome::Rendered);
    EXPECT_EQ(clock.mPumpIdleCallCount, 0u);
}

TEST(MovieFrameSync, RendersWhenExactlyInSync)
{
    FakeMovieSyncClock clock;
    clock.mAudioClockMs = 1000;

    EXPECT_EQ(ProcessMovieFrameSync(1000, clock), MovieFrameOutcome::Rendered);
    EXPECT_EQ(clock.mPumpIdleCallCount, 0u);
}

TEST(MovieFrameSync, RendersWhenBehindButWithinDropThreshold)
{
    FakeMovieSyncClock clock;
    // Exactly at the threshold: frameMs + kMovieFrameDropThresholdMs == audioClockMs is NOT
    // "less than", so this must not be dropped.
    clock.mAudioClockMs = 1000 + kMovieFrameDropThresholdMs;

    EXPECT_EQ(ProcessMovieFrameSync(1000, clock), MovieFrameOutcome::Rendered);
    EXPECT_EQ(clock.mPumpIdleCallCount, 0u);
}

TEST(MovieFrameSync, DropsWhenPastDropThreshold)
{
    FakeMovieSyncClock clock;
    clock.mAudioClockMs = 1000 + kMovieFrameDropThresholdMs + 1;

    EXPECT_EQ(ProcessMovieFrameSync(1000, clock), MovieFrameOutcome::Dropped);
    // Dropping doesn't wait on anything - it's a pure "skip this frame" decision.
    EXPECT_EQ(clock.mPumpIdleCallCount, 0u);
}

TEST(MovieFrameSync, WaitsThenRendersOnceClockCatchesUp)
{
    FakeMovieSyncClock clock;
    clock.mAudioClockMs = 0;
    // Each idle pump lets 40ms more of "audio" play out, same idea as real playback where a
    // PSX_VSync() tick corresponds to some real elapsed time.
    clock.mOnPumpIdle = [&clock]()
    {
        clock.mAudioClockMs += 40;
    };

    EXPECT_EQ(ProcessMovieFrameSync(120, clock), MovieFrameOutcome::Rendered);
    // 0 -> 40 -> 80 -> 120: three pumps needed before 120 <= audioClockMs holds.
    EXPECT_EQ(clock.mPumpIdleCallCount, 3u);
}

TEST(MovieFrameSync, ReturnsSkippedWithoutPumpingWhenSkipAlreadyHeld)
{
    FakeMovieSyncClock clock;
    clock.mAudioClockMs = 0;
    clock.mSkipRequested = true;
    // Never advances - if the implementation checked pump before skip, this would spin forever.
    clock.mOnPumpIdle = [&clock]()
    {
        FAIL() << "PumpIdle should not run once a skip is already held";
    };

    EXPECT_EQ(ProcessMovieFrameSync(1000, clock), MovieFrameOutcome::SkippedByUserInput);
    EXPECT_EQ(clock.mPumpIdleCallCount, 0u);
}

TEST(MovieFrameSync, ReturnsSkippedPartwayThroughAWait)
{
    FakeMovieSyncClock clock;
    clock.mAudioClockMs = 0;
    u32 pumpsBeforeSkip = 2;
    clock.mOnPumpIdle = [&]()
    {
        clock.mAudioClockMs += 10;
        if (--pumpsBeforeSkip == 0)
        {
            clock.mSkipRequested = true;
        }
    };

    EXPECT_EQ(ProcessMovieFrameSync(1000, clock), MovieFrameOutcome::SkippedByUserInput);
    EXPECT_EQ(clock.mPumpIdleCallCount, 2u);
}

// Reproduces the real "video freezes then hitches" bug report this suite was written to guard:
// once the audio clock is well ahead of a whole backlog of already-decoded/queued frames (e.g.
// because the decoder genuinely fell behind for a few real seconds - see Movie.cpp's decoder
// init comment), every one of those backlogged frames should be Dropped in one burst rather than
// rendered (which would show the stale frame for the entire backlog) or waited on (which would
// never resolve, since the clock isn't going to run backwards to meet them). A fake "demuxer" is
// just a sequence of plain frame numbers/timestamps here - no real pixels/audio needed to verify
// this.
TEST(MovieFrameSync, DropsAnEntireStaleBacklogBurstThenResumesRenderingLiveFrames)
{
    FakeMovieSyncClock clock;
    // The decoder fell behind for 11 real seconds - the audio clock kept advancing throughout.
    clock.mAudioClockMs = 11000;

    // ~50 backlogged frames at 15fps (66.67ms apart), timestamps starting well before the clock.
    std::vector<u64> backlogFrameTimestampsMs;
    for (int i = 0; i < 50; ++i)
    {
        backlogFrameTimestampsMs.push_back(static_cast<u64>(i) * 67);
    }

    u32 droppedCount = 0;
    u32 renderedCount = 0;
    for (const u64 frameMs : backlogFrameTimestampsMs)
    {
        switch (ProcessMovieFrameSync(frameMs, clock))
        {
            case MovieFrameOutcome::Dropped:
                ++droppedCount;
                break;
            case MovieFrameOutcome::Rendered:
                ++renderedCount;
                break;
            case MovieFrameOutcome::SkippedByUserInput:
                FAIL() << "no skip input held in this scenario";
                break;
        }
    }

    // The backlog runs up to (50-1)*67 = 3283ms, still well outside the clock's -100ms tolerance
    // from 11000ms, so every single backlogged frame should drop, not render - and none of them
    // should have blocked in a wait (a live-decoding pipeline must never be made to wait on
    // stale, already-late frames).
    EXPECT_EQ(droppedCount, 50u);
    EXPECT_EQ(renderedCount, 0u);
    EXPECT_EQ(clock.mPumpIdleCallCount, 0u);

    // Once caught up to a live (in-sync) frame, playback resumes rendering normally.
    EXPECT_EQ(ProcessMovieFrameSync(11000, clock), MovieFrameOutcome::Rendered);
}
