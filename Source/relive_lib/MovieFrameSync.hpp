#pragma once

#include "Types.hpp"

#include <optional>

// The clock movie frames are timed against. While the movie's audio is playing it follows the
// audio. Before the audio starts, after it ends (the video can run a few seconds past it) or
// when there's no audio at all, it runs on wall-clock time on from where it was, so the video
// keeps its pace instead of showing every remaining frame as fast as it can.
class MovieClock final
{
public:
    // audioClockMs: how much of the movie's audio has played, or nullopt if none is playing.
    // wallMs: any monotonically increasing ms time source (real playback uses SYS_GetTicks()).
    // Returns the movie time in ms.
    u64 Update(std::optional<u64> audioClockMs, u64 wallMs);

private:
    u64 mClockMs = 0;
    std::optional<u64> mLastWallMs;
};

// The bits of the environment movie playback's per-frame sync decision (see
// ProcessMovieFrameSync) needs - real playback wires this to MovieClock and
// AreMovieSkippingInputsHeld() (see Movie.cpp), tests wire it to a scripted fake so the
// drop/wait/render decisions can be verified without real audio/video/threads.
class IMovieSyncClock
{
public:
    virtual ~IMovieSyncClock() = default;

    // The movie time in ms (see MovieClock).
    virtual u64 ClockMs() const = 0;

    // True if the player is currently holding a skip-movie input.
    virtual bool SkipRequested() const = 0;
};

enum class MovieFrameOutcome
{
    Rendered,           // caller should display this frame
    Dropped,            // frame was too far behind the movie clock - skip it, don't display it
    Wait,               // frame is ahead of the movie clock - keep it and ask again next tick
    SkippedByUserInput, // a skip-movie input was held while waiting on the movie clock
};

// A frame counts as "too far behind" (and gets dropped rather than rendered) once the movie
// clock has moved more than this many ms past the frame's own timestamp.
constexpr u64 kMovieFrameDropThresholdMs = 100;

// Movie playback's drop/wait/render decision for a single already-dequeued video frame, factored
// out so it can be unit tested against a fake IMovieSyncClock instead of real audio/video/
// threads - see MovieFrameSyncTests.cpp. frameTimestampMs is the frame's own presentation
// timestamp (mkv PTS in ms). Never blocks: a frame that's ahead of the movie clock returns
// Wait, and the caller asks again with the same frame on its next tick.
MovieFrameOutcome ProcessMovieFrameSync(u64 frameTimestampMs, IMovieSyncClock& clock);

// While catching up on a backlog of Dropped frames (e.g. because decode fell behind for a few
// real seconds - see Movie.cpp's decoder init comment), nothing gets displayed at all until a
// frame is finally back in sync and actually Rendered - so playback looks completely frozen for
// however long the backlog takes to clear, then jumps straight to current. Rather than skip every
// Dropped frame in total silence, the caller should occasionally paint one of them anyway (still
// counted as dropped for droppedFrameCount/sync purposes - this is display-only), so it reads as
// fast-forwarding through stale content instead of a freeze. Throttled by wall-clock time so it
// doesn't slow down actually catching up. nowMs/lastDisplayMs are any shared, monotonically
// increasing time source (real playback uses SYS_GetTicks()).
constexpr u64 kStaleFrameDisplayIntervalMs = 200;
bool ShouldDisplayStaleFrame(u64 nowMs, u64 lastDisplayMs, u64 minIntervalMs = kStaleFrameDisplayIntervalMs);
