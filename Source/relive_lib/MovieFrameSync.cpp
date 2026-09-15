#include "stdafx.h"
#include "MovieFrameSync.hpp"

MovieFrameOutcome ProcessMovieFrameSync(u64 frameTimestampMs, IMovieSyncClock& clock)
{
    if (!clock.AudioStarted())
    {
        return MovieFrameOutcome::Rendered;
    }

    if (frameTimestampMs + kMovieFrameDropThresholdMs < clock.AudioClockMs())
    {
        return MovieFrameOutcome::Dropped;
    }

    while (frameTimestampMs > clock.AudioClockMs())
    {
        if (clock.SkipRequested())
        {
            return MovieFrameOutcome::SkippedByUserInput;
        }
        clock.PumpIdle();
    }

    return MovieFrameOutcome::Rendered;
}

bool ShouldDisplayStaleFrame(u64 nowMs, u64 lastDisplayMs, u64 minIntervalMs)
{
    return (nowMs - lastDisplayMs) >= minIntervalMs;
}
