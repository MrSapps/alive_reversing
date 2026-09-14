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

TEST(ClampLengthToMapBounds, WithinBoundsUnchanged)
{
    EXPECT_EQ(ClampLengthToMapBounds(500, 1024), 500);
}

TEST(ClampLengthToMapBounds, EqualToMapUnchanged)
{
    EXPECT_EQ(ClampLengthToMapBounds(1024, 1024), 1024);
}

TEST(ClampLengthToMapBounds, LargerThanMapShrinksToMapSize)
{
    // The scenario from shrinking a map down to e.g. a single 1024-wide camera: an existing
    // object spanning several cameras (say 3000px) must shrink to fit, not just get repositioned
    // (which is all ClampRangeStartToMapBounds alone can do for a range that already fits).
    EXPECT_EQ(ClampLengthToMapBounds(3000, 1024), 1024);
}

TEST(ClampLengthToMapBounds, NegativeClampsToZero)
{
    EXPECT_EQ(ClampLengthToMapBounds(-50, 1024), 0);
}

TEST(ClampLengthToMapBounds, ZeroSizeMapClampsToZero)
{
    EXPECT_EQ(ClampLengthToMapBounds(500, 0), 0);
}

// RemapLineToBoundingBox takes an already-computed new box (typically from ClampLengthToMapBounds
// + ClampRangeStartToMapBounds, one call per axis - the same two functions a rect's width/height/
// position clamp uses directly above) and remaps a line's two endpoints into it. It doesn't know
// about map bounds itself, so these pass the new box in directly rather than going through a
// map size, unlike the map-bounds tests above.
TEST(RemapLineToBoundingBox, UnchangedWhenBoxIsIdentical)
{
    const auto result = RemapLineToBoundingBox(100, 50, 300, 50, 100, 50, 200, 0);
    EXPECT_EQ(result.x1, 100);
    EXPECT_EQ(result.y1, 50);
    EXPECT_EQ(result.x2, 300);
    EXPECT_EQ(result.y2, 50);
}

TEST(RemapLineToBoundingBox, TranslatesWithoutDistortingWhenOnlyPositionChanges)
{
    // New box is the same size (100x50) as the line's own bounding box - a pure reposition, the
    // same case ChangeMapSizeDialog's ForceItemsInsideMapBounds and PasteItemsCommand hit for a
    // line that already fits but is offset out of bounds.
    const auto result = RemapLineToBoundingBox(2000, 3000, 2100, 3050, 923, 429, 100, 50);
    EXPECT_EQ(result.x1, 923);
    EXPECT_EQ(result.y1, 429);
    EXPECT_EQ(result.x2, 1023);
    EXPECT_EQ(result.y2, 479);
}

TEST(RemapLineToBoundingBox, ShrinksProportionallyAboutTopLeftCorner)
{
    // A line spanning a 2000x1000 box shrunk to fit a 1000x1000 one - each axis scales
    // independently (width halves, height unchanged), same as a rect's width/height clamp.
    const auto result = RemapLineToBoundingBox(0, 0, 2000, 1000, 0, 0, 1000, 1000);
    EXPECT_EQ(result.x1, 0);
    EXPECT_EQ(result.y1, 0);
    EXPECT_EQ(result.x2, 1000);
    EXPECT_EQ(result.y2, 1000);
}

TEST(RemapLineToBoundingBox, PreservesEndpointDirectionWhenShrinking)
{
    // x1 is the "high" end (1000) and x2 the "low" end (0) before the shrink - it must still be
    // the high end afterwards, not swapped, since callers (ResizeableArrowItem) rely on which
    // endpoint is which for the model's X1/X2 and (via mNext/mPrevious) chained collision lines.
    const auto result = RemapLineToBoundingBox(1000, 0, 0, 0, 0, 0, 500, 0);
    EXPECT_EQ(result.x1, 500);
    EXPECT_EQ(result.x2, 0);
    EXPECT_GT(result.x1, result.x2);
}

TEST(RemapLineToBoundingBox, ZeroWidthVerticalLineDoesNotDivideByZero)
{
    // A perfectly vertical line (x1 == x2) has zero width - the x-axis scale factor must fall
    // back to a safe default instead of computing 0/0, while the y-axis (height 2000, shrunk to
    // 480) still scales normally.
    const auto result = RemapLineToBoundingBox(500, 0, 500, 2000, 10, 20, 0, 480);
    EXPECT_EQ(result.x1, 10);
    EXPECT_EQ(result.x2, 10);
    EXPECT_EQ(result.y1, 20);
    EXPECT_EQ(result.y2, 500);
}

TEST(RemapLineToBoundingBox, ZeroHeightHorizontalLineDoesNotDivideByZero)
{
    const auto result = RemapLineToBoundingBox(0, 300, 2000, 300, 5, 7, 1024, 0);
    EXPECT_EQ(result.y1, 7);
    EXPECT_EQ(result.y2, 7);
    EXPECT_EQ(result.x1, 5);
    EXPECT_EQ(result.x2, 1029);
}

// FitLineToBounds is generic over how each axis gets clamped (4 callables), so both
// FitLineToMapBounds below and ResizeableArrowItem::SyncFromCollisionItem (which clamps via
// mSnapper instead of a raw map size) share this one composition. Use deliberately distinct,
// order-sensitive clamp callables per axis here - not real map-bounds clamps - so a wiring bug
// (e.g. the Y callables receiving X's length, or a (start, length) pair passed swapped) shows up
// as a wrong number rather than silently passing because X and Y happened to behave the same.
TEST(FitLineToBounds, ThreadsPerAxisCallablesInCorrectOrder)
{
    const auto result = FitLineToBounds(
        0, 0, 300, 100,
        [](int length) { return std::min(length, 200); },      // X length capped at 200
        [](int length) { return std::min(length, 50); },       // Y length capped at 50
        [](int start, int length) { return start + length; },  // distinct, order-sensitive X start
        [](int start, int length) { return start - length; }); // distinct, order-sensitive Y start
    EXPECT_EQ(result.x1, 200);
    EXPECT_EQ(result.y1, -50);
    EXPECT_EQ(result.x2, 400);
    EXPECT_EQ(result.y2, 0);
}

// FitLineToMapBounds is the one-shot version for a caller with the raw map size on hand
// (ChangeMapSizeDialog's ForceItemsInsideMapBounds) - it composes ClampLengthToMapBounds +
// ClampRangeStartToMapBounds + RemapLineToBoundingBox, so these mostly just confirm the
// composition wires those three together correctly; the boundary behavior of each piece is
// already covered by their own tests above.
TEST(FitLineToMapBounds, WithinBoundsUnchanged)
{
    const auto result = FitLineToMapBounds(10, 10, 110, 60, 1024, 480);
    EXPECT_EQ(result.x1, 10);
    EXPECT_EQ(result.y1, 10);
    EXPECT_EQ(result.x2, 110);
    EXPECT_EQ(result.y2, 60);
}

TEST(FitLineToMapBounds, RepositionsWithoutShrinkingWhenAlreadyFits)
{
    // 100x50 already fits a 1024x480 map - only needs to move back onto it, not shrink.
    const auto result = FitLineToMapBounds(2000, 3000, 2100, 3050, 1024, 480);
    EXPECT_EQ(result.x1, 923);
    EXPECT_EQ(result.y1, 429);
    EXPECT_EQ(result.x2, 1023);
    EXPECT_EQ(result.y2, 479);
}

TEST(FitLineToMapBounds, ShrinksOversizedLineToFitMap)
{
    // The regression case: a line 3000px wide pasted/left over from a bigger map, fit onto a
    // single 1024x480 camera - must shrink, not just slide sideways and still hang off the edge.
    const auto result = FitLineToMapBounds(0, 0, 3000, 0, 1024, 480);
    EXPECT_EQ(result.x1, 0);
    EXPECT_EQ(result.y1, 0);
    EXPECT_EQ(result.x2, 1024);
    EXPECT_EQ(result.y2, 0);
}

namespace
{
    // Order-independent comparison for CalcMapChanges' output - the exact set of edits matters,
    // not what order the nested x/y loop happened to produce them in.
    bool SameCameraEdits(const std::vector<CamToEdit>& actual, const std::vector<CamToEdit>& expected)
    {
        if (actual.size() != expected.size())
        {
            return false;
        }
        for (const auto& item : expected)
        {
            if (std::find(actual.begin(), actual.end(), item) == actual.end())
            {
                return false;
            }
        }
        return true;
    }
}

// CalcMapChanges diffs an old camera grid size against a new one. These 7 scenarios were
// previously a hand-rolled set of Test_*() functions (DoMapSizeTests, ChangeMapSizeDialog.cpp)
// that ran unconditionally at the very start of every real editor launch (main.cpp) and called
// abort() on failure - not exactly a subtle way to fail a release build. Same scenarios, now
// proper (and correctly spelled - "Coulmn" was never a word) tests.

TEST(CalcMapChanges, RemoveColumn)
{
    const auto edits = CalcMapChanges(2, 1, 2, 2);
    const std::vector<CamToEdit> expected = {CamToEdit{1, 0, false}, CamToEdit{1, 1, false}};
    EXPECT_TRUE(SameCameraEdits(edits, expected));
}

TEST(CalcMapChanges, AddColumn)
{
    const auto edits = CalcMapChanges(2, 3, 2, 2);
    const std::vector<CamToEdit> expected = {CamToEdit{2, 0, true}, CamToEdit{2, 1, true}};
    EXPECT_TRUE(SameCameraEdits(edits, expected));
}

TEST(CalcMapChanges, RemoveRow)
{
    const auto edits = CalcMapChanges(2, 2, 2, 1);
    const std::vector<CamToEdit> expected = {CamToEdit{0, 1, false}, CamToEdit{1, 1, false}};
    EXPECT_TRUE(SameCameraEdits(edits, expected));
}

TEST(CalcMapChanges, AddRow)
{
    const auto edits = CalcMapChanges(2, 2, 2, 3);
    const std::vector<CamToEdit> expected = {CamToEdit{0, 2, true}, CamToEdit{1, 2, true}};
    EXPECT_TRUE(SameCameraEdits(edits, expected));
}

TEST(CalcMapChanges, RemoveColumnAddRow)
{
    const auto edits = CalcMapChanges(2, 1, 2, 3);
    const std::vector<CamToEdit> expected = {
        CamToEdit{1, 0, false}, CamToEdit{1, 1, false},
        CamToEdit{0, 2, true}, CamToEdit{1, 2, true}};
    EXPECT_TRUE(SameCameraEdits(edits, expected));
}

TEST(CalcMapChanges, AddColumnAddRow)
{
    const auto edits = CalcMapChanges(2, 3, 2, 3);
    const std::vector<CamToEdit> expected = {
        CamToEdit{2, 0, true}, CamToEdit{2, 1, true},
        CamToEdit{0, 2, true}, CamToEdit{1, 2, true},
        CamToEdit{2, 2, true}};
    EXPECT_TRUE(SameCameraEdits(edits, expected));
}

TEST(CalcMapChanges, RemoveColumnRemoveRow)
{
    // The corner cell (1,1) is visited from both the shrinking-width and shrinking-height
    // checks inside CalcMapChanges - AddCameraGridEdit must not list it twice.
    const auto edits = CalcMapChanges(2, 1, 2, 1);
    const std::vector<CamToEdit> expected = {
        CamToEdit{1, 0, false}, CamToEdit{1, 1, false},
        CamToEdit{0, 1, false}};
    EXPECT_TRUE(SameCameraEdits(edits, expected));
    EXPECT_EQ(edits.size(), 3u) << "corner cell double-counted";
}

TEST(CalcMapChanges, NoChangeProducesNoEdits)
{
    EXPECT_TRUE(CalcMapChanges(2, 2, 2, 2).empty());
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
