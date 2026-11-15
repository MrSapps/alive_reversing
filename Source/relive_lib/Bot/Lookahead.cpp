#include "Lookahead.hpp"
#include <algorithm>
#include <cstdlib>

namespace Bot {

s32 Lookahead::Count()
{
    return kWindow / kFinest;
}

s32 Lookahead::Delay(s32 index)
{
    if (index < 0)
    {
        return -1;
    }
    // The first pass has every multiple of the coarsest step, each pass after it the odd multiples of half the
    // step before
    const s32 first = kWindow / kCoarsest;
    if (index < first)
    {
        return index * kCoarsest;
    }
    index -= first;
    for (s32 step = kCoarsest / 2; step >= kFinest; step /= 2)
    {
        const s32 count = kWindow / (2 * step);
        if (index < count)
        {
            return step + index * 2 * step;
        }
        index -= count;
    }
    return -1;
}

s32 Lookahead::Failed(Stretch& stretch, s32 frames, s32 place)
{
    if (frames <= stretch.mDelay)
    {
        // (sooner than any before: this try held back less than the ceiling there was)
        stretch.mCeiling = std::max(frames, 0);
    }
    else
    {
        const s32 after = frames - stretch.mDelay;
        if (stretch.mAlike > 0 && std::abs(after - stretch.mAfter) <= 1 && place == stretch.mPlace)
        {
            stretch.mAlike++;
        }
        else
        {
            stretch.mAlike = 1;
            stretch.mAfter = after;
            stretch.mPlace = place;
        }
        if (stretch.mAlike >= kAlike)
        {
            stretch.mNext = Count();
            return -1;
        }
    }
    for (;;)
    {
        const s32 delay = Delay(stretch.mNext);
        if (delay < 0)
        {
            return -1;
        }
        stretch.mNext++;
        if (delay < stretch.mCeiling)
        {
            stretch.mDelay = delay;
            return delay;
        }
    }
}

s32 Lookahead::Again(Stretch& stretch)
{
    stretch.mRounds++;
    stretch.mAlike = 0;
    stretch.mNext = 1;
    stretch.mDelay = 0;
    return 0;
}

} // namespace Bot
