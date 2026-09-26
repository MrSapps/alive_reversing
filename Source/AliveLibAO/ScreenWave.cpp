#include "stdafx_ao.h"
#include "ScreenWave.hpp"
#include "Map.hpp"
#include "Math.hpp"
#include "../relive_lib/GameObjects/ScreenManager.hpp"
#include "../relive_lib/PsxDisplay.hpp"
#include "../AliveLibAE/PsxRender.hpp"

#undef min
#undef max

#include <algorithm>

namespace AO {

ScreenWave::ScreenWave(FP xpos, FP ypos, Layer layer, FP width, FP speed, s32 radius, ResourceManagerWrapper& resMan, BaseMap& map)
    : BaseGameObject(true, 0, resMan, map)
    , mLayer(layer)
    , mXPos(xpos)
    , mYPos(ypos)
    , mLevel(map.mCurrentLevel)
    , mPath(map.mCurrentPath)
    , mSpeed(speed)
{
    SetType(ReliveTypes::eScreenWave);
    SetDrawable(true);
    gObjListDrawables->Push_Back(this);

    mScreenX = static_cast<s16>(FP_GetExponent(xpos) - FP_GetExponent(gScreenManager->CamXPos()));
    mScreenY = static_cast<s16>(FP_GetExponent(ypos) - FP_GetExponent(gScreenManager->CamYPos()));

    // It's gone once it has grown past the screen's furthest corner (measured along x plus along y)
    const s16 right = gPsxDisplay.mWidth;
    const s16 bottom = gPsxDisplay.mHeight;
    const s16 cornerDistances[4] = {
        static_cast<s16>(std::abs(mScreenX) + std::abs(mScreenY)),
        static_cast<s16>(std::abs(mScreenX - right) + std::abs(mScreenY)),
        static_cast<s16>(std::abs(mScreenX) + std::abs(mScreenY - bottom)),
        static_cast<s16>(std::abs(mScreenX - right) + std::abs(mScreenY - bottom))};
    mMaxRadius = *std::max_element(std::begin(cornerDistances), std::end(cornerDistances));
    if (radius > 0)
    {
        mMaxRadius = std::min(mMaxRadius, static_cast<s16>(radius));
    }

    // Along each spoke, the source points are evenly spread across the ring, and the destination
    // points bulge out in the middle of it: that's what bends the picture.
    const FP sourceStep = width / FP_FromInteger(4);
    const FP halfWidth = width / FP_FromInteger(2);

    u8 spokeAngle = 0;
    for (s32 spoke = 0; spoke < kSpokes; spoke++)
    {
        const FP dirX = Math_Sine(spokeAngle);
        const FP dirY = Math_Cosine(spokeAngle);

        u8 bulgeAngle = 128;
        for (s32 point = 0; point < kPointsPerSpoke; point++)
        {
            const FP sourceDistance = FP_FromInteger(point) * sourceStep;
            mSource[spoke][point].x = sourceDistance * dirX;
            mSource[spoke][point].y = sourceDistance * dirY;

            const FP destDistance = (Math_Sine(bulgeAngle) * halfWidth) + halfWidth;
            mDest[spoke][point].x = destDistance * dirX;
            mDest[spoke][point].y = destDistance * dirY;

            bulgeAngle -= 32;
        }

        mVelocity[spoke].x = dirX * speed;
        mVelocity[spoke].y = dirY * speed;
        spokeAngle += 256 / kSpokes;
    }
}

ScreenWave::~ScreenWave()
{
    gObjListDrawables->Remove_Item(this);
}

void ScreenWave::VScreenChanged()
{
    if (mMap.LevelChanged() || mMap.PathChanged())
    {
        SetDead(true);
    }
}

void ScreenWave::VUpdate()
{
    if (FP_GetExponent(mRadius) > mMaxRadius)
    {
        SetDead(true);
        return;
    }

    mRadius += mSpeed;
    for (s32 spoke = 0; spoke < kSpokes; spoke++)
    {
        for (s32 point = 0; point < kPointsPerSpoke; point++)
        {
            mSource[spoke][point].x += mVelocity[spoke].x;
            mSource[spoke][point].y += mVelocity[spoke].y;
            mDest[spoke][point].x += mVelocity[spoke].x;
            mDest[spoke][point].y += mVelocity[spoke].y;
        }
    }
}

void ScreenWave::VRender(OrderingTable& ot)
{
    if (!mMap.Is_Point_In_Current_Camera(mLevel, mPath, mXPos, mYPos, 0))
    {
        return;
    }

    // Points are in PSX screen coordinates, the quads in PC ones
    auto toScreenX = [this](const FP_Point& p)
    {
        return static_cast<s16>(PsxToPCX(static_cast<s16>(mScreenX + FP_GetExponent(p.x)), 11));
    };
    auto toScreenY = [this](const FP_Point& p)
    {
        return static_cast<s16>(mScreenY + FP_GetExponent(p.y));
    };

    for (s32 spoke = 0; spoke < kSpokes; spoke++)
    {
        const s32 nextSpoke = (spoke + 1) % kSpokes;
        for (s32 quad = 0; quad < kQuadsPerSpoke; quad++)
        {
            const FP_Point* dest[4] = {&mDest[spoke][quad], &mDest[spoke][quad + 1], &mDest[nextSpoke][quad], &mDest[nextSpoke][quad + 1]};
            const FP_Point* source[4] = {&mSource[spoke][quad], &mSource[spoke][quad + 1], &mSource[nextSpoke][quad], &mSource[nextSpoke][quad + 1]};

            s16 x[4] = {};
            s16 y[4] = {};
            for (s32 i = 0; i < 4; i++)
            {
                x[i] = toScreenX(*dest[i]);
                y[i] = toScreenY(*dest[i]);
            }

            const bool onScreen = *std::max_element(x, x + 4) >= 0 && *std::max_element(y, y + 4) >= 0 && *std::min_element(x, x + 4) < gPsxDisplay.mWidth && *std::min_element(y, y + 4) < gPsxDisplay.mHeight;
            if (!onScreen)
            {
                continue;
            }

            Prim_ScreenWave& prim = mQuads[spoke][quad];
            prim.SetXY0(x[0], y[0]);
            prim.SetXY1(x[1], y[1]);
            prim.SetXY2(x[2], y[2]);
            prim.SetXY3(x[3], y[3]);
            for (u32 i = 0; i < 4; i++)
            {
                // Source rows are kept to 0-255, so a piece reaching past the top or bottom of the
                // screen draws from right across it: that smears a band of the picture along the
                // edge, which is part of how the ripple looks
                prim.SetSource(i, toScreenX(*source[i]), static_cast<u8>(toScreenY(*source[i])));
            }
            ot.Add(mLayer, &prim);
        }
    }
}

} // namespace AO
