#include "stdafx.h"
#include "MovieFrameSync.hpp"

MovieFrameOutcome ProcessMovieFrameSync(u64 frameTimestampMs, IMovieSyncClock& clock)
{
    if (frameTimestampMs + kMovieFrameDropThresholdMs < clock.ClockMs())
    {
        return MovieFrameOutcome::Dropped;
    }

    if (frameTimestampMs > clock.ClockMs())
    {
        if (clock.SkipRequested())
        {
            return MovieFrameOutcome::SkippedByUserInput;
        }
        return MovieFrameOutcome::Wait;
    }

    return MovieFrameOutcome::Rendered;
}

bool ShouldDisplayStaleFrame(u64 nowMs, u64 lastDisplayMs, u64 minIntervalMs)
{
    return (nowMs - lastDisplayMs) >= minIntervalMs;
}

u64 MovieClock::Update(std::optional<u64> audioClockMs, u64 wallMs)
{
    if (audioClockMs)
    {
        mClockMs = *audioClockMs;
    }
    else if (mLastWallMs)
    {
        mClockMs += wallMs - *mLastWallMs;
    }
    mLastWallMs = wallMs;
    return mClockMs;
}
