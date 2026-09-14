#pragma once

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <utility>

// Small, pure placement-math helpers shared by AddCollisionCommand and AddObjectDialog's
// AddNewObjectCommand. Deliberately dependency-free (no Qt, EditorTab, or Model types) so
// they're directly unit-testable without any GUI/event-loop machinery.
namespace GridPlacement
{
    inline int ClampInt(int value, int minValue, int maxValue)
    {
        if (value < minValue)
        {
            return minValue;
        }
        if (value > maxValue)
        {
            return maxValue;
        }
        return value;
    }

    // Clamps a camera grid column/row index to the valid range for a map with gridCount
    // cameras along that axis: [0, gridCount - 1]. gridCount == 0 (no cameras on that axis)
    // always returns 0.
    inline int ClampCameraIndex(int index, unsigned int gridCount)
    {
        if (gridCount == 0)
        {
            return 0;
        }
        return ClampInt(index, 0, static_cast<int>(gridCount) - 1);
    }

    // Clamps a pixel coordinate to stay within a map whose total size along that axis, in
    // pixels, is totalSizePixels.
    inline int ClampPixelToMapBounds(int value, unsigned int totalSizePixels)
    {
        if (totalSizePixels == 0)
        {
            return 0;
        }
        return ClampInt(value, 0, static_cast<int>(totalSizePixels) - 1);
    }

    // Clamps where a [start, start + length] range begins so *both* start and start + length
    // are valid coordinates (i.e. each individually satisfies the same [0, totalSizePixels - 1]
    // bound as ClampPixelToMapBounds), keeping length fixed - unlike clamping each endpoint of
    // the range independently (via ClampPixelToMapBounds on start and start+length separately),
    // which can collapse the range to a single point: e.g. start=-180, length=100 clamps start
    // to 0 and the end (-80) *also* clamps to 0, losing the range entirely even though a
    // same-length range beginning at 0 fits fine. Only shrinks length as a last resort, if
    // totalSizePixels is smaller than length + 1 (nothing of that length can fit at all).
    inline int ClampRangeStartToMapBounds(int start, int length, unsigned int totalSizePixels)
    {
        if (totalSizePixels == 0)
        {
            return 0;
        }
        const int maxStart = static_cast<int>(totalSizePixels) - 1 - length;
        if (maxStart < 0)
        {
            return 0;
        }
        return ClampInt(start, 0, maxStart);
    }

    // Shrinks a length (a rect's width or height) so it can't be bigger than the map itself -
    // e.g. when the map shrinks to fewer/smaller cameras than an existing object spans. Used
    // ahead of ClampRangeStartToMapBounds, which alone can only reposition a range that already
    // fits; it can't make an oversized one smaller. A negative length clamps to 0.
    inline int ClampLengthToMapBounds(int length, unsigned int totalSizePixels)
    {
        if (length < 0)
        {
            return 0;
        }
        if (static_cast<unsigned int>(length) > totalSizePixels)
        {
            return static_cast<int>(totalSizePixels);
        }
        return length;
    }

    struct ClampedLine final
    {
        int x1, y1, x2, y2;
    };

    // Remaps a line's two endpoints into a new bounding box (newLeft, newTop, newWidth,
    // newHeight), scaling each axis independently and keeping each endpoint at the same
    // relative position within the box it had before - e.g. an endpoint that sat at the box's
    // top-left stays at the new box's top-left, one halfway across stays halfway across. Used
    // to shrink-then-reposition a line the same way a rect's width/height get clamped directly
    // (ClampLengthToMapBounds then ClampRangeStartToMapBounds), except a line has no
    // independent width/height to write back to - only two endpoints - so the equivalent
    // "shrink" has to be expressed as remapping those endpoints into the already-clamped box.
    // Pure geometry: doesn't know about map bounds at all, so the caller works out newWidth/
    // newHeight/newLeft/newTop itself (typically via ClampLengthToMapBounds and
    // ClampRangeStartToMapBounds, one call per axis).
    inline ClampedLine RemapLineToBoundingBox(int x1, int y1, int x2, int y2, int newLeft, int newTop, int newWidth, int newHeight)
    {
        const int left = std::min(x1, x2);
        const int top = std::min(y1, y2);
        const int width = std::abs(x2 - x1);
        const int height = std::abs(y2 - y1);

        // A zero-length axis (a perfectly horizontal/vertical line) has nothing to scale -
        // leave its own axis alone and just let the translation below carry it into the new box.
        const double sx = width > 0 ? static_cast<double>(newWidth) / width : 1.0;
        const double sy = height > 0 ? static_cast<double>(newHeight) / height : 1.0;

        const auto remap = [&](int x, int y) -> std::pair<int, int>
        {
            return {newLeft + static_cast<int>(std::lround((x - left) * sx)),
                    newTop + static_cast<int>(std::lround((y - top) * sy))};
        };
        const auto [rx1, ry1] = remap(x1, y1);
        const auto [rx2, ry2] = remap(x2, y2);
        return {rx1, ry1, rx2, ry2};
    }

    // Full shrink-then-reposition fit for a line: clamps each axis's length then its range
    // start (one call per axis, same primitives + order a rect's width/height/position clamp
    // uses directly) and feeds the results into RemapLineToBoundingBox. Generic over how each
    // axis is actually clamped, via 4 callables - (length) -> length for the two length clamps,
    // (start, length) -> start for the two range-start clamps - so both a caller with the raw
    // map size on hand (FitLineToMapBounds below, which just plugs ClampLengthToMapBounds/
    // ClampRangeStartToMapBounds straight in) and one that only has an IGridPointSnapper-style
    // interface to clamp against (ResizeableArrowItem::SyncFromCollisionItem, which clamps
    // against whichever tab it belongs to without ever needing to know that tab's raw pixel
    // size) share this one composition instead of each repeating it by hand.
    template <typename ClampLengthX, typename ClampLengthY, typename ClampRangeStartX, typename ClampRangeStartY>
    inline ClampedLine FitLineToBounds(int x1, int y1, int x2, int y2,
                                        ClampLengthX clampLengthX, ClampLengthY clampLengthY,
                                        ClampRangeStartX clampRangeStartX, ClampRangeStartY clampRangeStartY)
    {
        const int width = clampLengthX(std::abs(x2 - x1));
        const int height = clampLengthY(std::abs(y2 - y1));
        const int left = clampRangeStartX(std::min(x1, x2), width);
        const int top = clampRangeStartY(std::min(y1, y2), height);
        return RemapLineToBoundingBox(x1, y1, x2, y2, left, top, width, height);
    }

    // FitLineToBounds against a map of totalWidthPixels x totalHeightPixels directly, for a
    // caller with the raw map size on hand (e.g. ChangeMapSizeDialog's ForceItemsInsideMapBounds).
    inline ClampedLine FitLineToMapBounds(int x1, int y1, int x2, int y2, unsigned int totalWidthPixels, unsigned int totalHeightPixels)
    {
        return FitLineToBounds(
            x1, y1, x2, y2,
            [totalWidthPixels](int length) { return ClampLengthToMapBounds(length, totalWidthPixels); },
            [totalHeightPixels](int length) { return ClampLengthToMapBounds(length, totalHeightPixels); },
            [totalWidthPixels](int start, int length) { return ClampRangeStartToMapBounds(start, length, totalWidthPixels); },
            [totalHeightPixels](int start, int length) { return ClampRangeStartToMapBounds(start, length, totalHeightPixels); });
    }
}
