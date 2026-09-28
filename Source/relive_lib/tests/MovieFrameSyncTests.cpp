// Unit tests for ProcessMovieFrameSync (MovieFrameSync.hpp) - the drop/wait/render decision
// movie playback (Movie.cpp) makes for each decoded video frame against the movie clock, and for MovieClock.
// Exercised here against a fake IMovieSyncClock instead of real audio/video/threads - frames are
// just plain integers/timestamps from a fake sequence, not real decoded pixels, per the same
// idea FileSystemTests.cpp already uses for pure-logic coverage elsewhere in this repo.

#include "MovieFrameSync.hpp"

#include <gtest/gtest.h>
#include <vector>

namespace
{
    // A scriptable IMovieSyncClock: tests set the fields directly, advancing the movie clock
    // between calls the way real playback's main loop ticks let more audio play out.
    class FakeMovieSyncClock final : public IMovieSyncClock
    {
    public:
        u64 mClockMs = 0;
        bool mSkipRequested = false;

        u64 ClockMs() const override
        {
            return mClockMs;
        }

        bool SkipRequested() const override
        {
            return mSkipRequested;
        }
    };
} // namespace

TEST(MovieFrameSync, RendersWhenExactlyInSync)
{
    FakeMovieSyncClock clock;
    clock.mClockMs = 1000;

    EXPECT_EQ(ProcessMovieFrameSync(1000, clock), MovieFrameOutcome::Rendered);
}

TEST(MovieFrameSync, RendersWhenBehindButWithinDropThreshold)
{
    FakeMovieSyncClock clock;
    // Exactly at the threshold: frameMs + kMovieFrameDropThresholdMs == audioClockMs is NOT
    // "less than", so this must not be dropped.
    clock.mClockMs = 1000 + kMovieFrameDropThresholdMs;

    EXPECT_EQ(ProcessMovieFrameSync(1000, clock), MovieFrameOutcome::Rendered);
}

TEST(MovieFrameSync, DropsWhenPastDropThreshold)
{
    FakeMovieSyncClock clock;
    clock.mClockMs = 1000 + kMovieFrameDropThresholdMs + 1;

    EXPECT_EQ(ProcessMovieFrameSync(1000, clock), MovieFrameOutcome::Dropped);
    // Dropping doesn't wait on anything - it's a pure "skip this frame" decision.
}

TEST(MovieFrameSync, WaitsThenRendersOnceClockCatchesUp)
{
    FakeMovieSyncClock clock;
    clock.mClockMs = 0;

    // Each tick lets 40ms more of "audio" play out, same idea as real playback where a main
    // loop tick corresponds to some real elapsed time.
    u32 waitCount = 0;
    MovieFrameOutcome outcome = MovieFrameOutcome::Wait;
    while ((outcome = ProcessMovieFrameSync(120, clock)) == MovieFrameOutcome::Wait)
    {
        ++waitCount;
        clock.mClockMs += 40;
    }

    EXPECT_EQ(outcome, MovieFrameOutcome::Rendered);
    // 0 -> 40 -> 80 -> 120: three ticks needed before 120 <= audioClockMs holds.
    EXPECT_EQ(waitCount, 3u);
}

TEST(MovieFrameSync, ReturnsSkippedInsteadOfWaitingWhenSkipHeld)
{
    FakeMovieSyncClock clock;
    clock.mClockMs = 0;
    clock.mSkipRequested = true;

    EXPECT_EQ(ProcessMovieFrameSync(1000, clock), MovieFrameOutcome::SkippedByUserInput);
}

TEST(MovieFrameSync, ReturnsSkippedPartwayThroughAWait)
{
    FakeMovieSyncClock clock;
    clock.mClockMs = 0;

    u32 waitCount = 0;
    MovieFrameOutcome outcome = MovieFrameOutcome::Wait;
    while ((outcome = ProcessMovieFrameSync(1000, clock)) == MovieFrameOutcome::Wait)
    {
        clock.mClockMs += 10;
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
    clock.mClockMs = 11000;

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

TEST(MovieClock, FollowsTheAudioWhileItPlays)
{
    MovieClock clock;
    EXPECT_EQ(clock.Update(0, 1000), 0u);
    // The audio, not the wall clock, decides: here it's running a bit slower
    EXPECT_EQ(clock.Update(90, 1100), 90u);
    EXPECT_EQ(clock.Update(180, 1200), 180u);
}

TEST(MovieClock, RunsOnWallClockTimeAfterTheAudioEnds)
{
    MovieClock clock;
    EXPECT_EQ(clock.Update(5000, 10000), 5000u);
    EXPECT_EQ(clock.Update(std::nullopt, 10016), 5016u);
    EXPECT_EQ(clock.Update(std::nullopt, 11016), 6016u);
}

TEST(MovieClock, RunsOnWallClockTimeWithNoAudio)
{
    MovieClock clock;
    // Starts at 0 whatever the wall clock reads
    EXPECT_EQ(clock.Update(std::nullopt, 12345), 0u);
    EXPECT_EQ(clock.Update(std::nullopt, 12345 + 67), 67u);
}

// Plays a 15fps movie on a 60Hz main loop and returns the wall-clock time it took. The audio
// stops at audioEndMs (nullopt: no audio). Every frame must be rendered, none dropped. It
// should take about as long as the video, give or take a frame.
static u64 PlayMovie(u64 videoEndMs, std::optional<u64> audioEndMs)
{
    MovieClock movieClock;
    FakeMovieSyncClock clock;
    u64 wallMs = 0;
    u64 frameMs = 0;
    while (frameMs <= videoEndMs)
    {
        const bool audioPlaying = audioEndMs && wallMs < *audioEndMs;
        clock.mClockMs = movieClock.Update(audioPlaying ? std::optional<u64>(wallMs) : std::nullopt, wallMs);
        const MovieFrameOutcome outcome = ProcessMovieFrameSync(frameMs, clock);
        EXPECT_NE(outcome, MovieFrameOutcome::Dropped) << "frame " << frameMs;
        if (outcome != MovieFrameOutcome::Wait)
        {
            frameMs += 67;
        }
        wallMs += 16;
    }
    return wallMs;
}

// The video can run a few seconds past the end of its audio. Those last seconds used to play
// as fast as the main loop could show frames, because with the audio gone there was no clock.
TEST(MovieFrameSync, VideoThatOutlastsItsAudioKeepsItsPace)
{
    const u64 wallMs = PlayMovie(8000, 5000);
    EXPECT_GE(wallMs, 8000u - 67u);
    EXPECT_LE(wallMs, 8000u + 67u + 16u);
}

TEST(MovieFrameSync, VideoWithNoAudioKeepsItsPace)
{
    const u64 wallMs = PlayMovie(3000, std::nullopt);
    EXPECT_GE(wallMs, 3000u - 67u);
    EXPECT_LE(wallMs, 3000u + 67u + 16u);
}
