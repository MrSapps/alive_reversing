#pragma once

#include "../Types.hpp"

#include <deque>
#include <string>
#include <utility>

// Estimates remaining time from a *recent* completion rate, not an average since the whole run
// started - categories run at wildly different speeds (paths/animations/cameras/misc typically
// finish in seconds, fmvs can take many minutes), so an average-since-start estimate stays
// dragged down by an early fast burst for a long time into a slow phase, making the ETA visibly
// climb instead of settle (confirmed live: ETA climbed from ~1min to 6min during fmv conversion).
// A short sliding window reflects only the current phase's real speed.
class EtaEstimator final
{
public:
    // Call once per frame with the current tick count (see SYS_GetTicks()) and the overall
    // completed/total counts (see ConversionProgress::Snapshot).
    void Update(u32 nowTicks, u64 totalCompleted, u64 totalItems);

    // "calculating..." until enough samples exist, "almost done" once totalCompleted reaches
    // totalItems, otherwise a formatted duration like "5m32s" or "1h23m".
    [[nodiscard]] const std::string& EtaString() const
    {
        return mEtaStr;
    }

private:
    static std::string FormatDuration(u32 ms);

    // (ticks, totalCompleted) samples, oldest first, spanning the last kWindowMs - see .cpp.
    std::deque<std::pair<u32, u64>> mSamples;
    std::string mEtaStr = "calculating...";
};
