#pragma once

class IGridPointSnapper
{
public:
    virtual ~IGridPointSnapper() = default;
    virtual int SnapX(bool enabled, int x) = 0;
    virtual int SnapY(bool enabled, int y) = 0;

    // Always-on (not opt-in like Snap*) clamp to keep a point within the current map's bounds.
    virtual int ClampX(int x) = 0;
    virtual int ClampY(int y) = 0;

    // Clamps where a [start, start + length) range begins so the whole range stays within the
    // map's bounds, keeping length fixed rather than clamping each end of the range
    // independently (which can collapse it to a point - see GridPlacement::ClampRangeStartToMapBounds).
    virtual int ClampRangeStartX(int start, int length) = 0;
    virtual int ClampRangeStartY(int start, int length) = 0;

    // Shrinks a length (a rect's width or height) so it can't be bigger than the map itself -
    // see GridPlacement::ClampLengthToMapBounds. Apply before ClampRangeStartX/Y, which alone
    // can only reposition a range that already fits, not shrink an oversized one.
    virtual int ClampLengthX(int length) = 0;
    virtual int ClampLengthY(int length) = 0;
};
