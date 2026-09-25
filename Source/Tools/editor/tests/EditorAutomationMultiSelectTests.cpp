// Editor automation tests: multi-selection dragging and changing the map size.
// Helpers and main() are in EditorAutomationTestHelpers.cpp.

#include "EditorAutomationTestHelpers.hpp"

using namespace AutomationTest;

TEST(EditorAutomation, MultiSelectDragKeepsWholeSelectionInBounds)
{
    QProcess editor;
    AutomationClient client;
    QString socketName;
    ASSERT_TRUE(LaunchEditorAndConnect(editor, client, socketName));

    ASSERT_TRUE(CreateNewPath(client));
    ASSERT_TRUE(AddCollisionLine(client));
    ASSERT_TRUE(AddMapObject(client));

    const nlohmann::json items = GetSceneItems(client);
    const nlohmann::json line = FindKind(items, "collision_line");
    const nlohmann::json rect = FindKind(items, "map_object");

    const double maxX = std::max({line.at("x1").get<double>(), line.at("x2").get<double>(), rect.at("xpos").get<double>() + rect.at("width").get<double>()});
    const double maxY = std::max({line.at("y1").get<double>(), line.at("y2").get<double>(), rect.at("ypos").get<double>() + rect.at("height").get<double>()});

    // Rubber-band select both: press starts well outside either item (in the scene's empty
    // margin - see EditorGraphicsScene::UpdateSceneRect's 100px border - so it's unambiguously
    // empty canvas, not on top of either item) and releases past their far corner.
    DragView(client, SceneToView(client, -50, -50), SceneToView(client, maxX + 20, maxY + 20));

    {
        const nlohmann::json selectedItems = GetSceneItems(client);
        EXPECT_TRUE(FindKind(selectedItems, "collision_line").value("selected", false)) << "rubber-band drag did not select the collision line";
        EXPECT_TRUE(FindKind(selectedItems, "map_object").value("selected", false)) << "rubber-band drag did not select the map object";
    }

    const nlohmann::json beforeGroupDrag = GetSceneItems(client);
    const nlohmann::json lineBefore = FindKind(beforeGroupDrag, "collision_line");
    const nlohmann::json rectBefore = FindKind(beforeGroupDrag, "map_object");
    const double relDX = rectBefore.at("xpos").get<double>() - lineBefore.at("x1").get<double>();
    const double relDY = rectBefore.at("ypos").get<double>() - lineBefore.at("y1").get<double>();
    const int lineWidth = lineBefore.at("x2").get<int>() - lineBefore.at("x1").get<int>();
    const int lineHeight = lineBefore.at("y2").get<int>() - lineBefore.at("y1").get<int>();

    // Drag the map object's body (part of the multi-selection) far off the map. Grabbing any
    // selected+movable item drags the whole group by the same delta.
    const double midX = rectBefore.at("xpos").get<double>() + rectBefore.at("width").get<double>() / 2;
    const double midY = rectBefore.at("ypos").get<double>() + rectBefore.at("height").get<double>() / 2;
    DragView(client, SceneToView(client, midX, midY), SceneToView(client, 1'000'000, 1'000'000));

    const nlohmann::json afterGroupDrag = GetSceneItems(client);
    const nlohmann::json lineAfter = FindKind(afterGroupDrag, "collision_line");
    const nlohmann::json rectAfter = FindKind(afterGroupDrag, "map_object");

    // Both items stayed within the map...
    EXPECT_LT(rectAfter.value("xpos", -1.0) + rectAfter.value("width", 0.0), 100000) << "map object escaped the map bounds";
    EXPECT_LT(rectAfter.value("ypos", -1.0) + rectAfter.value("height", 0.0), 100000) << "map object escaped the map bounds";
    EXPECT_LT(lineAfter.value("x1", -1), 100000) << "collision line escaped the map bounds";
    EXPECT_LT(lineAfter.value("y1", -1), 100000) << "collision line escaped the map bounds";
    EXPECT_LT(lineAfter.value("x2", -1), 100000) << "collision line escaped the map bounds";
    EXPECT_LT(lineAfter.value("y2", -1), 100000) << "collision line escaped the map bounds";

    // ...neither was resized/distorted while clamping...
    EXPECT_DOUBLE_EQ(rectAfter.value("width", -1.0), rectBefore.at("width").get<double>()) << "group clamp resized the map object";
    EXPECT_DOUBLE_EQ(rectAfter.value("height", -1.0), rectBefore.at("height").get<double>());
    EXPECT_EQ(lineAfter.value("x2", -1) - lineAfter.value("x1", -2), lineWidth) << "group clamp distorted the collision line's shape";
    EXPECT_EQ(lineAfter.value("y2", -1) - lineAfter.value("y1", -2), lineHeight);

    // ...and critically, their relative offset is exactly preserved - proving the whole
    // selection was shifted together as a rigid group, not each item clamped independently
    // (which is all the old code did, and only for whichever single item was actually dragged).
    EXPECT_DOUBLE_EQ(rectAfter.value("xpos", -1.0) - lineAfter.value("x1", -2), relDX) << "group members' relative layout was not preserved while clamping";
    EXPECT_DOUBLE_EQ(rectAfter.value("ypos", -1.0) - lineAfter.value("y1", -2), relDY);

    editor.terminate();
    ASSERT_TRUE(editor.waitForFinished(5000)) << "editor did not exit after terminate()";
}

// Regression test for a follow-up bug in the fix above: clamping the whole multi-selection only
// at mouseReleaseEvent meant a multi-item drag no longer stopped at the map's edge *while being
// dragged* - it would sail arbitrarily far out of bounds and only snap back once the mouse
// button was released, unlike a single selected item (which has always clamped live, on every
// mouseMoveEvent). Uses mouse_event's separate down/move/up phases (rather than "drag", which
// does all three atomically) so the object's position can be inspected *before* the button is
// released.
TEST(EditorAutomation, MultiSelectDragClampsLiveNotJustOnRelease)
{
    QProcess editor;
    AutomationClient client;
    QString socketName;
    ASSERT_TRUE(LaunchEditorAndConnect(editor, client, socketName));

    ASSERT_TRUE(CreateNewPath(client));
    ASSERT_TRUE(AddCollisionLine(client));
    ASSERT_TRUE(AddMapObject(client));

    const nlohmann::json items = GetSceneItems(client);
    const nlohmann::json line = FindKind(items, "collision_line");
    const nlohmann::json rect = FindKind(items, "map_object");
    const double maxX = std::max({line.at("x1").get<double>(), line.at("x2").get<double>(), rect.at("xpos").get<double>() + rect.at("width").get<double>()});
    const double maxY = std::max({line.at("y1").get<double>(), line.at("y2").get<double>(), rect.at("ypos").get<double>() + rect.at("height").get<double>()});

    // Rubber-band select both, same as MultiSelectDragKeepsWholeSelectionInBounds.
    DragView(client, SceneToView(client, -50, -50), SceneToView(client, maxX + 20, maxY + 20));
    {
        const nlohmann::json selectedItems = GetSceneItems(client);
        ASSERT_TRUE(FindKind(selectedItems, "collision_line").value("selected", false)) << "rubber-band drag did not select the collision line";
        ASSERT_TRUE(FindKind(selectedItems, "map_object").value("selected", false)) << "rubber-band drag did not select the map object";
    }

    const nlohmann::json before = FindKind(GetSceneItems(client), "map_object");
    const double midX = before.at("xpos").get<double>() + before.at("width").get<double>() / 2;
    const double midY = before.at("ypos").get<double>() + before.at("height").get<double>() / 2;
    const QPoint from = SceneToView(client, midX, midY);
    const QPoint farOut = SceneToView(client, 1'000'000, 1'000'000);

    // Press on the map object (part of the selection) and move far off the map - deliberately
    // not releasing yet.
    ASSERT_TRUE(SendMouseEvent(client, "down", from));
    ASSERT_TRUE(SendMouseEvent(client, "move", farOut));

    // While the button is still held down, both items must already be clamped inside the map -
    // not wherever the raw mouse position would otherwise put them.
    {
        const nlohmann::json midDragItems = GetSceneItems(client);
        const nlohmann::json midDragRect = FindKind(midDragItems, "map_object");
        const nlohmann::json midDragLine = FindKind(midDragItems, "collision_line");
        EXPECT_LT(midDragRect.value("xpos", -1.0) + midDragRect.value("width", 0.0), 100000) << "map object escaped the map bounds mid-drag, before release";
        EXPECT_LT(midDragRect.value("ypos", -1.0) + midDragRect.value("height", 0.0), 100000) << "map object escaped the map bounds mid-drag, before release";
        EXPECT_LT(midDragLine.value("x1", -1), 100000) << "collision line escaped the map bounds mid-drag, before release";
        EXPECT_LT(midDragLine.value("y1", -1), 100000) << "collision line escaped the map bounds mid-drag, before release";
        EXPECT_LT(midDragLine.value("x2", -1), 100000) << "collision line escaped the map bounds mid-drag, before release";
        EXPECT_LT(midDragLine.value("y2", -1), 100000) << "collision line escaped the map bounds mid-drag, before release";
    }

    ASSERT_TRUE(SendMouseEvent(client, "up", farOut));

    editor.terminate();
    ASSERT_TRUE(editor.waitForFinished(5000)) << "editor did not exit after terminate()";
}

// Regression test: 3 identically-sized map objects in a row, all selected, grid snap for map
// objects enabled - dragging any one of them (the leftmost here) used to snap only *that* item
// to the grid, while the other two (moved by Qt's own default multi-select translate) kept the
// raw, unsnapped delta - so a previously-evenly-spaced row would end up with the grabbed object
// on-grid and the other two holding their old (now wrong) relative spacing, silently breaking
// the row's alignment. Fixed by ResizeableRectItem::mouseMoveEvent measuring how far the grab
// correction moved the grabbed item beyond the shared raw delta and reapplying that same
// correction to the rest of the selection (ItemPositionData::TranslateOtherSelectedItems).
TEST(EditorAutomation, MultiSelectDragWithSnapKeepsRowAligned)
{
    QProcess editor;
    AutomationClient client;
    QString socketName;
    ASSERT_TRUE(LaunchEditorAndConnect(editor, client, socketName));

    ASSERT_TRUE(CreateNewPath(client));

    const std::vector<nlohmann::json> before = AddThreeAlignedMapObjects(client, 100, 500);
    ASSERT_EQ(before.size(), 3u);
    const double spacingA = before[1].at("xpos").get<double>() - before[0].at("xpos").get<double>();
    const double spacingB = before[2].at("xpos").get<double>() - before[1].at("xpos").get<double>();
    ASSERT_GT(spacingA, 0) << "test setup: objects are not distinctly positioned";
    ASSERT_DOUBLE_EQ(before[0].at("ypos").get<double>(), before[1].at("ypos").get<double>()) << "test setup: row is not aligned";
    ASSERT_DOUBLE_EQ(before[1].at("ypos").get<double>(), before[2].at("ypos").get<double>());

    // Rubber-band select all 3.
    {
        double minX = before[0].at("xpos").get<double>();
        double minY = before[0].at("ypos").get<double>();
        double maxX = 0;
        double maxY = 0;
        for (const auto& obj : before)
        {
            minX = std::min(minX, obj.at("xpos").get<double>());
            minY = std::min(minY, obj.at("ypos").get<double>());
            maxX = std::max(maxX, obj.at("xpos").get<double>() + obj.at("width").get<double>());
            maxY = std::max(maxY, obj.at("ypos").get<double>() + obj.at("height").get<double>());
        }
        DragView(client, SceneToView(client, minX - 20, minY - 20), SceneToView(client, maxX + 20, maxY + 20));
    }
    for (const auto& item : GetSceneItems(client))
    {
        if (item.value("kind", std::string()) == "map_object")
        {
            EXPECT_TRUE(item.value("selected", false)) << "rubber-band select did not select all 3 objects";
        }
    }

    ASSERT_TRUE(client.Call({{"cmd", "click"}, {"target", "action_snap_map_objects_x"}}).value("ok", false));
    ASSERT_TRUE(client.Call({{"cmd", "click"}, {"target", "action_snap_map_objects_y"}}).value("ok", false));

    // Drag the leftmost (grabbed) object's body by a deliberately non-grid-aligned amount.
    {
        const double x = before[0].at("xpos").get<double>();
        const double y = before[0].at("ypos").get<double>();
        DragView(client, SceneToView(client, x + 10, y + 10), SceneToView(client, x + 10 + 47, y + 10 + 47));
    }

    ASSERT_TRUE(client.Call({{"cmd", "click"}, {"target", "action_snap_map_objects_x"}}).value("ok", false));
    ASSERT_TRUE(client.Call({{"cmd", "click"}, {"target", "action_snap_map_objects_y"}}).value("ok", false));

    const std::vector<nlohmann::json> after = SortedMapObjectsByX(client);
    ASSERT_EQ(after.size(), 3u);

    // The grabbed object actually snapped (same check MapObjectDragResizeBoundsAndSnap uses).
    const int grabbedYPos = static_cast<int>(after[0].at("ypos").get<double>());
    EXPECT_EQ(grabbedYPos % 20, 0) << "grabbed object did not snap to the Y grid: ypos=" << grabbedYPos;

    // Relative spacing between all 3 must be preserved exactly - not just the grabbed one moved.
    EXPECT_DOUBLE_EQ(after[1].at("xpos").get<double>() - after[0].at("xpos").get<double>(), spacingA)
        << "middle object desynced from the grabbed one after a snapped multi-select drag";
    EXPECT_DOUBLE_EQ(after[2].at("xpos").get<double>() - after[1].at("xpos").get<double>(), spacingB)
        << "rightmost object desynced from the middle one after a snapped multi-select drag";
    EXPECT_DOUBLE_EQ(after[0].at("ypos").get<double>(), after[1].at("ypos").get<double>())
        << "row misaligned vertically after a snapped multi-select drag";
    EXPECT_DOUBLE_EQ(after[1].at("ypos").get<double>(), after[2].at("ypos").get<double>());

    editor.terminate();
    ASSERT_TRUE(editor.waitForFinished(5000)) << "editor did not exit after terminate()";
}

// Same scenario as MultiSelectDragWithSnapKeepsRowAligned, but with grid snapping OFF (the
// default state) - a user separately reported the grabbed item can still drift out of sync with
// the rest of the selection by a stray pixel or so even without snapping enabled, because
// ResizeableRectItem::mouseMoveEvent always truncates the grabbed item's position to a whole
// pixel every frame (via static_cast<int>, right before calling SnapX/Y - which itself is a
// no-op when disabled) regardless of whether grid rounding is actually on - a truncation the
// other, non-grabbed selected items (moved only by Qt's own raw float delta) never go through.
// TranslateOtherSelectedItems corrects for this the same general way it does for an actual snap:
// it doesn't care *why* the grabbed item's final position differs from the shared raw delta,
// only by how much, and reapplies exactly that to the rest of the selection.
//
// At the default 1:1 view zoom, an integer view-pixel drag already lands on an exact integer
// scene position, so there's nothing for that truncation to actually round away - the bug needs
// a non-integer view<->scene mapping to show up at all. Zooms in once (action_zoom_in, a real
// pre-existing menu action - see EditorTab::ZoomIn, scale 1.1x) before the drag so the
// press/release view pixels this test sends round-trip through a fractional scene position, the
// same way a real mouse drag at a non-100% zoom would.
TEST(EditorAutomation, MultiSelectDragWithoutSnapKeepsRowAligned)
{
    QProcess editor;
    AutomationClient client;
    QString socketName;
    ASSERT_TRUE(LaunchEditorAndConnect(editor, client, socketName));

    ASSERT_TRUE(CreateNewPath(client));

    const std::vector<nlohmann::json> before = AddThreeAlignedMapObjects(client, 100, 500);
    ASSERT_EQ(before.size(), 3u);
    const double spacingA = before[1].at("xpos").get<double>() - before[0].at("xpos").get<double>();
    const double spacingB = before[2].at("xpos").get<double>() - before[1].at("xpos").get<double>();
    ASSERT_GT(spacingA, 0) << "test setup: objects are not distinctly positioned";
    ASSERT_DOUBLE_EQ(before[0].at("ypos").get<double>(), before[1].at("ypos").get<double>()) << "test setup: row is not aligned";
    ASSERT_DOUBLE_EQ(before[1].at("ypos").get<double>(), before[2].at("ypos").get<double>());

    {
        double minX = before[0].at("xpos").get<double>();
        double minY = before[0].at("ypos").get<double>();
        double maxX = 0;
        double maxY = 0;
        for (const auto& obj : before)
        {
            minX = std::min(minX, obj.at("xpos").get<double>());
            minY = std::min(minY, obj.at("ypos").get<double>());
            maxX = std::max(maxX, obj.at("xpos").get<double>() + obj.at("width").get<double>());
            maxY = std::max(maxY, obj.at("ypos").get<double>() + obj.at("height").get<double>());
        }
        DragView(client, SceneToView(client, minX - 20, minY - 20), SceneToView(client, maxX + 20, maxY + 20));
    }
    for (const auto& item : GetSceneItems(client))
    {
        if (item.value("kind", std::string()) == "map_object")
        {
            EXPECT_TRUE(item.value("selected", false)) << "rubber-band select did not select all 3 objects";
        }
    }

    ASSERT_TRUE(client.Call({{"cmd", "click"}, {"target", "action_zoom_in"}}).value("ok", false));

    // Drag the leftmost object's body by an arbitrary, non-round amount - no snapping is enabled
    // here (the default state), so this alone must not desync the group.
    {
        const double x = before[0].at("xpos").get<double>();
        const double y = before[0].at("ypos").get<double>();
        DragView(client, SceneToView(client, x + 10, y + 10), SceneToView(client, x + 10 + 33, y + 10 + 17));
    }

    ASSERT_TRUE(client.Call({{"cmd", "click"}, {"target", "action_zoom_reset"}}).value("ok", false));

    const std::vector<nlohmann::json> after = SortedMapObjectsByX(client);
    ASSERT_EQ(after.size(), 3u);

    EXPECT_DOUBLE_EQ(after[1].at("xpos").get<double>() - after[0].at("xpos").get<double>(), spacingA)
        << "middle object desynced from the grabbed one after an unsnapped multi-select drag";
    EXPECT_DOUBLE_EQ(after[2].at("xpos").get<double>() - after[1].at("xpos").get<double>(), spacingB)
        << "rightmost object desynced from the middle one after an unsnapped multi-select drag";
    EXPECT_DOUBLE_EQ(after[0].at("ypos").get<double>(), after[1].at("ypos").get<double>())
        << "row misaligned vertically after an unsnapped multi-select drag";
    EXPECT_DOUBLE_EQ(after[1].at("ypos").get<double>(), after[2].at("ypos").get<double>());

    editor.terminate();
    ASSERT_TRUE(editor.waitForFinished(5000)) << "editor did not exit after terminate()";
}

// Same bug, mixed selection: 2 map objects + 1 collision line selected together, grabbing and
// snap-dragging one of the *rects*. ResizeableArrowItem's own whole-line body drag never snaps
// or truncates a line's position at all (unlike ResizeableRectItem, it stays float-precise
// throughout - see its mouseMoveEvent), so a co-selected line was never at risk of desyncing
// *itself*, but it still needs to receive the *other* item's snap correction to stay attached to
// the group - TranslateOtherSelectedItems handles both ResizeableRectItem and ResizeableArrowItem
// (ItemPositionData.cpp), so this checks the line arm of that, not just the rect arm the tests
// above already cover.
TEST(EditorAutomation, MultiSelectDragWithSnapKeepsMixedRectAndLineSelectionAligned)
{
    QProcess editor;
    AutomationClient client;
    QString socketName;
    ASSERT_TRUE(LaunchEditorAndConnect(editor, client, socketName));

    ASSERT_TRUE(CreateNewPath(client));

    // Place each item immediately after adding it, identified while it's still the only one of
    // its kind (map objects) or unambiguous (the one collision line) - same discipline
    // AddThreeAlignedMapObjects uses, and for the same reason: a fresh map object always lands
    // at the current viewport's center, so two of them would otherwise be indistinguishable.

    // Map object A -> (100, 100).
    ASSERT_TRUE(AddMapObject(client));
    {
        const nlohmann::json obj = FindSelectedKind(GetSceneItems(client), "map_object");
        ASSERT_FALSE(obj.empty());
        const double curX = obj.at("xpos").get<double>();
        const double curY = obj.at("ypos").get<double>();
        DragView(client, SceneToView(client, curX + 10, curY + 10), SceneToView(client, 110, 110));
    }

    // Map object B -> (600, 100), well clear of A.
    ASSERT_TRUE(AddMapObject(client));
    {
        const nlohmann::json obj = FindSelectedKind(GetSceneItems(client), "map_object");
        ASSERT_FALSE(obj.empty());
        const double curX = obj.at("xpos").get<double>();
        const double curY = obj.at("ypos").get<double>();
        DragView(client, SceneToView(client, curX + 10, curY + 10), SceneToView(client, 610, 110));
    }

    // Collision line -> (1100, 300)-(1200, 300), clear of both objects. Dragged by its body
    // midpoint (150, 100) - the fixed default placement AddCollisionLine always uses - which is
    // far enough from either endpoint (50px) to grab the whole line rather than resize one end.
    ASSERT_TRUE(AddCollisionLine(client));
    DragView(client, SceneToView(client, 150, 100), SceneToView(client, 1150, 300));

    const std::vector<nlohmann::json> objsBefore = SortedMapObjectsByX(client);
    ASSERT_EQ(objsBefore.size(), 2u);
    const nlohmann::json& objABefore = objsBefore[0];
    const nlohmann::json& objBBefore = objsBefore[1];
    const nlohmann::json lineBefore = FindKind(GetSceneItems(client), "collision_line");

    // Rubber-band select all 3.
    DragView(client, SceneToView(client, 50, 50), SceneToView(client, 1250, 350));
    ASSERT_EQ(CountKind(GetSceneItems(client), "map_object"), 2);
    ASSERT_EQ(CountKind(GetSceneItems(client), "collision_line"), 1);
    for (const auto& item : GetSceneItems(client))
    {
        const std::string kind = item.value("kind", std::string());
        if (kind != "map_object" && kind != "collision_line")
        {
            continue;
        }
        EXPECT_TRUE(item.value("selected", false)) << "rubber-band select did not select every item: " << item.dump();
    }

    ASSERT_TRUE(client.Call({{"cmd", "click"}, {"target", "action_snap_map_objects_x"}}).value("ok", false));
    ASSERT_TRUE(client.Call({{"cmd", "click"}, {"target", "action_snap_map_objects_y"}}).value("ok", false));

    // Drag object A's (the grabbed rect's) body by a deliberately non-grid-aligned amount.
    {
        const double x = objABefore.at("xpos").get<double>();
        const double y = objABefore.at("ypos").get<double>();
        DragView(client, SceneToView(client, x + 10, y + 10), SceneToView(client, x + 10 + 47, y + 10 + 47));
    }

    ASSERT_TRUE(client.Call({{"cmd", "click"}, {"target", "action_snap_map_objects_x"}}).value("ok", false));
    ASSERT_TRUE(client.Call({{"cmd", "click"}, {"target", "action_snap_map_objects_y"}}).value("ok", false));

    const std::vector<nlohmann::json> objsAfter = SortedMapObjectsByX(client);
    ASSERT_EQ(objsAfter.size(), 2u);
    const nlohmann::json& objAAfter = objsAfter[0];
    const nlohmann::json& objBAfter = objsAfter[1];
    const nlohmann::json lineAfter = FindKind(GetSceneItems(client), "collision_line");

    // The dx/dy A actually moved by (raw drag delta + whatever snap correction was applied).
    const double dx = objAAfter.at("xpos").get<double>() - objABefore.at("xpos").get<double>();
    const double dy = objAAfter.at("ypos").get<double>() - objABefore.at("ypos").get<double>();
    ASSERT_NE(dx, 47) << "test setup: snap did not actually change the raw 47px drag - nothing to desync";

    EXPECT_DOUBLE_EQ(objBAfter.at("xpos").get<double>() - objBBefore.at("xpos").get<double>(), dx)
        << "other map object desynced from the grabbed one after a snapped multi-select drag";
    EXPECT_DOUBLE_EQ(objBAfter.at("ypos").get<double>() - objBBefore.at("ypos").get<double>(), dy);

    EXPECT_DOUBLE_EQ(lineAfter.at("x1").get<int>() - lineBefore.at("x1").get<int>(), dx)
        << "collision line desynced from the grabbed map object after a snapped multi-select drag";
    EXPECT_DOUBLE_EQ(lineAfter.at("y1").get<int>() - lineBefore.at("y1").get<int>(), dy);
    EXPECT_DOUBLE_EQ(lineAfter.at("x2").get<int>() - lineBefore.at("x2").get<int>(), dx);
    EXPECT_DOUBLE_EQ(lineAfter.at("y2").get<int>() - lineBefore.at("y2").get<int>(), dy);

    editor.terminate();
    ASSERT_TRUE(editor.waitForFinished(5000)) << "editor did not exit after terminate()";
}

// Regression test for ChangeMapSizeCommand's "TODO: Handle collision items that are now outside
// of the map rect": shrinking the map used to leave surviving objects wherever they already
// were, even entirely outside the new (smaller) bounds, and never shrank an object that was
// bigger than the map itself got. Also checks undo/redo, since ForceItemsInsideMapBounds's
// clamp needed its own before-state snapshot (ChangeMapSizeCommand::mBeforeResizeSnapshot) -
// just restoring the old map size on undo isn't enough once an oversized object has been
// shrunk to fit.
TEST(EditorAutomation, ChangeMapSizeForcesObjectsInsideBoundsWithUndoRedo)
{
    QProcess editor;
    AutomationClient client;
    QString socketName;
    ASSERT_TRUE(LaunchEditorAndConnect(editor, client, socketName));

    ASSERT_TRUE(CreateNewPath(client));

    // Add the object while the map is still the default 1x1, so it's guaranteed to land in the
    // one camera that's always still there after shrinking back down later (AddNewObjectCommand
    // places a new object at the current viewport's center, which - once the map has already
    // grown - could otherwise land in a camera cell that the later shrink removes entirely
    // along with the object, rather than the surviving cell this test means to exercise).
    ASSERT_TRUE(AddMapObject(client));

    // Grow to a 3-wide map (AO's camera grid cell is 1024x480 - see Model::CameraGridWidth) so
    // there's room to resize the object bigger than a single camera cell.
    ASSERT_TRUE(SetMapSize(client, 3, 1));
    ASSERT_EQ(CountKind(GetSceneItems(client), "camera"), 3) << "map did not grow to 3 cameras wide";

    // Move the object to camera (0,0)'s top-left corner, then grow it wider than a single
    // camera cell while keeping its *center* inside camera (0,0) - every drag release
    // recalculates which camera "contains" an object from its center point
    // (ItemPositionData::Save -> CalcContainingCamera), and reassigns it there. Letting the
    // resize push the center into camera (1,0) would make this test exercise that (correct,
    // pre-existing, unrelated) reassignment-and-removal behavior instead of the bounds clamp
    // this test means to check - the object needs to still belong to the surviving camera.
    {
        const nlohmann::json before = FindKind(GetSceneItems(client), "map_object");
        const double x = before.at("xpos").get<double>();
        const double y = before.at("ypos").get<double>();
        const double w = before.at("width").get<double>();
        const double h = before.at("height").get<double>();
        DragView(client, SceneToView(client, x + w / 2, y + h / 2), SceneToView(client, w / 2, h / 2));
    }
    {
        const nlohmann::json before = FindKind(GetSceneItems(client), "map_object");
        const double x = before.at("xpos").get<double>();
        const double y = before.at("ypos").get<double>();
        const double w = before.at("width").get<double>();
        const double h = before.at("height").get<double>();
        DragView(client, SceneToView(client, x + w, y + h), SceneToView(client, x + 1200, y + h + 40));
    }

    const nlohmann::json oversized = FindKind(GetSceneItems(client), "map_object");
    ASSERT_GT(oversized.at("width").get<double>(), 1024) << "test setup failed to make the object bigger than a single camera";

    // Shrink back down to a single 1x1 camera: the object must shrink to fit AND reposition,
    // not just get left hanging outside the map.
    ASSERT_TRUE(SetMapSize(client, 1, 1));
    ASSERT_EQ(CountKind(GetSceneItems(client), "camera"), 1) << "map did not shrink to a single camera";

    const nlohmann::json shrunk = FindKind(GetSceneItems(client), "map_object");
    EXPECT_LE(shrunk.value("width", 99999.0), 1024) << "oversized object was not shrunk to fit the map";
    EXPECT_GE(shrunk.value("xpos", -1.0), 0);
    EXPECT_GE(shrunk.value("ypos", -1.0), 0);
    EXPECT_LE(shrunk.value("xpos", -1.0) + shrunk.value("width", 0.0), 1024) << "object still extends past the map's right edge";
    EXPECT_LE(shrunk.value("ypos", -1.0) + shrunk.value("height", 0.0), 480) << "object still extends past the map's bottom edge";

    // Undo must restore both the map size and the object's exact pre-shrink oversized rect -
    // not just whichever position/size it happened to have after the clamp.
    ASSERT_TRUE(client.Call({{"cmd", "click"}, {"target", "action_undo"}}).value("ok", false));
    ASSERT_EQ(CountKind(GetSceneItems(client), "camera"), 3) << "undo did not restore the 3-wide map";
    {
        const nlohmann::json afterUndo = FindKind(GetSceneItems(client), "map_object");
        EXPECT_DOUBLE_EQ(afterUndo.value("xpos", -1.0), oversized.at("xpos").get<double>()) << "undo did not restore the object's pre-shrink position/size";
        EXPECT_DOUBLE_EQ(afterUndo.value("ypos", -1.0), oversized.at("ypos").get<double>());
        EXPECT_DOUBLE_EQ(afterUndo.value("width", -1.0), oversized.at("width").get<double>());
        EXPECT_DOUBLE_EQ(afterUndo.value("height", -1.0), oversized.at("height").get<double>());
    }

    // Redo must re-apply the exact same shrink+reposition.
    ASSERT_TRUE(client.Call({{"cmd", "click"}, {"target", "action_redo"}}).value("ok", false));
    ASSERT_EQ(CountKind(GetSceneItems(client), "camera"), 1) << "redo did not reapply the 1-wide map";
    {
        const nlohmann::json afterRedo = FindKind(GetSceneItems(client), "map_object");
        EXPECT_DOUBLE_EQ(afterRedo.value("xpos", -1.0), shrunk.value("xpos", -1.0)) << "redo did not reapply the same clamp";
        EXPECT_DOUBLE_EQ(afterRedo.value("ypos", -1.0), shrunk.value("ypos", -1.0));
        EXPECT_DOUBLE_EQ(afterRedo.value("width", -1.0), shrunk.value("width", -1.0));
        EXPECT_DOUBLE_EQ(afterRedo.value("height", -1.0), shrunk.value("height", -1.0));
    }

    editor.terminate();
    ASSERT_TRUE(editor.waitForFinished(5000)) << "editor did not exit after terminate()";
}

// Same regression as ChangeMapSizeForcesObjectsInsideBoundsWithUndoRedo above, but for a
// collision line: ForceItemsInsideMapBounds used to only reposition a line, never shrink one
// that's bigger than the whole map (unlike the rect branch right above it, which always shrank
// via ClampLengthToMapBounds) - so shrinking the map down could leave a line hanging out past
// the far edge. Fixed via GridPlacement::RemapLineToBoundingBox, the same helper
// ResizeableArrowItem::SyncFromCollisionItem() uses for paste/load. Also confirms
// ItemPositionData::Save/Restore captures a line's full shape (not just position), so undo
// restores the exact pre-shrink oversized line rather than merely un-repositioning a still-
// shrunk one.
TEST(EditorAutomation, ChangeMapSizeForcesCollisionLineInsideBoundsWithUndoRedo)
{
    QProcess editor;
    AutomationClient client;
    QString socketName;
    ASSERT_TRUE(LaunchEditorAndConnect(editor, client, socketName));

    ASSERT_TRUE(CreateNewPath(client));
    ASSERT_TRUE(AddCollisionLine(client));

    // Grow to a 3-wide map (AO's camera grid cell is 1024x480 - see Model::CameraGridWidth) so
    // there's room to stretch the line bigger than a single camera cell.
    ASSERT_TRUE(SetMapSize(client, 3, 1));
    ASSERT_EQ(CountKind(GetSceneItems(client), "camera"), 3) << "map did not grow to 3 cameras wide";

    {
        const nlohmann::json before = FindKind(GetSceneItems(client), "collision_line");
        DragView(client, SceneToView(client, before.at("x2").get<int>(), before.at("y2").get<int>()),
                  SceneToView(client, 3000, before.at("y2").get<int>()));
    }

    const nlohmann::json oversized = FindKind(GetSceneItems(client), "collision_line");
    const int oversizedWidth = std::abs(oversized.at("x2").get<int>() - oversized.at("x1").get<int>());
    ASSERT_GT(oversizedWidth, 1024) << "test setup failed to make the line wider than a single camera";

    // Shrink back down to a single 1x1 camera: the line must shrink to fit AND reposition, not
    // just get left hanging outside the map.
    ASSERT_TRUE(SetMapSize(client, 1, 1));
    ASSERT_EQ(CountKind(GetSceneItems(client), "camera"), 1) << "map did not shrink to a single camera";

    const nlohmann::json shrunk = FindKind(GetSceneItems(client), "collision_line");
    const int shrunkWidth = std::abs(shrunk.at("x2").get<int>() - shrunk.at("x1").get<int>());
    const int shrunkMinX = std::min(shrunk.at("x1").get<int>(), shrunk.at("x2").get<int>());
    const int shrunkMaxX = std::max(shrunk.at("x1").get<int>(), shrunk.at("x2").get<int>());
    EXPECT_LE(shrunkWidth, 1024) << "oversized line was not shrunk to fit the map";
    EXPECT_GE(shrunkMinX, 0);
    EXPECT_LE(shrunkMaxX, 1024) << "line still extends past the map's right edge";

    // Undo must restore both the map size and the line's exact pre-shrink oversized shape - not
    // just whichever position it happened to have after the clamp.
    ASSERT_TRUE(client.Call({{"cmd", "click"}, {"target", "action_undo"}}).value("ok", false));
    ASSERT_EQ(CountKind(GetSceneItems(client), "camera"), 3) << "undo did not restore the 3-wide map";
    {
        const nlohmann::json afterUndo = FindKind(GetSceneItems(client), "collision_line");
        EXPECT_EQ(afterUndo.value("x1", -1), oversized.at("x1").get<int>()) << "undo did not restore the line's pre-shrink position/shape";
        EXPECT_EQ(afterUndo.value("y1", -1), oversized.at("y1").get<int>());
        EXPECT_EQ(afterUndo.value("x2", -1), oversized.at("x2").get<int>());
        EXPECT_EQ(afterUndo.value("y2", -1), oversized.at("y2").get<int>());
    }

    // Redo must re-apply the exact same shrink+reposition.
    ASSERT_TRUE(client.Call({{"cmd", "click"}, {"target", "action_redo"}}).value("ok", false));
    ASSERT_EQ(CountKind(GetSceneItems(client), "camera"), 1) << "redo did not reapply the 1-wide map";
    {
        const nlohmann::json afterRedo = FindKind(GetSceneItems(client), "collision_line");
        EXPECT_EQ(afterRedo.value("x1", -1), shrunk.at("x1").get<int>()) << "redo did not reapply the same clamp";
        EXPECT_EQ(afterRedo.value("y1", -1), shrunk.at("y1").get<int>());
        EXPECT_EQ(afterRedo.value("x2", -1), shrunk.at("x2").get<int>());
        EXPECT_EQ(afterRedo.value("y2", -1), shrunk.at("y2").get<int>());
    }

    editor.terminate();
    ASSERT_TRUE(editor.waitForFinished(5000)) << "editor did not exit after terminate()";
}

// Regression test for two camera bugs: CameraManager::CreateCamera used to save a brand new
// camera's image under the wrong (still-empty) name, orphaning it from the camera being
// created; and Model::ToJson() used mId != 0 as a "does this camera exist" check, which
// silently dropped a legitimately-new camera whose id happens to be 0 (the very first camera
// in a path whose path id is also 0) from the saved json entirely. Also covers the FG1
// "<camName>.json" layers sidecar, which nothing ever wrote before (CameraGraphicsItem::Load
// only ever reads it) - saves a small sample image generated here rather than depending on a
// checked-in test asset.
