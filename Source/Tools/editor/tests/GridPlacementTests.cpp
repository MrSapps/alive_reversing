// Plain unit tests for GridPlacement.hpp's pure clamp math - no Qt, no GUI, no event loop.
// This is deliberately separate from EditorAutomationTests: that one drives the real editor
// end-to-end over a socket to prove the UI workflow wires up correctly, but it can't see
// exact placement coordinates (they live in Model/QGraphicsItem, not the QWidget/QAction
// tree it inspects). Numeric boundary behavior belongs here instead.

#include "GridPlacement.hpp"

#include <gtest/gtest.h>

using namespace GridPlacement;

TEST(ClampInt, WithinRangeUnchanged)
{
    EXPECT_EQ(ClampInt(5, 0, 10), 5);
}

TEST(ClampInt, BelowMinClampsToMin)
{
    EXPECT_EQ(ClampInt(-5, 0, 10), 0);
}

TEST(ClampInt, AboveMaxClampsToMax)
{
    EXPECT_EQ(ClampInt(15, 0, 10), 10);
}

TEST(ClampCameraIndex, WithinGridUnchanged)
{
    EXPECT_EQ(ClampCameraIndex(3, 10), 3);
}

TEST(ClampCameraIndex, NegativeClampsToZero)
{
    EXPECT_EQ(ClampCameraIndex(-1, 10), 0);
}

TEST(ClampCameraIndex, AtGridCountClampsToLastValidIndex)
{
    // A map with 10 cameras along this axis has valid indices [0, 9]; index 10 is one past
    // the last camera. This is the case the old AddObjectDialog.cpp bug got wrong: it clamped
    // against CameraGridWidth()/Height() (a single camera's *pixel* size, e.g. 1024) instead
    // of the actual camera count, so an index like 10 sailed straight through unclamped and
    // could name a camera that doesn't exist.
    EXPECT_EQ(ClampCameraIndex(10, 10), 9);
}

TEST(ClampCameraIndex, WellPastGridClampsToLastValidIndex)
{
    EXPECT_EQ(ClampCameraIndex(1000, 10), 9);
}

TEST(ClampCameraIndex, ZeroGridCountClampsToZero)
{
    EXPECT_EQ(ClampCameraIndex(5, 0), 0);
}

TEST(ClampPixelToMapBounds, WithinBoundsUnchanged)
{
    EXPECT_EQ(ClampPixelToMapBounds(500, 1024), 500);
}

TEST(ClampPixelToMapBounds, NegativeClampsToZero)
{
    EXPECT_EQ(ClampPixelToMapBounds(-50, 1024), 0);
}

TEST(ClampPixelToMapBounds, PastMapEdgeClampsToLastPixel)
{
    EXPECT_EQ(ClampPixelToMapBounds(2000, 1024), 1023);
}

TEST(ClampPixelToMapBounds, ZeroSizeClampsToZero)
{
    EXPECT_EQ(ClampPixelToMapBounds(500, 0), 0);
}

TEST(ClampRangeStartToMapBounds, WithinBoundsUnchanged)
{
    EXPECT_EQ(ClampRangeStartToMapBounds(500, 100, 1024), 500);
}

TEST(ClampRangeStartToMapBounds, NegativeStartClampsToZeroKeepingLength)
{
    // The exact scenario that used to collapse a freshly-created collision line to a single
    // point at (0,0): scenePos.x() = -280 with the original +100/+200 endpoint offsets gives
    // x1 = -180 and x2 = -80 - both negative, so clamping each endpoint independently sent
    // both to 0, losing the line entirely. Clamping the start instead keeps the full length.
    const int start = ClampRangeStartToMapBounds(-180, 100, 1024);
    EXPECT_EQ(start, 0);
    EXPECT_EQ(start + 100, 100);
    EXPECT_NE(start, start + 100) << "line collapsed to a zero-length point";
}

TEST(ClampRangeStartToMapBounds, StartPastMapEdgeKeepsLengthByShiftingBack)
{
    // Max valid coordinate for a 1024px-wide map is 1023 (matches ClampPixelToMapBounds), so
    // a 100-long range's start must land at 923, not 924 - 924+100 = 1024 would be one pixel
    // past the map, which is exactly the off-by-one this function used to have (caught by
    // dragging a collision line's midpoint out of bounds and observing x2 come back as 1024).
    const int start = ClampRangeStartToMapBounds(1000, 100, 1024);
    EXPECT_EQ(start, 923);
    EXPECT_EQ(start + 100, 1023);
    EXPECT_NE(start, start + 100) << "line collapsed to a zero-length point";
}

TEST(ClampRangeStartToMapBounds, LengthLargerThanMapClampsToZero)
{
    EXPECT_EQ(ClampRangeStartToMapBounds(50, 2000, 1024), 0);
}

TEST(ClampRangeStartToMapBounds, ZeroSizeClampsToZero)
{
    EXPECT_EQ(ClampRangeStartToMapBounds(50, 100, 0), 0);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
