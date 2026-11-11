#pragma once

#include "ObjectTypes.hpp"
#include "data_conversion/guid.hpp"
#include "FixedPoint.hpp"
#include <cstring>

struct SaveStateBase
{
    // size is the whole derived struct. It's zeroed first so padding is always 0: saves are copied
    // out byte for byte, and padding left as stack garbage made two saves of the same state differ.
    SaveStateBase(ReliveTypes type, u32 size)
    {
        std::memset(static_cast<void*>(this), 0, size);
        mType = type;
        mSize = size;
    }
    ReliveTypes mType;
    u32 mSize;
    // The id the object had when it was saved, filled in by BaseGameObject::SaveState. A restore gives the
    // object it makes the same id back, so the ids the saves of other objects hold stay right.
    // Not valid for saves that didn't have one, e.g. converted from the original game.
    Guid mObjectId;
    // For a BaseAliveGameObject, the TLV its path iteration was at (BaseAliveGameObjectPathTLV): the next search
    // for a TLV it overlaps carries on from it
    Guid mPathTlvId;
    // For a BaseAliveGameObject, BaseAliveGameObjectLastLineYPos. The saves of the original game have it as a whole number.
    FP mLastLineYPos;
};
