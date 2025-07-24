#pragma once

#include "BaseGameObject.hpp"
#include "../data_conversion/AnimationConverter.hpp"

struct RGBA32;

class PalleteOverwriter final : public BaseGameObject
{
public:
    PalleteOverwriter(AnimationPal& pal, const RGBA32& colour);
    ~PalleteOverwriter();

    virtual void VScreenChanged() override;
    virtual void VRender(OrderingTable& /*ot*/) override;
    virtual void VUpdate() override;

private:
    AnimationPal& mPal;
    RGBA32 mColour = {};
    u32 mCurPalIndex = 0;
    u32 mNumEntriesToOverwrite = 0;
    bool mFirstUpdate = false;
    u32 mColoursToUpdatePerIteration = 0;
    u32 mPalDepth = 0;
public:
    bool mDone = false;
};
