#include "SligCycle.hpp"
#include <algorithm>

namespace Bot {

void SligCycle::Observe(u32 frame, s32 key, const std::vector<Seen>& sligs)
{
    if (key != mKey || frame != mFrame + 1 || sligs.size() != mPeriods.size())
    {
        mSeen.clear();
        mPeriods.assign(sligs.size(), 0);
        mKey = key;
    }
    mFrame = frame;
    if (sligs.empty())
    {
        return;
    }
    // (two of the longest periods and the stretch to match is all that is ever looked at)
    if (mSeen.size() >= static_cast<std::size_t>(4 * kMaxPeriod))
    {
        mSeen.erase(mSeen.begin(), mSeen.begin() + kMaxPeriod);
    }
    mSeen.push_back(sligs);
    const s32 n = static_cast<s32>(mSeen.size());
    for (s32 i = 0; i < Count(); i++)
    {
        if (mPeriods[i] > 0)
        {
            // Not what it did a period ago: something has put it off its beat (it shot, it heard something)
            if (n - 1 - mPeriods[i] < 0 || !(mSeen[n - 1][i] == mSeen[n - 1 - mPeriods[i]][i]))
            {
                mPeriods[i] = 0;
            }
        }
        if (mPeriods[i] == 0)
        {
            FindPeriod(i);
        }
    }
}

void SligCycle::FindPeriod(s32 slig)
{
    const s32 n = static_cast<s32>(mSeen.size());
    // The whole of the last period has to be what the one before it was: a shorter stretch is found again by
    // chance in what a Slig does after it has shot
    for (s32 period = kMinPeriod; period <= kMaxPeriod && n - 2 * period >= 0; period++)
    {
        bool same = true;
        // (and something has to happen in it: one that stands still matches itself at any shift)
        bool changed = false;
        for (s32 k = 0; k < period && same; k++)
        {
            same = mSeen[n - 1 - k][slig] == mSeen[n - 1 - k - period][slig];
            changed = changed || !(mSeen[n - 1 - k][slig] == mSeen[n - 1][slig]);
        }
        if (same && changed)
        {
            mPeriods[slig] = period;
            return;
        }
    }
}

bool SligCycle::Known() const
{
    if (mSeen.empty())
    {
        return false;
    }
    for (const s32 period : mPeriods)
    {
        if (period == 0)
        {
            return false;
        }
    }
    return true;
}

SligCycle::Seen SligCycle::Ahead(s32 slig, s32 frames) const
{
    const s32 n = static_cast<s32>(mSeen.size());
    const s32 period = mPeriods[slig];
    if (period == 0 || frames <= 0)
    {
        return mSeen[n - 1][slig];
    }
    // (the same phase, one period back or as many as it takes)
    const s32 back = period - (frames % period);
    return mSeen[n - 1 - (back == period ? 0 : back)][slig];
}

u32 SligCycle::SeenBy(s32 x, s32 frames) const
{
    u32 mask = 0;
    for (s32 i = 0; i < Count() && i < 5; i++)
    {
        for (s32 slack = -1; slack <= 1; slack++)
        {
            const Seen s = Ahead(i, frames + slack);
            if (!s.mBlind && (s.mFacingLeft ? x <= s.mX : x >= s.mX))
            {
                mask |= 1u << i;
            }
        }
    }
    return mask;
}

SligCycle::Move SligCycle::Through(const std::vector<Cell>& track, const std::vector<Shooter>& shooters, s32 within) const
{
    Move out;
    out.mSpotters = 99;
    const s32 n = static_cast<s32>(track.size());
    for (s32 delay = 0; delay <= within && out.mSpotters > 0; delay++)
    {
        // The frame each of them first sees him on
        s32 first[5] = {-1, -1, -1, -1, -1};
        for (s32 t = 0; t < n; t++)
        {
            if (track[t].mCover)
            {
                continue;
            }
            const u32 mask = SeenBy(track[t].mX, delay + t + 1 - kTrackSlack) | SeenBy(track[t].mX, delay + t + 1 + kTrackSlack) |
                             SeenBy(track[t].mX - kTrackAside, delay + t + 1) | SeenBy(track[t].mX + kTrackAside, delay + t + 1);
            for (s32 i = 0; i < 5; i++)
            {
                if (first[i] < 0 && (mask & (1u << i)))
                {
                    first[i] = t;
                }
            }
        }
        s32 bursts = 0;
        for (s32 i = 0; i < 5; i++)
        {
            const Shooter shooter = i < static_cast<s32>(shooters.size()) ? shooters[i] : Shooter();
            const s32 burst = shooter.mAfter + kShotFrame + kShotGap * std::max(shooter.mTimes, 1);
            for (s32 t = first[i]; t >= 0 && t < n;)
            {
                bursts++;
                // (and again, the next time it sees him after that)
                s32 next = -1;
                for (s32 u = t + burst; u < n && next < 0; u++)
                {
                    if (!track[u].mCover && (SeenBy(track[u].mX, delay + u + 1) & (1u << i)))
                    {
                        next = u;
                    }
                }
                t = next;
            }
        }
        if (bursts < out.mSpotters)
        {
            out.mSpotters = bursts;
            out.mDelay = delay;
        }
    }
    return out;
}

SligCycle::Move SligCycle::Cross(const std::vector<Cell>& cells, const std::function<bool(s32 x, bool rolling)>& covered) const
{
    Move out;
    const s32 n = static_cast<s32>(cells.size());
    out.mTo = n - 1;
    if (n < 2 || Count() == 0 || mSeen.empty())
    {
        return out;
    }
    s32 longest = 1;
    for (const s32 period : mPeriods)
    {
        longest = std::max(longest, period);
    }
    const s32 masks = 1 << std::min(Count(), 5);
    // (the wait for the start, the rolls, and as long again for waits on the way)
    const s32 frames = 2 * longest + n * (kRollBegin + kRollCell + kRollEnd) + 8;
    // Who sees him at each quarter cell of the way on each frame, rolling and crouched (worked out when asked)
    const s32 quarters = (n - 1) * kRollCell + 1;
    std::vector<s32> rollSeen(static_cast<std::size_t>(frames + 1) * quarters, -1);
    std::vector<s32> stillSeen(static_cast<std::size_t>(frames + 1) * n, -1);
    const auto xOf = [&](s32 quarter)
    {
        const s32 cell = quarter / kRollCell;
        const s32 part = quarter % kRollCell;
        return part == 0 ? cells[cell].mX : cells[cell].mX + (cells[cell + 1].mX - cells[cell].mX) * part / kRollCell;
    };
    const auto rolling = [&](s32 t, s32 quarter)
    {
        s32& seen = rollSeen[static_cast<std::size_t>(t) * quarters + quarter];
        if (seen < 0)
        {
            const s32 x = xOf(quarter);
            seen = covered(x, true) ? 0 : static_cast<s32>(SeenBy(x, t));
        }
        return static_cast<u32>(seen);
    };
    const auto still = [&](s32 t, s32 cell)
    {
        s32& seen = stillSeen[static_cast<std::size_t>(t) * n + cell];
        if (seen < 0)
        {
            seen = cells[cell].mCover || covered(cells[cell].mX, false) ? 0 : static_cast<s32>(SeenBy(cells[cell].mX, t));
        }
        return static_cast<u32>(seen);
    };
    // For each frame, cell and set of Sligs that have seen him: whether he can be there then, and the first
    // roll of the way that got him there (its frame and its cell; -1 while he has not rolled yet)
    struct Reached final
    {
        bool mIs = false;
        s32 mDelay = -1;
        s32 mTo = -1;
    };
    std::vector<Reached> reached(static_cast<std::size_t>(frames + 1) * n * masks);
    const auto at = [&](s32 t, s32 cell, s32 mask) -> Reached&
    {
        return reached[(static_cast<std::size_t>(t) * n + cell) * masks + mask];
    };
    at(0, 0, 0).mIs = true;
    s32 bestSpotters = 99;
    for (s32 t = 0; t <= frames; t++)
    {
        for (s32 mask = 0; mask < masks; mask++)
        {
            const Reached last = at(t, n - 1, mask);
            s32 spotters = 0;
            for (s32 b = mask; b != 0; b >>= 1)
            {
                spotters += b & 1;
            }
            if (last.mIs && spotters < bestSpotters)
            {
                bestSpotters = spotters;
                out.mDelay = last.mDelay;
                out.mTo = last.mTo;
                out.mSpotters = spotters;
            }
        }
        if (t == frames)
        {
            break;
        }
        for (s32 cell = 0; cell + 1 < n; cell++)
        {
            for (s32 mask = 0; mask < masks; mask++)
            {
                const Reached from = at(t, cell, mask);
                if (!from.mIs)
                {
                    continue;
                }
                // Wait a frame (at the start no longer than a period: it all comes round again)
                if (from.mDelay >= 0 || t < longest)
                {
                    Reached& next = at(t + 1, cell, static_cast<s32>(mask | still(t + 1, cell)));
                    if (!next.mIs)
                    {
                        next = from;
                    }
                }
                // Roll on, to each cell further along
                u32 seen = static_cast<u32>(mask);
                for (s32 f = 1; f <= kRollBegin; f++)
                {
                    seen |= t + f <= frames ? rolling(t + f, cell * kRollCell) : 0;
                }
                for (s32 to = cell + 1; to < n; to++)
                {
                    const s32 arrive = t + kRollBegin + (to - cell) * kRollCell;
                    if (arrive + kRollEnd > frames)
                    {
                        break;
                    }
                    for (s32 q = (to - 1) * kRollCell + 1; q <= to * kRollCell; q++)
                    {
                        seen |= rolling(t + kRollBegin + q - cell * kRollCell, q);
                    }
                    u32 ended = seen;
                    for (s32 f = 1; f <= kRollEnd; f++)
                    {
                        ended |= still(arrive + f, to);
                    }
                    Reached& next = at(arrive + kRollEnd, to, static_cast<s32>(ended));
                    if (!next.mIs)
                    {
                        next.mIs = true;
                        next.mDelay = from.mDelay >= 0 ? from.mDelay : t;
                        next.mTo = from.mDelay >= 0 ? from.mTo : to;
                    }
                }
            }
        }
    }
    if (bestSpotters == 99)
    {
        // (never: a straight roll always ends within the frames looked at)
        out.mDelay = 0;
        out.mTo = n - 1;
        out.mSpotters = Count();
    }
    return out;
}

} // namespace Bot
