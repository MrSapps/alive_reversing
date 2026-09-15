#include "stdafx.h"
#include "ProgressBar.hpp"
#include "Font.hpp"
#include "../AliveLibAE/PsxRender.hpp"

#include <algorithm>
#include <cstdio>

ProgressBar::ProgressBar(s16 x, s16 y, s16 width, s16 height)
    : mX(x)
    , mY(y)
    , mWidth(width)
    , mHeight(height)
{
    // Uniform, thin 2px padding on every side - this is just the visible ring, kept slim. Text
    // clearance from it is handled separately, by centering the text within height in Draw()
    // rather than by growing this padding (that was tried - widening the padding to get clearance
    // just made the ring itself look bulky instead).
    mBorder.SetXYWH(x - 2, y - 2, width + 4, height + 4);
    mBorder.SetRGB0(200, 200, 200);
    mBorder.SetRGB1(200, 200, 200);
    mBorder.SetRGB2(200, 200, 200);
    mBorder.SetRGB3(200, 200, 200);

    mTrack.SetXYWH(x, y, width, height);
    mTrack.SetRGB0(40, 40, 40);
    mTrack.SetRGB1(40, 40, 40);
    mTrack.SetRGB2(40, 40, 40);
    mTrack.SetRGB3(40, 40, 40);

    mFill.SetXYWH(x, y, 0, height);
    mFill.SetRGB0(80, 200, 120);
    mFill.SetRGB1(80, 200, 120);
    mFill.SetRGB2(80, 200, 120);
    mFill.SetRGB3(80, 200, 120);
}

void ProgressBar::SetPercent(float percent01)
{
    mPercent = std::clamp(percent01, 0.0f, 1.0f);
    mFill.SetXYWH(mX, mY, static_cast<s16>(mWidth * mPercent), mHeight);
}

s32 ProgressBar::Draw(OrderingTable& ot, AliveFont& font, s32 polyOffset)
{
    char barText[16];
    snprintf(barText, sizeof(barText), "%05.2f%%", mPercent * 100.0f);
    const s32 textWidth = font.MeasureTextWidth(barText);
    const s32 textX = mX + (mWidth - textWidth) / 2;
    // Vertically centered within the track rather than flush with its top - the debug font's
    // glyph cells are 8px tall (see FontResources.cpp's sDebugFontAtlas), so this only actually
    // adds clearance if mHeight is taller than that (see DataConversionUI's constructor).
    constexpr s32 kGlyphHeight = 8;
    const s16 textY = static_cast<s16>(mY + (mHeight - kGlyphHeight) / 2);
    polyOffset = font.DrawString(ot, barText, textX, textY, relive::TBlendModes::eBlend_0, 0, 0, Layer::eLayer_0, 255, 255, 255, polyOffset, FP_FromInteger(1), 640, 0);

    // OrderingTable::Add prepends, and DrawOTag() draws head-first - whatever's Add()'ed first
    // ends up drawn last (i.e. on top). fill on top of track on top of the border "ring" behind
    // both.
    ot.Add(Layer::eLayer_0, &mFill);
    ot.Add(Layer::eLayer_0, &mTrack);
    ot.Add(Layer::eLayer_0, &mBorder);

    return polyOffset;
}
