#pragma once

#include "../Primitives.hpp"

class OrderingTable;
class AliveFont;

// A small self-contained progress bar: border/track/fill polys plus a centered "XX.XX%" label.
// Owns its own primitives so callers (DataConversionUI) don't have to juggle them directly.
class ProgressBar final
{
public:
    ProgressBar(s16 x, s16 y, s16 width, s16 height);

    // percent01 is clamped to [0, 1].
    void SetPercent(float percent01);

    // Adds the bar's polys/text to ot. polyOffset must be the caller's current running offset
    // into AliveFont's shared per-frame poly array (see AliveFont::DrawString - it returns the
    // new absolute offset, not a delta) - pass it in and use the returned value for whatever's
    // drawn next.
    [[nodiscard]] s32 Draw(OrderingTable& ot, AliveFont& font, s32 polyOffset);

private:
    Poly_G4 mBorder;
    Poly_G4 mTrack;
    Poly_G4 mFill;
    s16 mX = 0;
    s16 mY = 0;
    s16 mWidth = 0;
    s16 mHeight = 0;
    float mPercent = 0.0f;
};
