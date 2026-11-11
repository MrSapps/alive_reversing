#pragma once

#include "SaveStateBase.hpp"
#include "data_conversion/guid.hpp"

// The order of the objects in one of the lists that are gone through besides the one they update in. The drawing goes
// through gObjListDrawables, and takes random numbers (a font's flicker) in the order of it; gBaseAliveGameObjects is
// looked through for who is near. A list closes up over an object taken out of it with the last, so the order
// depends on all that came before, and a restored world needs it from the save.
struct ListOrderSaveState final : public SaveStateBase
{
    ListOrderSaveState()
        : SaveStateBase(ReliveTypes::eListOrder, sizeof(*this))
    { }

    enum class List : s32
    {
        eDrawables = 0,
        eAliveObjects = 1,
        // The owner of each animation: an object with more than one is in it as many times, in their order
        eAnimations = 2,
        ePlatforms = 3,
    };

    static constexpr s32 kMaxIds = 1024;

    List mList;
    s32 mCount;
    Guid mIds[kMaxIds];
    // For eAnimations: where in its owner the animation is (-1 for one that isn't in it), as an owner can have several
    s32 mOffsets[kMaxIds];
};
