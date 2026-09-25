// Unit tests for ProcessMovieFrameSync (MovieFrameSync.hpp) - the drop/wait/render decision
// movie playback (Movie.cpp) makes for each decoded video frame against the movie's audio clock.
// Exercised here against a fake IMovieSyncClock instead of real audio/video/threads - frames are
// just plain integers/timestamps from a fake sequence, not real decoded pixels, per the same
// idea FileSystemTests.cpp already uses for pure-logic coverage elsewhere in this repo.

#include "MovieFrameSync.hpp"

#include <gtest/gtest.h>
#include <vector>

namespace
{
    // A scriptable IMovieSyncClock: tests set the fields directly, advancing the audio clock
    // between calls the way real playback's main loop ticks let more audio play out.
    class FakeMovieSyncClock final : public IMovieSyncClock
    {
    public:
        bool mAudioStarted = true;
        u64 mAudioClockMs = 0;
        bool mSkipRequested = false;

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
}

TEST(MovieFrameSync, RendersWhenExactlyInSync)
{
    FakeMovieSyncClock clock;
    clock.mAudioClockMs = 1000;

    EXPECT_EQ(ProcessMovieFrameSync(1000, clock), MovieFrameOutcome::Rendered);
}

TEST(MovieFrameSync, RendersWhenBehindButWithinDropThreshold)
{
    FakeMovieSyncClock clock;
    // Exactly at the threshold: frameMs + kMovieFrameDropThresholdMs == audioClockMs is NOT
    // "less than", so this must not be dropped.
    clock.mAudioClockMs = 1000 + kMovieFrameDropThresholdMs;

    EXPECT_EQ(ProcessMovieFrameSync(1000, clock), MovieFrameOutcome::Rendered);
}

TEST(MovieFrameSync, DropsWhenPastDropThreshold)
{
    FakeMovieSyncClock clock;
    clock.mAudioClockMs = 1000 + kMovieFrameDropThresholdMs + 1;

    EXPECT_EQ(ProcessMovieFrameSync(1000, clock), MovieFrameOutcome::Dropped);
    // Dropping doesn't wait on anything - it's a pure "skip this frame" decision.
}

TEST(MovieFrameSync, WaitsThenRendersOnceClockCatchesUp)
{
    FakeMovieSyncClock clock;
    clock.mAudioClockMs = 0;

    // Each tick lets 40ms more of "audio" play out, same idea as real playback where a main
    // loop tick corresponds to some real elapsed time.
    u32 waitCount = 0;
    MovieFrameOutcome outcome = MovieFrameOutcome::Wait;
    while ((outcome = ProcessMovieFrameSync(120, clock)) == MovieFrameOutcome::Wait)
    {
        ++waitCount;
        clock.mAudioClockMs += 40;
    }

    EXPECT_EQ(outcome, MovieFrameOutcome::Rendered);
    // 0 -> 40 -> 80 -> 120: three ticks needed before 120 <= audioClockMs holds.
    EXPECT_EQ(waitCount, 3u);
}

TEST(MovieFrameSync, ReturnsSkippedInsteadOfWaitingWhenSkipHeld)
{
    FakeMovieSyncClock clock;
    clock.mAudioClockMs = 0;
    clock.mSkipRequested = true;

    EXPECT_EQ(ProcessMovieFrameSync(1000, clock), MovieFrameOutcome::SkippedByUserInput);
}

TEST(MovieFrameSync, ReturnsSkippedPartwayThroughAWait)
{
    FakeMovieSyncClock clock;
    clock.mAudioClockMs = 0;

    u32 waitCount = 0;
    MovieFrameOutcome outcome = MovieFrameOutcome::Wait;
    while ((outcome = ProcessMovieFrameSync(1000, clock)) == MovieFrameOutcome::Wait)
    {
        clock.mAudioClockMs += 10;
        if (++waitCount == 2)
        {
            clock.mSkipRequested = true;
        }
    }

    EXPECT_EQ(outcome, MovieFrameOutcome::SkippedByUserInput);
    EXPECT_EQ(waitCount, 2u);
}

// Reproduces the real "video freezes then hitches" bug report this suite was written to guard:
// once the audio clock is well ahead of a whole backlog of already-decoded/queued frames (e.g.
// because decode genuinely fell behind for a few real seconds - see Movie.cpp's decoder init
// comment; turned out to be a debug-build-only slowdown, not a real throughput problem, but the
// backlog-handling behavior being tested here matters regardless of why a backlog built up),
// every one of those backlogged frames should be Dropped in one burst rather than rendered
// (which would show the stale frame for the entire backlog) or waited on (which would never
// resolve, since the clock isn't going to run backwards to meet them). A fake "demuxer" is just
// a sequence of plain frame numbers/timestamps here - no real pixels/audio needed to verify this.
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
    u32 waitCount = 0;
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
            case MovieFrameOutcome::Wait:
                ++waitCount;
                break;
            case MovieFrameOutcome::SkippedByUserInput:
                FAIL() << "no skip input held in this scenario";
                break;
        }
    }

    // The backlog runs up to (50-1)*67 = 3283ms, still well outside the clock's -100ms tolerance
    // from 11000ms, so every single backlogged frame should drop, not render - and none of them
    // should have been told to wait (a live-decoding pipeline must never be made to wait on
    // stale, already-late frames).
    EXPECT_EQ(droppedCount, 50u);
    EXPECT_EQ(renderedCount, 0u);
    EXPECT_EQ(waitCount, 0u);

    // Once caught up to a live (in-sync) frame, playback resumes rendering normally.
    EXPECT_EQ(ProcessMovieFrameSync(11000, clock), MovieFrameOutcome::Rendered);
}

TEST(MovieFrameSync, DoesNotDisplayStaleFrameBeforeIntervalElapses)
{
    EXPECT_FALSE(ShouldDisplayStaleFrame(/*nowMs=*/1000, /*lastDisplayMs=*/900, /*minIntervalMs=*/200));
}

TEST(MovieFrameSync, DisplaysStaleFrameOnceIntervalElapses)
{
    EXPECT_TRUE(ShouldDisplayStaleFrame(/*nowMs=*/1100, /*lastDisplayMs=*/900, /*minIntervalMs=*/200));
}

TEST(MovieFrameSync, DisplaysStaleFrameExactlyAtInterval)
{
    EXPECT_TRUE(ShouldDisplayStaleFrame(/*nowMs=*/1100, /*lastDisplayMs=*/900, /*minIntervalMs=*/200));
    EXPECT_FALSE(ShouldDisplayStaleFrame(/*nowMs=*/1099, /*lastDisplayMs=*/900, /*minIntervalMs=*/200));
}

// A whole backlog of Dropped frames still only trickles a handful of stale frames to the screen
// at the configured throttle rate, rather than painting every single one of them (which would
// defeat catching up quickly) - simulates "wall clock time" advancing a fixed amount per
// backlogged frame, same idea DropsAnEntireStaleBacklogBurstThenResumesRenderingLiveFrames above
// uses for the audio clock.
TEST(MovieFrameSync, ThrottlesStaleFrameDisplayAcrossABacklog)
{
    u64 nowMs = 0;
    u64 lastDisplayMs = 0;
    u32 displayedCount = 0;

    // 100 backlogged frames, 10ms of "wall clock" apart - a real decode stall spans a much wider
    // range of frame timestamps than wall-clock time (see the DropsAnEntireStaleBacklogBurst
    // test's own frame timestamps vs. this test's time source), but what's under test here is
    // purely the throttle, so a plain fixed step is enough.
    for (int i = 0; i < 100; ++i)
    {
        nowMs += 10;
        if (ShouldDisplayStaleFrame(nowMs, lastDisplayMs))
        {
            ++displayedCount;
            lastDisplayMs = nowMs;
        }
    }

    // 1000ms of backlog at a 200ms throttle should display roughly 1000/200 = 5 times, not all
    // 100 (silent/frozen) or 0 (still frozen, just differently).
    EXPECT_GT(displayedCount, 0u);
    EXPECT_LT(displayedCount, 100u);
    EXPECT_EQ(displayedCount, 5u);
}
