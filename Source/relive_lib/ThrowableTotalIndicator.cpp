#include "stdafx.h"
#include "ThrowableTotalIndicator.hpp"
#include "Events.hpp"
#include "GameObjects/ScreenManager.hpp"
#include "PsxDisplay.hpp"
#include "../AliveLibAE/stdlib.hpp"

struct GlyphPoint final
{
    s32 x = 0;
    s32 y = 0;

    GlyphPoint operator+(const GlyphPoint& other)
    {
        GlyphPoint result = { x + other.x, y + other.y };
        return result;
    }

    GlyphPoint operator-(const GlyphPoint& other)
    {
        GlyphPoint result = { x - other.x, y - other.y };
        return result;
    }
};

struct GlyphLine final
{
    GlyphPoint p1 = {0,0};
    GlyphPoint p2 = {0,0};
};

struct Glyph final
{
    u32 mPointsCount = 0;
    const GlyphLine* mPoints = nullptr;
};

GlyphPoint TOP_LEFT = {-5, -4};
GlyphPoint TOP_RIGHT = {5, -4};

GlyphPoint MIDDLE_LEFT = {-5, 0};
GlyphPoint MIDDLE_RIGHT = {5, 0};

GlyphPoint BOTTOM_LEFT = {-5, 4};
GlyphPoint BOTTOM_RIGHT = {5, 4};

GlyphPoint TOP_MIDDLE = {0, -4};
GlyphPoint BOTTOM_MIDDLE = {0, 4};

GlyphPoint OFFSET_1X = {1, 0};
GlyphPoint OFFSET_1Y = {0, 1};

static const GlyphLine kNumber_0[] = 
{
    { {-3, -4},{ 3, -4} },
    { {3, -3} ,{ 3, 3 }},
    { {3, 4}  ,{-3, 4 }},
    { {-3, 3} ,{-3, -3}}
};

static const GlyphLine kNumber_1[] = 
{
    {TOP_MIDDLE + OFFSET_1X + OFFSET_1X, BOTTOM_MIDDLE + OFFSET_1X + OFFSET_1X}
};

static const GlyphLine kNumber_2[] = 
{
    {TOP_LEFT, TOP_RIGHT},
    {TOP_RIGHT + OFFSET_1Y, MIDDLE_RIGHT - OFFSET_1Y},
    {MIDDLE_RIGHT, MIDDLE_LEFT},
    {MIDDLE_LEFT + OFFSET_1Y,BOTTOM_LEFT - OFFSET_1Y},
    {BOTTOM_LEFT, BOTTOM_RIGHT},
};

static const GlyphLine kNumber_3[] = 
{
    {TOP_LEFT, TOP_RIGHT},
    {MIDDLE_LEFT + OFFSET_1X, MIDDLE_RIGHT - OFFSET_1X},
    {BOTTOM_LEFT, BOTTOM_RIGHT},
    {TOP_RIGHT + OFFSET_1Y, BOTTOM_RIGHT - OFFSET_1Y},
};

static const GlyphLine kNumber_4[] = 
{
    {TOP_LEFT,MIDDLE_LEFT - OFFSET_1Y},
    {MIDDLE_LEFT, MIDDLE_RIGHT - OFFSET_1X},
    {TOP_RIGHT, BOTTOM_RIGHT},
};

static const GlyphLine kNumber_5[] = {
    {TOP_RIGHT, TOP_LEFT},
    {TOP_LEFT + OFFSET_1Y,MIDDLE_LEFT - OFFSET_1Y},
    {MIDDLE_LEFT, MIDDLE_RIGHT},
    {MIDDLE_RIGHT + OFFSET_1Y, BOTTOM_RIGHT - OFFSET_1Y},
    {BOTTOM_RIGHT, BOTTOM_LEFT},
};

static const GlyphLine kNumber_6[] = 
{
    {TOP_RIGHT, TOP_LEFT},
    {TOP_LEFT + OFFSET_1Y, BOTTOM_LEFT - OFFSET_1Y},
    {BOTTOM_LEFT, BOTTOM_RIGHT},
    {BOTTOM_RIGHT - OFFSET_1Y,MIDDLE_RIGHT + OFFSET_1Y},
    {MIDDLE_RIGHT,MIDDLE_LEFT + OFFSET_1X},
};

static const GlyphLine kNumber_7[] = 
{
    {TOP_LEFT, TOP_RIGHT},
    {TOP_RIGHT + OFFSET_1Y, BOTTOM_MIDDLE},
};

static const GlyphLine kNumber_8[] = 
{
    {TOP_LEFT, TOP_RIGHT},
    {MIDDLE_LEFT + OFFSET_1X,MIDDLE_RIGHT - OFFSET_1X},
    {BOTTOM_LEFT, BOTTOM_RIGHT},
    {TOP_RIGHT + OFFSET_1Y, BOTTOM_RIGHT - OFFSET_1Y},
    {TOP_LEFT + OFFSET_1Y, BOTTOM_LEFT - OFFSET_1Y,},
};

static const GlyphLine kNumber_9[] = 
{
    {BOTTOM_RIGHT,TOP_RIGHT + OFFSET_1Y},
    {TOP_LEFT, TOP_RIGHT},
    {TOP_LEFT + OFFSET_1Y,MIDDLE_LEFT - OFFSET_1Y},
    {MIDDLE_LEFT,MIDDLE_RIGHT - OFFSET_1X},
};

static const GlyphLine kInfinity[] = 
{
    {{-3, -2}, {-5, 0}},
    {{-5, 1}, {-3, 3}},
    {{-2, 3}, {2, -2}},
    {{3, -2}, {5, 0}},
    {{5, 1}, {3, 3}},
    {{2, 3}, {-2, -2}}
};

static const GlyphLine kCheckpoint[] = 
{
    {{0, -6}, {1, -6}},
    {{12, 0}, {13, 0}},
    {{0, 6}, {1, 6}},
    {{-11, 0}, {-12, 0}},
    {{2, -5}, {11, -1}},
    {{11, 1}, {2, 5}},
    {{-1, 5},{-10, 1}},
    {{-10, -1},{-1, -5}}
};

static const Glyph sGlyphs[] = {
    {ALIVE_COUNTOF(kNumber_0), kNumber_0},
    {ALIVE_COUNTOF(kNumber_1), kNumber_1},
    {ALIVE_COUNTOF(kNumber_2), kNumber_2},
    {ALIVE_COUNTOF(kNumber_3), kNumber_3},
    {ALIVE_COUNTOF(kNumber_4), kNumber_4},
    {ALIVE_COUNTOF(kNumber_5), kNumber_5},
    {ALIVE_COUNTOF(kNumber_6), kNumber_6},
    {ALIVE_COUNTOF(kNumber_7), kNumber_7},
    {ALIVE_COUNTOF(kNumber_8), kNumber_8},
    {ALIVE_COUNTOF(kNumber_9), kNumber_9},
    {ALIVE_COUNTOF(kInfinity), kInfinity},
    {ALIVE_COUNTOF(kCheckpoint), kCheckpoint},
};

u16 ThrowableTotalIndicator::mThrowableIndicatorExists = 0;

bool ThrowableTotalIndicator::IsExists() { return mThrowableIndicatorExists != 0; }

ThrowableTotalIndicator::ThrowableTotalIndicator(FP xpos, FP ypos, Layer layer, FP /*scale*/, s32 count, bool bFade)
    : BaseGameObject(true, 0)
{
    SetDrawable(true);
    SetType(ReliveTypes::eThrowableTotalIndicator);

    gObjListDrawables->Push_Back(this);

    mStartXPos = xpos;
    mStartYPos = ypos;

    mXPos = xpos;
    mYPos = ypos;

    mSpeedX = FP_FromInteger(0);

    mFade = bFade;

    if (mFade)
    {
        mSpeedY = FP_FromDouble(-0.7);
    }
    else
    {
        mSpeedY = FP_FromInteger(0);
    }

    mSpriteScale = FP_FromInteger(1); // OG bug - should be using scale ??
    mOtLayer = layer;

    mRGB.SetRGB(0, 0, 0);

    if (mFade)
    {
        mState = ThrowableTotalIndicatorState::eFading;
    }
    else
    {
        mState = ThrowableTotalIndicatorState::eCreated;
    }

    if (count < 0 || count > 10)
    {
        mNumToShow = 11;
    }
    else
    {
        mNumToShow = static_cast<s16>(count);
    }

    if (bFade)
    {
        mThrowableIndicatorExists++;
    }
}

ThrowableTotalIndicator::~ThrowableTotalIndicator()
{
    if (GetDrawable())
    {
        gObjListDrawables->Remove_Item(this);
    }

    if (mFade)
    {
        mThrowableIndicatorExists--;
    }
}

void ThrowableTotalIndicator::VScreenChanged()
{
    SetDead(true);
}

extern s16 gNumCamSwappers;

void ThrowableTotalIndicator::VUpdate()
{
    if (EventGet(kEventDeathReset))
    {
        SetDead(true);
    }

    if (gNumCamSwappers != 0)
    {
        return;
    }

    switch (mState)
    {
        case ThrowableTotalIndicatorState::eCreated:
        {
            mXPos = mStartXPos - (FP_FromInteger(12) * Math_Sine(static_cast<u8>(2 * sGnFrame)));
            mYPos = (FP_FromInteger(12) * Math_Cosine(static_cast<u8>(2 * sGnFrame))) + mStartYPos;

            const s16 rgb = FP_GetExponent(FP_FromInteger(48) * Math_Sine(static_cast<u8>(3 * sGnFrame))) + 80;

            mRGB.SetRGB(rgb, rgb, rgb);
        }
        break;

        case ThrowableTotalIndicatorState::eFading:
            if (mYPos >= (mStartYPos - FP_FromInteger(20)))
            {
                if (mRGB.r < 70 && mRGB.g < 90 && mRGB.b < 20)
                {
                    mRGB.r += 14;
                    mRGB.g += 18;
                    mRGB.b += 4;
                }

                mXPos += mSpeedX;
                mYPos += mSpeedY;
                return;
            }
            else
            {
                mState = ThrowableTotalIndicatorState::eVanishing;
            }
            break;

        case ThrowableTotalIndicatorState::eVanishing:
            if (mRGB.r < 7 && mRGB.g < 7 && mRGB.b < 7)
            {
                SetDead(true);
                return;
            }

            mRGB.r -= 7;
            mRGB.g -= 9;
            mRGB.b -= 2;

            mXPos += mSpeedX;
            mYPos += mSpeedY;
            break;
    }
}

void ThrowableTotalIndicator::VRender(PrimHeader** ppOt)
{
    const FP camX = FP_NoFractional(gScreenManager->CamXPos());
    const FP camY = FP_NoFractional(gScreenManager->CamYPos());

    s16 xpos = 0;
    s16 ypos = 0;

    const Glyph& pointData = sGlyphs[mNumToShow];
    for (u32 counter = 0; counter < pointData.mPointsCount; counter++)
    {
        xpos = FP_GetExponent(((mXPos - camX) * FP_FromInteger(40) + FP_FromInteger(11)) / FP_FromInteger(23));
        ypos = FP_GetExponent(mYPos - camY);

        const FP x0 = FP_FromInteger(pointData.mPoints[counter].p1.x) * mSpriteScale;
        const FP y0 = FP_FromInteger(pointData.mPoints[counter].p1.y) * mSpriteScale;
        const FP x1 = FP_FromInteger(pointData.mPoints[counter].p2.x) * mSpriteScale;
        const FP y1 = FP_FromInteger(pointData.mPoints[counter].p2.y) * mSpriteScale;
        Line_G2* pLine = &mLines[gPsxDisplay.mBufferIndex][counter];
        LineG2_Init(pLine);

        SetXY0(pLine, xpos + FP_GetExponent(x0), ypos + FP_GetExponent(y0));
        SetXY1(pLine, xpos + FP_GetExponent(x1), ypos + FP_GetExponent(y1));

        SetRGB0(pLine, static_cast<u8>(mRGB.r), static_cast<u8>(mRGB.g), static_cast<u8>(mRGB.b));
        SetRGB1(pLine, static_cast<u8>(mRGB.r), static_cast<u8>(mRGB.g), static_cast<u8>(mRGB.b));
        SetRGB2(pLine, static_cast<u8>(mRGB.r), static_cast<u8>(mRGB.g), static_cast<u8>(mRGB.b));


        Poly_Set_SemiTrans(&pLine->mBase.header, true);
        OrderingTable_Add(OtLayer(ppOt, mOtLayer), &pLine->mBase.header);
    }

    Init_SetTPage(&mTPage[gPsxDisplay.mBufferIndex], PSX_getTPage(TPageAbr::eBlend_1));
    OrderingTable_Add(OtLayer(ppOt, mOtLayer), &mTPage[gPsxDisplay.mBufferIndex].mBase);
}
