#pragma once

#include "Types.hpp"

// The bits of the environment DDV_Play_Impl's per-frame sync decision (see
// ProcessMovieFrameSync) needs - real playback wires this to
// SND_Get_Generated_Audio_Samples()/SND_Get_Device_Sample_Rate()/AreMovieSkippingInputsHeld()/
// SYS_EventsPump()+PSX_VSync() (see Movie.cpp), tests wire it to a scripted fake so the
// drop/wait/render decisions can be verified without real audio/video/threads.
class IMovieSyncClock
{
public:
    virtual ~IMovieSyncClock() = default;

    // Has playback audio actually started yet (still prerolling if not) - while false, video
    // free-runs (every frame renders immediately, no drop/wait), matching a movie with no
    // audio track at all.
    virtual bool AudioStarted() const = 0;

    // Milliseconds of movie audio the mixer has actually played so far.
    virtual u64 AudioClockMs() const = 0;

    // True if the player is currently holding a skip-movie input.
    virtual bool SkipRequested() const = 0;

    // Called once per spin while waiting for the audio clock to catch up to a frame that's
    // running ahead of it - pumps window/input events and yields a video sync tick.
    virtual void PumpIdle() = 0;
};

enum class MovieFrameOutcome
{
    Rendered,           // caller should display this frame
    Dropped,            // frame was too far behind the audio clock - skip it, don't display it
    SkippedByUserInput, // a skip-movie input was held while waiting on the audio clock
};

// A frame counts as "too far behind" (and gets dropped rather than rendered) once the audio
// clock has moved more than this many ms past the frame's own timestamp.
constexpr u64 kMovieFrameDropThresholdMs = 100;

// DDV_Play_Impl's drop/wait/render decision for a single already-dequeued video frame, factored
// out so it can be unit tested against a fake IMovieSyncClock instead of real audio/video/
// threads - see MovieFrameSyncTests.cpp. frameTimestampMs is the frame's own presentation
// timestamp (mkv PTS in ms).
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
