#pragma once

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

    // Clamps where a [start, start + length) range begins so the *whole* range fits within
    // [0, totalSizePixels), keeping length fixed - unlike clamping each endpoint of the range
    // independently (via ClampPixelToMapBounds on start and start+length separately), which
    // can collapse the range to a single point: e.g. start=-180, length=100 clamps start to 0
    // and the end (-80) *also* clamps to 0, losing the range entirely even though a
    // same-length range beginning at 0 fits fine. Only shrinks length as a last resort, if
    // totalSizePixels is smaller than length itself (nothing of that length can fit at all).
    inline int ClampRangeStartToMapBounds(int start, int length, unsigned int totalSizePixels)
    {
        if (totalSizePixels == 0)
        {
            return 0;
        }
        const int maxStart = static_cast<int>(totalSizePixels) - length;
        if (maxStart < 0)
        {
            return 0;
        }
        return ClampInt(start, 0, maxStart);
    }
}
