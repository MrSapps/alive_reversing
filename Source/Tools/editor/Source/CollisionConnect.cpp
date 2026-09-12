#include "CollisionConnect.hpp"
#include "ResizeableArrowItem.hpp"
#include "CollisionObject.hpp"
#include <algorithm>
#include <array>

namespace
{
    long long DistanceSquared(int x1, int y1, int x2, int y2)
    {
        const long long dx = x1 - x2;
        const long long dy = y1 - y2;
        return dx * dx + dy * dy;
    }

    int Midpoint(int a, int b)
    {
        return (a + b) / 2;
    }

    // Which endpoint of each line is nearest the other line's - one of 4 possible pairings.
    struct Candidate final
    {
        bool aIsP1;
        bool bIsP1;
        long long distSq;
    };
}

CollisionConnectCommand::CollisionConnectCommand(ResizeableArrowItem* pLineA, ResizeableArrowItem* pLineB)
    : mLineA(pLineA)
    , mLineB(pLineB)
    , mBeforeA(CaptureState(pLineA))
    , mBeforeB(CaptureState(pLineB))
{
    setText("Connected collisions");

    // Of the 4 possible endpoint pairings between the two lines, connect whichever pair is
    // physically closest together - not just endpoints that already happen to coincide exactly
    // (the old behavior; two independently-drawn lines essentially never do, so this almost
    // never actually connected anything in practice).
    const std::array<Candidate, 4> candidates = {
        Candidate{true, true, DistanceSquared(mBeforeA.x1, mBeforeA.y1, mBeforeB.x1, mBeforeB.y1)},
        Candidate{true, false, DistanceSquared(mBeforeA.x1, mBeforeA.y1, mBeforeB.x2, mBeforeB.y2)},
        Candidate{false, true, DistanceSquared(mBeforeA.x2, mBeforeA.y2, mBeforeB.x1, mBeforeB.y1)},
        Candidate{false, false, DistanceSquared(mBeforeA.x2, mBeforeA.y2, mBeforeB.x2, mBeforeB.y2)},
    };

    const Candidate& nearest = *std::min_element(candidates.begin(), candidates.end(),
                                                  [](const Candidate& lhs, const Candidate& rhs)
                                                  { return lhs.distSq < rhs.distSq; });

    const int aX = nearest.aIsP1 ? mBeforeA.x1 : mBeforeA.x2;
    const int aY = nearest.aIsP1 ? mBeforeA.y1 : mBeforeA.y2;
    const int bX = nearest.bIsP1 ? mBeforeB.x1 : mBeforeB.x2;
    const int bY = nearest.bIsP1 ? mBeforeB.y1 : mBeforeB.y2;

    // Move both nearest endpoints to their shared midpoint, so the two lines' ends physically
    // marry up exactly - rather than just linking mNext/mPrevious while leaving a visible gap
    // (or overlap) between them.
    const int meetX = Midpoint(aX, bX);
    const int meetY = Midpoint(aY, bY);

    mAfterA = mBeforeA;
    mAfterB = mBeforeB;

    if (nearest.aIsP1)
    {
        mAfterA.x1 = meetX;
        mAfterA.y1 = meetY;
    }
    else
    {
        mAfterA.x2 = meetX;
        mAfterA.y2 = meetY;
    }

    if (nearest.bIsP1)
    {
        mAfterB.x1 = meetX;
        mAfterB.y1 = meetY;
    }
    else
    {
        mAfterB.x2 = meetX;
        mAfterB.y2 = meetY;
    }

    // Next/Previous form a directed chain: a line's Next is whichever line continues on from
    // its P2 endpoint, and its Previous is whichever line precedes its P1 endpoint. Whichever
    // endpoint of each line is the one that just got connected determines which field it (and,
    // symmetrically, the other line) gets linked through.
    if (nearest.aIsP1)
    {
        mAfterA.previous = mLineB->GetCollisionItem()->mId;
    }
    else
    {
        mAfterA.next = mLineB->GetCollisionItem()->mId;
    }

    if (nearest.bIsP1)
    {
        mAfterB.previous = mLineA->GetCollisionItem()->mId;
    }
    else
    {
        mAfterB.next = mLineA->GetCollisionItem()->mId;
    }
}

void CollisionConnectCommand::undo()
{
    ApplyState(mLineA, mBeforeA);
    ApplyState(mLineB, mBeforeB);
}

void CollisionConnectCommand::redo()
{
    ApplyState(mLineA, mAfterA);
    ApplyState(mLineB, mAfterB);
}

CollisionConnectCommand::LineState CollisionConnectCommand::CaptureState(ResizeableArrowItem* pLine)
{
    CollisionObject* line = pLine->GetCollisionItem();
    return {line->X1(), line->Y1(), line->X2(), line->Y2(), line->Next(), line->Previous()};
}

void CollisionConnectCommand::ApplyState(ResizeableArrowItem* pLine, const LineState& state)
{
    CollisionObject* line = pLine->GetCollisionItem();
    line->SetX1(state.x1);
    line->SetY1(state.y1);
    line->SetX2(state.x2);
    line->SetY2(state.y2);
    line->SetNext(state.next);
    line->SetPrevious(state.previous);
    line->CalculateLength();

    // The line's geometry was just changed directly on the model (not via a live drag), so the
    // graphics item's visual line needs an explicit refresh to match.
    pLine->SyncInternalObject();
}
