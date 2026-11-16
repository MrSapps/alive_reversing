#include "BombCycle.hpp"
#include "NavGraph.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace Bot {

s32 BombCycle::Dist(const Pt& a, const Pt& b)
{
    const double dx = static_cast<double>(a.mX - b.mX);
    const double dy = static_cast<double>(a.mY - b.mY);
    return static_cast<s32>(std::sqrt(dx * dx + dy * dy) + 0.5);
}

const BombCycle::Bomb* BombCycle::Find(u32 id) const
{
    for (const Bomb& b : mBombs)
    {
        if (b.mNow.mId == id)
        {
            return &b;
        }
    }
    return nullptr;
}

std::vector<BombCycle::Track> BombCycle::Tracks(const NavInput& in)
{
    // (OGData::eLineTypes: eTrackLine_8)
    const s32 kTrackLine = 8;
    std::vector<Track> out;
    for (const NavFeature& f : in.mFeatures)
    {
        if (f.mKind != FeatureKind::eFlyingMine)
        {
            continue;
        }
        // The line it is put on: the one a ray 24 down and right of it meets first (MovingBomb's constructor)
        const NavLine* first = nullptr;
        double bestT = 2.0;
        for (const NavLine& l : in.mLines)
        {
            if (l.mType != kTrackLine)
            {
                continue;
            }
            const double rx = 24.0;
            const double ry = 24.0;
            const double sx = static_cast<double>(l.mX2 - l.mX1);
            const double sy = static_cast<double>(l.mY2 - l.mY1);
            const double den = rx * sy - ry * sx;
            if (den == 0.0)
            {
                continue;
            }
            const double qx = static_cast<double>(l.mX1 - f.mX1);
            const double qy = static_cast<double>(l.mY1 - f.mY1);
            const double t = (qx * sy - qy * sx) / den;
            const double u = (qx * ry - qy * rx) / den;
            if (t >= 0.0 && t <= 1.0 && u >= 0.0 && u <= 1.0 && t < bestT)
            {
                bestT = t;
                first = &l;
            }
        }
        Track track;
        track.mHalf = f.mHalf;
        if (!first)
        {
            // (on no track: it stays where it is put)
            track.mXs.push_back(f.mX1);
            track.mYs.push_back(f.mY1);
            out.push_back(track);
            continue;
        }
        track.mXs = {first->mX1, first->mX2};
        track.mYs = {first->mY1, first->mY2};
        // And the lines that go on from it, end to end
        for (s32 n = 0; n < 64; n++)
        {
            const s32 x = track.mXs.back();
            const s32 y = track.mYs.back();
            if (x == track.mXs[0] && y == track.mYs[0])
            {
                break;
            }
            const NavLine* next = nullptr;
            for (const NavLine& l : in.mLines)
            {
                if (l.mType == kTrackLine && l.mX1 == x && l.mY1 == y && !(l.mX2 == l.mX1 && l.mY2 == l.mY1))
                {
                    next = &l;
                }
            }
            if (!next)
            {
                break;
            }
            track.mXs.push_back(next->mX2);
            track.mYs.push_back(next->mY2);
        }
        // (and those that lead to it, where it doesn't come round)
        for (s32 n = 0; n < 64 && !(track.mXs.back() == track.mXs[0] && track.mYs.back() == track.mYs[0]); n++)
        {
            const NavLine* before = nullptr;
            for (const NavLine& l : in.mLines)
            {
                if (l.mType == kTrackLine && l.mX2 == track.mXs[0] && l.mY2 == track.mYs[0] && !(l.mX2 == l.mX1 && l.mY2 == l.mY1))
                {
                    before = &l;
                }
            }
            if (!before)
            {
                break;
            }
            track.mXs.insert(track.mXs.begin(), before->mX1);
            track.mYs.insert(track.mYs.begin(), before->mY1);
        }
        out.push_back(track);
    }
    return out;
}

void BombCycle::SetTracks(s32 key, const std::vector<Track>& tracks)
{
    mTrackKey = key;
    mTracks = tracks;
    mAlong.clear();
    for (const Track& track : mTracks)
    {
        std::vector<Pt> along;
        for (std::size_t i = 0; i < track.mXs.size(); i++)
        {
            along.push_back(Pt{track.mXs[i], track.mYs[i]});
            if (i + 1 < track.mXs.size())
            {
                const s32 dx = track.mXs[i + 1] - track.mXs[i];
                const s32 dy = track.mYs[i + 1] - track.mYs[i];
                const s32 steps = std::max(std::abs(dx), std::abs(dy)) / 4;
                for (s32 k = 1; k < steps; k++)
                {
                    along.push_back(Pt{track.mXs[i] + dx * k / steps, track.mYs[i] + dy * k / steps});
                }
            }
        }
        mAlong.push_back(std::move(along));
    }
    for (Bomb& bomb : mBombs)
    {
        bomb.mTrackAt = -1;
    }
}

void BombCycle::Observe(u32 frame, s32 key, const std::vector<Seen>& bombs)
{
    if (key != mKey)
    {
        mBombs.clear();
    }
    const bool gap = frame != mFrame + 1;
    mKey = key;
    mFrame = frame;
    for (Bomb& bomb : mBombs)
    {
        bomb.mSeen = false;
    }
    mSeen = 0;
    for (const Seen& seen : bombs)
    {
        Bomb* bomb = nullptr;
        for (Bomb& old : mBombs)
        {
            if (old.mNow.mId == seen.mId)
            {
                bomb = &old;
            }
        }
        if (!bomb)
        {
            mBombs.emplace_back();
            bomb = &mBombs.back();
            bomb->mNow = seen;
            bomb->mSeen = true;
            WatchBox(*bomb, seen, true);
            mSeen++;
            continue;
        }
        if (gap || bomb->mMissed)
        {
            // Not seen the frame before: where it is, and nothing of how it moves
            if (!bomb->mLapped)
            {
                bomb->mTrack.clear();
            }
            bomb->mSteps.clear();
            bomb->mStill = 0;
            bomb->mAtPace = false;
            bomb->mNow = seen;
            WatchBox(*bomb, seen, true);
            if (bomb->mLapped)
            {
                Locate(*bomb);
            }
        }
        else
        {
            Watch(*bomb, seen);
            WatchBox(*bomb, seen, false);
        }
        bomb->mSeen = true;
        bomb->mMissed = false;
        mSeen++;
    }
    for (std::size_t i = 0; i < mBombs.size();)
    {
        Bomb& bomb = mBombs[i];
        if (!bomb.mSeen)
        {
            bomb.mMissed = true;
            if (!bomb.mLapped)
            {
                mBombs.erase(mBombs.begin() + static_cast<std::ptrdiff_t>(i));
                continue;
            }
        }
        else if (bomb.mTrackAt < 0 && mTrackKey == mKey)
        {
            // The track of the map it is on
            const Pt at{bomb.mNow.mX / 256, bomb.mNow.mY / 256};
            for (std::size_t t = 0; t < mAlong.size() && bomb.mTrackAt < 0; t++)
            {
                for (const Pt& p : mAlong[t])
                {
                    if (std::abs(p.mX - at.mX) <= 4 && std::abs(p.mY - at.mY) <= 4)
                    {
                        bomb.mTrackAt = static_cast<s32>(t);
                        break;
                    }
                }
            }
        }
        i++;
    }
}

void BombCycle::Watch(Bomb& bomb, const Seen& seen)
{
    const Pt was{bomb.mNow.mX, bomb.mNow.mY};
    const Pt now{seen.mX, seen.mY};
    const s32 step = Dist(was, now);
    bomb.mStill = step == 0 ? bomb.mStill + 1 : 0;
    bomb.mNow = seen;
    if (bomb.mLapped)
    {
        // (round a corner it moves less as the crow flies)
        bomb.mAtPace = step * 4 >= bomb.mPace * 3;
        Locate(bomb);
        return;
    }
    if (step == 0)
    {
        bomb.mTrack.clear();
        bomb.mSteps.clear();
        return;
    }
    // At its pace: it moved as far over the last four frames as over the four before
    bomb.mSteps.push_back(step);
    if (bomb.mTrack.empty())
    {
        const std::size_t n = bomb.mSteps.size();
        if (n < 8)
        {
            return;
        }
        s32 late = 0;
        s32 early = 0;
        for (std::size_t i = 0; i < 4; i++)
        {
            late += bomb.mSteps[n - 1 - i];
            early += bomb.mSteps[n - 5 - i];
        }
        if (std::abs(late - early) > 256)
        {
            return;
        }
        bomb.mTrack.push_back(was);
        bomb.mTrack.push_back(now);
        return;
    }
    // Round once it passes where the lap began, going the way it went then
    const std::size_t n = bomb.mTrack.size();
    const Pt& first = bomb.mTrack[0];
    const Pt& second = bomb.mTrack[1];
    const Pt& last = bomb.mTrack[n - 1];
    const double sx = static_cast<double>(now.mX - last.mX);
    const double sy = static_cast<double>(now.mY - last.mY);
    const double len2 = sx * sx + sy * sy;
    const double along = (static_cast<double>(first.mX - last.mX) * sx + static_cast<double>(first.mY - last.mY) * sy) / len2;
    const double px = static_cast<double>(last.mX) + along * sx - static_cast<double>(first.mX);
    const double py = static_cast<double>(last.mY) + along * sy - static_cast<double>(first.mY);
    const double sameWay = sx * static_cast<double>(second.mX - first.mX) + sy * static_cast<double>(second.mY - first.mY);
    if (n >= 6 && along > 0.0 && along <= 1.0 && px * px + py * py < 3.0 * 256.0 * 3.0 * 256.0 && sameWay > 0.0)
    {
        bomb.mLapped = true;
        bomb.mClose = static_cast<s32>(along * 256.0);
        s32 sum = 0;
        for (std::size_t i = 1; i < n; i++)
        {
            sum += Dist(bomb.mTrack[i - 1], bomb.mTrack[i]);
        }
        bomb.mPace = sum / static_cast<s32>(n - 1);
        bomb.mAtPace = true;
        bomb.mSteps.clear();
        Locate(bomb);
        return;
    }
    bomb.mTrack.push_back(now);
    if (bomb.mTrack.size() > static_cast<std::size_t>(kMaxLap))
    {
        bomb.mTrack.clear();
        bomb.mSteps.clear();
    }
}

// Where on its lap it is: the nearest place of it
void BombCycle::Locate(Bomb& bomb)
{
    const Pt now{bomb.mNow.mX, bomb.mNow.mY};
    const std::size_t n = bomb.mTrack.size();
    double best = -1.0;
    for (std::size_t i = 0; i < n; i++)
    {
        const Pt& a = bomb.mTrack[i];
        const Pt& b = bomb.mTrack[(i + 1) % n];
        const double sx = static_cast<double>(b.mX - a.mX);
        const double sy = static_cast<double>(b.mY - a.mY);
        const double len2 = sx * sx + sy * sy;
        double along = len2 > 0.0 ? (static_cast<double>(now.mX - a.mX) * sx + static_cast<double>(now.mY - a.mY) * sy) / len2 : 0.0;
        along = std::max(0.0, std::min(1.0, along));
        const double px = static_cast<double>(a.mX) + along * sx - static_cast<double>(now.mX);
        const double py = static_cast<double>(a.mY) + along * sy - static_cast<double>(now.mY);
        const double d = px * px + py * py;
        if (best < 0.0 || d < best)
        {
            best = d;
            const s32 span = i + 1 == n ? bomb.mClose : 256;
            bomb.mPhase = static_cast<s32>(i) * 256 + static_cast<s32>(along * static_cast<double>(span));
        }
    }
}

BombCycle::Pt BombCycle::Place(const Bomb& bomb, s32 frames)
{
    if (!bomb.mLapped || bomb.mStill > 0)
    {
        return Pt{bomb.mNow.mX, bomb.mNow.mY};
    }
    const s32 n = static_cast<s32>(bomb.mTrack.size());
    const s32 lap = (n - 1) * 256 + bomb.mClose;
    s32 phase = static_cast<s32>((static_cast<s64>(bomb.mPhase) + static_cast<s64>(frames) * 256) % lap);
    if (phase < 0)
    {
        phase += lap;
    }
    const s32 i = std::min(phase / 256, n - 1);
    const s32 span = i == n - 1 ? bomb.mClose : 256;
    const s32 part = phase - i * 256;
    const Pt& a = bomb.mTrack[i];
    const Pt& b = bomb.mTrack[(i + 1) % n];
    if (span <= 0)
    {
        return a;
    }
    return Pt{a.mX + static_cast<s32>(static_cast<s64>(b.mX - a.mX) * part / span), a.mY + static_cast<s32>(static_cast<s64>(b.mY - a.mY) * part / span)};
}

BombCycle::Box BombCycle::BoxAt(const Bomb& bomb, const Pt& at, s32 frames)
{
    const s32 x = at.mX / 256;
    const s32 y = at.mY / 256;
    const s32 n = static_cast<s32>(bomb.mBoxes.size());
    if (bomb.mBoxPeriod > 0 && frames >= 0)
    {
        // As it was the last time its animation was where it will be then
        const Seen& then = bomb.mBoxes[static_cast<std::size_t>(n - 1 - (bomb.mBoxPeriod - frames % bomb.mBoxPeriod) % bomb.mBoxPeriod)];
        return Box{x + then.mLeft, y + then.mTop, x + then.mRight, y + then.mBottom};
    }
    Box box{x + bomb.mNow.mLeft, y + bomb.mNow.mTop, x + bomb.mNow.mRight, y + bomb.mNow.mBottom};
    for (const Seen& was : bomb.mBoxes)
    {
        box.mX1 = std::min(box.mX1, x + was.mLeft);
        box.mY1 = std::min(box.mY1, y + was.mTop);
        box.mX2 = std::max(box.mX2, x + was.mRight);
        box.mY2 = std::max(box.mY2, y + was.mBottom);
    }
    return box;
}

void BombCycle::WatchBox(Bomb& bomb, const Seen& seen, bool fresh)
{
    const std::size_t kKept = 160;
    if (fresh)
    {
        bomb.mBoxes.clear();
        bomb.mBoxPeriod = 0;
    }
    bomb.mBoxes.push_back(seen);
    if (bomb.mBoxes.size() > kKept)
    {
        bomb.mBoxes.erase(bomb.mBoxes.begin());
    }
    const auto same = [&bomb](std::size_t i, std::size_t k)
    {
        const Seen& a = bomb.mBoxes[i];
        const Seen& b = bomb.mBoxes[k];
        return a.mLeft == b.mLeft && a.mTop == b.mTop && a.mRight == b.mRight && a.mBottom == b.mBottom;
    };
    // The fewest frames after which the last two rounds of it were the same
    const std::size_t n = bomb.mBoxes.size();
    bomb.mBoxPeriod = 0;
    for (std::size_t period = 1; period * 3 <= n && period <= 50; period++)
    {
        bool round = true;
        for (std::size_t i = n - 2 * period; i < n && round; i++)
        {
            round = same(i, i - period);
        }
        if (round)
        {
            bomb.mBoxPeriod = static_cast<s32>(period);
            break;
        }
    }
}

bool BombCycle::Known(u32 id) const
{
    const Bomb* bomb = Find(id);
    return bomb && Ready(*bomb);
}

bool BombCycle::TrackMeets(std::size_t track, bool half, const Box& box) const
{
    if (mTracks[track].mHalf != half)
    {
        return false;
    }
    // (its box about where it is, as it mostly is: 11 to the left, 13 to the right, from 26 to 4 above)
    const s32 scale = half ? 2 : 1;
    const Box wide{box.mX1 - 13 / scale, box.mY1 + 4 / scale, box.mX2 + 11 / scale, box.mY2 + 26 / scale};
    for (const Pt& p : mAlong[track])
    {
        if (p.mX >= wide.mX1 && p.mX <= wide.mX2 && p.mY >= wide.mY1 && p.mY <= wide.mY2)
        {
            return true;
        }
    }
    return false;
}

bool BombCycle::Watching(bool half, const Box& box) const
{
    for (const Bomb& b : mBombs)
    {
        if (!b.mSeen || Ready(b) || b.mNow.mHalf != half)
        {
            continue;
        }
        if (b.mTrackAt >= 0 ? TrackMeets(static_cast<std::size_t>(b.mTrackAt), half, box) : Ever(half, box))
        {
            return true;
        }
    }
    return false;
}

bool BombCycle::Missing(bool half, const Box& box) const
{
    for (std::size_t t = 0; mTrackKey == mKey && t < mTracks.size(); t++)
    {
        if (!TrackMeets(t, half, box))
        {
            continue;
        }
        bool on = false;
        for (const Bomb& b : mBombs)
        {
            on = on || (b.mSeen && b.mTrackAt == static_cast<s32>(t));
        }
        if (!on)
        {
            return true;
        }
    }
    return false;
}

s32 BombCycle::Lap256(u32 id) const
{
    const Bomb* bomb = Find(id);
    return bomb && bomb->mLapped ? (static_cast<s32>(bomb->mTrack.size()) - 1) * 256 + bomb->mClose : 0;
}

bool BombCycle::At(u32 id, s32 frames, Box& box) const
{
    const Bomb* bomb = Find(id);
    if (!bomb)
    {
        return false;
    }
    box = BoxAt(*bomb, Place(*bomb, frames), frames);
    return true;
}

bool BombCycle::Hits(bool half, s32 frames, const Box& box) const
{
    for (const Bomb& b : mBombs)
    {
        if (b.mSeen && b.mNow.mHalf == half && BoxAt(b, Place(b, frames), frames).Meets(box))
        {
            return true;
        }
    }
    return false;
}

bool BombCycle::Ever(bool half, const Box& box) const
{
    for (std::size_t t = 0; mTrackKey == mKey && t < mTracks.size(); t++)
    {
        if (TrackMeets(t, half, box))
        {
            return true;
        }
    }
    for (const Bomb& b : mBombs)
    {
        if (!b.mSeen || b.mNow.mHalf != half || b.mTrackAt >= 0)
        {
            continue;
        }
        if (!b.mLapped)
        {
            const Box wide{box.mX1 - 150, box.mY1 - 150, box.mX2 + 150, box.mY2 + 150};
            if (BoxAt(b, Pt{b.mNow.mX, b.mNow.mY}, -1).Meets(b.mStill >= kStill ? box : wide))
            {
                return true;
            }
            continue;
        }
        for (const Pt& p : b.mTrack)
        {
            if (BoxAt(b, p, -1).Meets(box))
            {
                return true;
            }
        }
    }
    return false;
}

s32 BombCycle::Next(bool half, const Box& box, s32 from, s32 within) const
{
    for (s32 t = from; t <= from + within; t++)
    {
        if (Hits(half, t, box))
        {
            return t - from;
        }
    }
    return -1;
}

} // namespace Bot
