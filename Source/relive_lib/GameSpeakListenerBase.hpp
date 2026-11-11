#pragma once

#include "Types.hpp"

// What both games' GameSpeakListener keep: where in the events the character that listens has got to. A quicksave
// keeps it, see BaseGameObject::VListener
class GameSpeakListenerBase
{
public:
    s32 LastEventIdx() const { return mLastEventIdx; }
    void SetLastEventIdx(s32 idx) { mLastEventIdx = idx; }

protected:
    s32 mLastEventIdx = -1;
};
