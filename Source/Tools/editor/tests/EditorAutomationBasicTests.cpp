// Editor automation tests: opening/closing, new paths, dragging/resizing single items, save/reload and grid size.
// Helpers and main() are in EditorAutomationTestHelpers.cpp.

#include "EditorAutomationTestHelpers.hpp"

using namespace AutomationTest;

TEST(EditorAutomation, OpenCloseAboutAndExit)
{
    QProcess editor;
    AutomationClient client;
    QString socketName;
    ASSERT_TRUE(LaunchEditorAndConnect(editor, client, socketName));

    ASSERT_TRUE(client.Call({{"cmd", "ping"}}).value("ok", false));

    {
        const auto resp = client.Call({{"cmd", "list_actions"}});
        ASSERT_TRUE(resp.value("ok", false));

        bool foundAbout = false;
        for (const auto& action : resp.at("result"))
        {
            if (action.value("objectName", std::string()) == "action_about")
            {
                foundAbout = true;
                EXPECT_TRUE(action.value("enabled", false));
            }
        }
        ASSERT_TRUE(foundAbout) << "action_about not found in list_actions result";
    }

    // click on action_about opens a modal AboutDialog (AboutDialog::exec()), so this
    // response won't arrive until the dialog is dismissed below - don't wait for it yet.
    const int clickAboutId = client.SendCommand({{"cmd", "click"}, {"target", "action_about"}});

    {
        const auto resp = client.Call({{"cmd", "get_state"}, {"target", "AboutDialog"}});
        ASSERT_TRUE(resp.value("ok", false)) << "AboutDialog did not open: " << resp.value("error", std::string());
    }

    EXPECT_TRUE(client.Call({{"cmd", "close"}, {"target", "AboutDialog"}}).value("ok", false));
    EXPECT_TRUE(client.WaitForResponse(clickAboutId, 5000).value("ok", false));

    {
        const auto resp = client.Call({{"cmd", "get_state"}});
        ASSERT_TRUE(resp.value("ok", false));
        for (const auto& child : resp.at("result").value("children", nlohmann::json::array()))
        {
            EXPECT_NE(child.value("objectName", std::string()), "AboutDialog") << "AboutDialog still present after close";
        }
    }

    EXPECT_TRUE(client.Call({{"cmd", "close"}}).value("ok", false));

    ASSERT_TRUE(editor.waitForFinished(10000)) << "editor did not exit after close";
    EXPECT_EQ(editor.exitStatus(), QProcess::NormalExit);
    EXPECT_EQ(editor.exitCode(), 0);
}

// action_exit_application (the Exit menu item) previously had no wired-up slot at all, so
// clicking it silently did nothing. This exercises the fix directly, rather than via the
// generic "close" command used above.
TEST(EditorAutomation, ExitActionClosesWindow)
{
    QProcess editor;
    AutomationClient client;
    QString socketName;
    ASSERT_TRUE(LaunchEditorAndConnect(editor, client, socketName));

    {
        const auto resp = client.Call({{"cmd", "list_actions"}});
        ASSERT_TRUE(resp.value("ok", false));

        bool foundExit = false;
        for (const auto& action : resp.at("result"))
        {
            if (action.value("objectName", std::string()) == "action_exit_application")
            {
                foundExit = true;
                EXPECT_TRUE(action.value("enabled", false));
            }
        }
        ASSERT_TRUE(foundExit) << "action_exit_application not found in list_actions result";
    }

    EXPECT_TRUE(client.Call({{"cmd", "click"}, {"target", "action_exit_application"}}).value("ok", false));

    ASSERT_TRUE(editor.waitForFinished(10000)) << "editor did not exit after clicking Exit";
    EXPECT_EQ(editor.exitStatus(), QProcess::NormalExit);
    EXPECT_EQ(editor.exitCode(), 0);
}

// Exercises the core level-editing workflow: create a new path, add a collision line, add a
// map object. Success is verified through the undo history (QUndoView "undoView"), since the
// added collision line and object live in the tab's QGraphicsScene, which isn't part of the
// QWidget/QAction tree get_state can see.
TEST(EditorAutomation, NewPathAddCollisionAddObject)
{
    QProcess editor;
    AutomationClient client;
    QString socketName;
    ASSERT_TRUE(LaunchEditorAndConnect(editor, client, socketName));

    ASSERT_TRUE(CreateNewPath(client));

    {
        const auto resp = client.Call({{"cmd", "get_state"}, {"target", "tabWidget"}});
        ASSERT_TRUE(resp.value("ok", false));
        EXPECT_TRUE(ContainsClassName(resp.at("result"), "EditorTab")) << "new path tab not found under tabWidget";
    }

    ASSERT_TRUE(AddCollisionLine(client));
    {
        const auto items = GetUndoWidgetTextList(client);
        EXPECT_TRUE(UndoContains(items, "Add collision line")) << "undo history does not show the added collision line";
    }

    ASSERT_TRUE(AddMapObject(client));
    {
        const auto items = GetUndoWidgetTextList(client);
        EXPECT_TRUE(UndoContainsPrefix(items, "Add new object ")) << "undo history does not show the added object";
    }

    // Unlike the other tests, this tab now has real unsaved changes, so "close" hits
    // closeEvent()'s other branch: an unsaved-changes QMessageBox (Save/Cancel/Discard) that
    // blocks the close until answered. Its buttons have no objectName (and the QMessageBox
    // itself has no parent, so it isn't reachable from root by descending the widget tree
    // either) - resolved by visible text via "@active_modal_button:" instead. The exact label
    // is platform-theme-dependent (confirmed "Discard" under QT_QPA_PLATFORM=offscreen, which
    // these tests run under; a GTK-integrated desktop session showed "Close without Saving"
    // for the same button).
    const int closeClickId = client.SendCommand({{"cmd", "close"}});
    ASSERT_TRUE(client.Call({{"cmd", "click"}, {"target", "@active_modal_button:Discard"}}).value("ok", false))
        << "no unsaved-changes prompt to dismiss";
    EXPECT_TRUE(client.WaitForResponse(closeClickId, 5000).value("ok", false));

    ASSERT_TRUE(editor.waitForFinished(10000)) << "editor did not exit after close";
    EXPECT_EQ(editor.exitStatus(), QProcess::NormalExit);
    EXPECT_EQ(editor.exitCode(), 0);
}

// Exercises interactive dragging of a collision line: each endpoint moves independently,
// dragging out of bounds clamps rather than escaping the map (or collapsing to a point -
// see GridPlacement::ClampRangeStartToMapBounds), dragging the middle moves the whole line
// as a rigid unit, and grid-snapping actually snaps when enabled. Coordinates are always
// read back via get_scene_items / derived via scene_to_view rather than hardcoded, since the
// "offscreen" Qt platform's virtual screen size (and so the view's initial scroll position)
// isn't the same as a real display's.
TEST(EditorAutomation, CollisionLineDragBoundsAndSnap)
{
    QProcess editor;
    AutomationClient client;
    QString socketName;
    ASSERT_TRUE(LaunchEditorAndConnect(editor, client, socketName));

    ASSERT_TRUE(CreateNewPath(client));
    ASSERT_TRUE(AddCollisionLine(client));

    auto getCollisionLine = [&]() -> nlohmann::json
    { return FindKind(GetSceneItems(client), "collision_line"); };

    const nlohmann::json baseline = getCollisionLine();
    ASSERT_TRUE(baseline.contains("x1"));
    const int baseX1 = baseline.at("x1").get<int>();
    const int baseY1 = baseline.at("y1").get<int>();
    const int baseX2 = baseline.at("x2").get<int>();
    const int baseY2 = baseline.at("y2").get<int>();
    EXPECT_NE(baseX1, baseX2) << "freshly created collision line is zero-length";

    // 1. Endpoint drag, in bounds: P2 should move to exactly the target; P1 stays put.
    {
        const QPoint from = SceneToView(client, baseX2, baseY2);
        const int targetX = baseX2 + 50;
        const int targetY = baseY2 + 30;
        DragView(client, from, SceneToView(client, targetX, targetY));

        const nlohmann::json line = getCollisionLine();
        EXPECT_EQ(line.value("x1", -1), baseX1) << "P1 should not have moved";
        EXPECT_EQ(line.value("y1", -1), baseY1);
        EXPECT_EQ(line.value("x2", -1), targetX);
        EXPECT_EQ(line.value("y2", -1), targetY);
    }

    // 2. Endpoint drag, out of bounds: dragging P2 far past the map edge must clamp it back
    // in, not let it escape and not collapse the line to a single point.
    {
        const nlohmann::json before = getCollisionLine();
        const QPoint from = SceneToView(client, before.at("x2").get<int>(), before.at("y2").get<int>());
        DragView(client, from, SceneToView(client, 1'000'000, 1'000'000));

        const nlohmann::json line = getCollisionLine();
        const int x1 = line.value("x1", -1);
        const int x2 = line.value("x2", -1);
        const int y2 = line.value("y2", -1);
        EXPECT_NE(x2, x1) << "line collapsed to a zero-length point when dragged out of bounds";
        EXPECT_LT(x2, 100000) << "endpoint escaped the map bounds";
        EXPECT_LT(y2, 100000) << "endpoint escaped the map bounds";
    }

    // 3. Whole-line drag, in bounds: grabbing the middle (away from both endpoints, so
    // CalcWhichEndOfLineClicked falls into the whole-line branch with no modifier needed)
    // moves both endpoints by the same delta - a rigid translation.
    int shapeWidth = 0;
    int shapeHeight = 0;
    {
        const nlohmann::json before = getCollisionLine();
        const int x1 = before.at("x1").get<int>();
        const int y1 = before.at("y1").get<int>();
        const int x2 = before.at("x2").get<int>();
        const int y2 = before.at("y2").get<int>();
        shapeWidth = x2 - x1;
        shapeHeight = y2 - y1;

        // Shift toward the map's origin (negative), not away from it: step 2 just dragged P2
        // hard against the map's far edge, so the line's bounding box is already pinned there
        // - shifting further in that direction would legitimately have nowhere to go and get
        // clamped (that's step 4's job), which would make this "unclamped" case flaky.
        const int midX = (x1 + x2) / 2;
        const int midY = (y1 + y2) / 2;
        const int targetMidX = midX - 20;
        const int targetMidY = midY - 20;
        DragView(client, SceneToView(client, midX, midY), SceneToView(client, targetMidX, targetMidY));

        const nlohmann::json line = getCollisionLine();
        EXPECT_EQ(line.value("x1", -1), x1 + (targetMidX - midX)) << "whole-line drag did not translate P1 correctly";
        EXPECT_EQ(line.value("y1", -1), y1 + (targetMidY - midY));
        EXPECT_EQ(line.value("x2", -1), x2 + (targetMidX - midX)) << "whole-line drag did not translate P2 correctly";
        EXPECT_EQ(line.value("y2", -1), y2 + (targetMidY - midY));
    }

    // 4. Whole-line drag, out of bounds: dragging the middle far off the map must keep the
    // line fully in bounds *and* preserve its exact shape (not clamp each endpoint
    // independently, which could distort or collapse it).
    {
        const nlohmann::json before = getCollisionLine();
        const int midX = (before.at("x1").get<int>() + before.at("x2").get<int>()) / 2;
        const int midY = (before.at("y1").get<int>() + before.at("y2").get<int>()) / 2;
        DragView(client, SceneToView(client, midX, midY), SceneToView(client, -1'000'000, -1'000'000));

        const nlohmann::json line = getCollisionLine();
        const int x1 = line.value("x1", -1);
        const int y1 = line.value("y1", -1);
        const int x2 = line.value("x2", -1);
        const int y2 = line.value("y2", -1);
        EXPECT_GE(x1, 0) << "line escaped the map bounds";
        EXPECT_GE(y1, 0) << "line escaped the map bounds";
        EXPECT_LT(x1, 100000) << "line escaped the map bounds";
        EXPECT_LT(y1, 100000) << "line escaped the map bounds";
        EXPECT_EQ(x2 - x1, shapeWidth) << "line shape was not preserved while clamping";
        EXPECT_EQ(y2 - y1, shapeHeight) << "line shape was not preserved while clamping";
    }

    // 5. Snapping: enabling collision-line grid snap and dragging an endpoint to a
    // deliberately non-grid-aligned target should still land on the grid. EditorTab::SnapY
    // (Source/Tools/editor/Source/EditorTab.cpp) is a simple, documented (y / 20) * 20, which
    // this checks directly without duplicating the AO/AE fixed-point X-grid formula.
    {
        EXPECT_TRUE(client.Call({{"cmd", "click"}, {"target", "action_snap_collision_items_on_x"}}).value("ok", false));
        EXPECT_TRUE(client.Call({{"cmd", "click"}, {"target", "action_snap_collision_objects_on_y"}}).value("ok", false));

        const nlohmann::json before = getCollisionLine();
        const QPoint from = SceneToView(client, before.at("x2").get<int>(), before.at("y2").get<int>());
        // Deliberately not a multiple of 20 (or of the AO grid).
        DragView(client, from, SceneToView(client, before.at("x2").get<int>() + 47, before.at("y2").get<int>() + 47));

        const nlohmann::json line = getCollisionLine();
        const int y2 = line.value("y2", -1);
        EXPECT_EQ(y2 % 20, 0) << "endpoint did not snap to the Y grid: y2=" << y2;
    }

    editor.terminate();
    ASSERT_TRUE(editor.waitForFinished(5000)) << "editor did not exit after terminate()";
}

// Regression test: ResizeableArrowItem's whole-line body drag used to silently ignore
// CollisionSnapping entirely - only the single-endpoint resize branch actually called
// SnapX/SnapY (see CollisionLineDragBoundsAndSnap's step 5, which only drags an endpoint).
// Enables snap, grabs the line's middle (away from both endpoints, so this is a rigid whole-line
// translation, not a resize), and checks the whole line lands on the grid - not just repositioned
// but with its exact shape preserved, the same rigid-box-snap idea as a map object's body drag.
TEST(EditorAutomation, CollisionLineWholeLineDragSnapsToGrid)
{
    QProcess editor;
    AutomationClient client;
    QString socketName;
    ASSERT_TRUE(LaunchEditorAndConnect(editor, client, socketName));

    ASSERT_TRUE(CreateNewPath(client));
    ASSERT_TRUE(AddCollisionLine(client));

    ASSERT_TRUE(client.Call({{"cmd", "click"}, {"target", "action_snap_collision_items_on_x"}}).value("ok", false));
    ASSERT_TRUE(client.Call({{"cmd", "click"}, {"target", "action_snap_collision_objects_on_y"}}).value("ok", false));

    const nlohmann::json before = FindKind(GetSceneItems(client), "collision_line");
    const int x1 = before.at("x1").get<int>();
    const int y1 = before.at("y1").get<int>();
    const int x2 = before.at("x2").get<int>();
    const int y2 = before.at("y2").get<int>();
    const int shapeWidth = x2 - x1;
    const int shapeHeight = y2 - y1;

    const int midX = (x1 + x2) / 2;
    const int midY = (y1 + y2) / 2;
    // Deliberately not a multiple of 20 (or of the AO X grid).
    DragView(client, SceneToView(client, midX, midY), SceneToView(client, midX + 47, midY + 47));

    const nlohmann::json line = FindKind(GetSceneItems(client), "collision_line");
    const int newX1 = line.at("x1").get<int>();
    const int newY1 = line.at("y1").get<int>();
    const int newX2 = line.at("x2").get<int>();
    const int newY2 = line.at("y2").get<int>();

    EXPECT_EQ(newY1 % 20, 0) << "whole-line drag did not snap P1 to the Y grid: y1=" << newY1;
    EXPECT_EQ(newY2 % 20, 0) << "whole-line drag did not snap P2 to the Y grid: y2=" << newY2;
    // The drag must still be a rigid translation - the box snaps, not each endpoint
    // independently, which could distort the line's shape.
    EXPECT_EQ(newX2 - newX1, shapeWidth) << "whole-line drag snap distorted the line's shape";
    EXPECT_EQ(newY2 - newY1, shapeHeight);

    editor.terminate();
    ASSERT_TRUE(editor.waitForFinished(5000)) << "editor did not exit after terminate()";
}

// Multi-select desync bug (see MultiSelectDragWithSnapKeepsRowAligned), but the grabbed item is
// the *line* this time - exercises the TranslateOtherSelectedItems call in
// ResizeableArrowItem::mouseMoveEvent's own whole-line body-drag branch (added alongside that
// branch's new snap support above), not just the rect arm the earlier multi-select tests cover.
TEST(EditorAutomation, MultiSelectDragWithSnapKeepsLineGrabbedSelectionAligned)
{
    QProcess editor;
    AutomationClient client;
    QString socketName;
    ASSERT_TRUE(LaunchEditorAndConnect(editor, client, socketName));

    ASSERT_TRUE(CreateNewPath(client));

    // Map object -> (600, 100), clear of the line's default (100,100)-(200,100) placement.
    ASSERT_TRUE(AddMapObject(client));
    {
        const nlohmann::json obj = FindSelectedKind(GetSceneItems(client), "map_object");
        ASSERT_FALSE(obj.empty());
        const double curX = obj.at("xpos").get<double>();
        const double curY = obj.at("ypos").get<double>();
        DragView(client, SceneToView(client, curX + 10, curY + 10), SceneToView(client, 610, 110));
    }

    ASSERT_TRUE(AddCollisionLine(client));

    const nlohmann::json objBefore = FindKind(GetSceneItems(client), "map_object");
    const nlohmann::json lineBefore = FindKind(GetSceneItems(client), "collision_line");

    // Rubber-band select both.
    DragView(client, SceneToView(client, 50, 50), SceneToView(client, 750, 250));
    ASSERT_EQ(CountKind(GetSceneItems(client), "map_object"), 1);
    ASSERT_EQ(CountKind(GetSceneItems(client), "collision_line"), 1);
    for (const auto& item : GetSceneItems(client))
    {
        const std::string kind = item.value("kind", std::string());
        if (kind != "map_object" && kind != "collision_line")
        {
            continue;
        }
        EXPECT_TRUE(item.value("selected", false)) << "rubber-band select did not select both items: " << item.dump();
    }

    ASSERT_TRUE(client.Call({{"cmd", "click"}, {"target", "action_snap_collision_items_on_x"}}).value("ok", false));
    ASSERT_TRUE(client.Call({{"cmd", "click"}, {"target", "action_snap_collision_objects_on_y"}}).value("ok", false));

    // Drag the line's middle by a deliberately non-grid-aligned amount.
    {
        const int midX = (lineBefore.at("x1").get<int>() + lineBefore.at("x2").get<int>()) / 2;
        const int midY = (lineBefore.at("y1").get<int>() + lineBefore.at("y2").get<int>()) / 2;
        DragView(client, SceneToView(client, midX, midY), SceneToView(client, midX + 47, midY + 47));
    }

    ASSERT_TRUE(client.Call({{"cmd", "click"}, {"target", "action_snap_collision_items_on_x"}}).value("ok", false));
    ASSERT_TRUE(client.Call({{"cmd", "click"}, {"target", "action_snap_collision_objects_on_y"}}).value("ok", false));

    const nlohmann::json lineAfter = FindKind(GetSceneItems(client), "collision_line");
    const nlohmann::json objAfter = FindKind(GetSceneItems(client), "map_object");

    // The dx/dy the grabbed line actually moved by (raw drag delta + snap correction).
    const double dx = lineAfter.at("x1").get<int>() - lineBefore.at("x1").get<int>();
    const double dy = lineAfter.at("y1").get<int>() - lineBefore.at("y1").get<int>();
    ASSERT_NE(dy, 47) << "test setup: snap did not actually change the raw 47px Y drag - nothing to desync";

    EXPECT_DOUBLE_EQ(objAfter.at("xpos").get<double>() - objBefore.at("xpos").get<double>(), dx)
        << "map object desynced from the grabbed line after a snapped multi-select drag";
    EXPECT_DOUBLE_EQ(objAfter.at("ypos").get<double>() - objBefore.at("ypos").get<double>(), dy);

    editor.terminate();
    ASSERT_TRUE(editor.waitForFinished(5000)) << "editor did not exit after terminate()";
}

// Exercises interactive dragging of a map object's box (ResizeableRectItem): moving the whole
// box, resizing via a corner handle, and bounds-clamping for both - added alongside the same
// clamping for AddNewObjectCommand's initial placement and ResizeableArrowItem's collision-line
// dragging. Move tests run before the resize-to-extremes tests below, since growing/shrinking
// the box to the map's edges (deliberately, to test clamping) leaves it with no room left to
// move - order matters here, not just what's tested.
TEST(EditorAutomation, MapObjectDragResizeBoundsAndSnap)
{
    QProcess editor;
    AutomationClient client;
    QString socketName;
    ASSERT_TRUE(LaunchEditorAndConnect(editor, client, socketName));

    ASSERT_TRUE(CreateNewPath(client));
    ASSERT_TRUE(AddMapObject(client));

    auto getMapObject = [&]() -> nlohmann::json
    { return FindKind(GetSceneItems(client), "map_object"); };

    const nlohmann::json baseline = getMapObject();
    ASSERT_TRUE(baseline.contains("xpos"));
    const double baseX = baseline.at("xpos").get<double>();
    const double baseY = baseline.at("ypos").get<double>();
    const double baseW = baseline.at("width").get<double>();
    const double baseH = baseline.at("height").get<double>();
    EXPECT_GT(baseW, 0) << "freshly created map object has zero/negative width";
    EXPECT_GT(baseH, 0) << "freshly created map object has zero/negative height";

    // 1. Move, in bounds: drag the body (not a resize handle - well inside the box) by a
    // modest delta; size must stay exactly the same, position shifts by exactly the delta.
    {
        const double midX = baseX + baseW / 2;
        const double midY = baseY + baseH / 2;
        DragView(client, SceneToView(client, midX, midY), SceneToView(client, midX + 30, midY + 20));

        const nlohmann::json rect = getMapObject();
        EXPECT_DOUBLE_EQ(rect.value("xpos", -1.0), baseX + 30) << "body drag did not move the box to the right place";
        EXPECT_DOUBLE_EQ(rect.value("ypos", -1.0), baseY + 20);
        EXPECT_DOUBLE_EQ(rect.value("width", -1.0), baseW) << "body drag changed the box size";
        EXPECT_DOUBLE_EQ(rect.value("height", -1.0), baseH);
    }

    // 2. Snapping: enable map-object grid snap and move by a deliberately non-grid-aligned
    // delta; same EditorTab::SnapY formula as the collision-line test, checked the same way.
    {
        EXPECT_TRUE(client.Call({{"cmd", "click"}, {"target", "action_snap_map_objects_x"}}).value("ok", false));
        EXPECT_TRUE(client.Call({{"cmd", "click"}, {"target", "action_snap_map_objects_y"}}).value("ok", false));

        const nlohmann::json before = getMapObject();
        const double x = before.at("xpos").get<double>();
        const double y = before.at("ypos").get<double>();
        const double w = before.at("width").get<double>();
        const double h = before.at("height").get<double>();
        const double midX = x + w / 2;
        const double midY = y + h / 2;
        // Deliberately not a multiple of 20.
        DragView(client, SceneToView(client, midX, midY), SceneToView(client, midX + 47, midY + 47));

        const nlohmann::json rect = getMapObject();
        const int ypos = static_cast<int>(rect.value("ypos", -1.0));
        EXPECT_EQ(ypos % 20, 0) << "box did not snap to the Y grid: ypos=" << ypos;

        EXPECT_TRUE(client.Call({{"cmd", "click"}, {"target", "action_snap_map_objects_x"}}).value("ok", false));
        EXPECT_TRUE(client.Call({{"cmd", "click"}, {"target", "action_snap_map_objects_y"}}).value("ok", false));
    }

    // 3. Resize, in bounds: drag the bottom-right corner handle by a modest amount; top-left
    // stays put, width/height grow by exactly the delta. Runs before any of the out-of-bounds
    // tests below, which deliberately pin the box against an edge (e.g. a move clamped so
    // x+width sits exactly at the map's last valid pixel leaves zero room left to grow, which
    // would make an "in bounds" resize check here flaky if it ran afterward).
    {
        const nlohmann::json before = getMapObject();
        const double x = before.at("xpos").get<double>();
        const double y = before.at("ypos").get<double>();
        const double w = before.at("width").get<double>();
        const double h = before.at("height").get<double>();
        DragView(client, SceneToView(client, x + w, y + h), SceneToView(client, x + w + 40, y + h + 25));

        const nlohmann::json rect = getMapObject();
        EXPECT_DOUBLE_EQ(rect.value("xpos", -1.0), x) << "resize moved the anchored top-left corner";
        EXPECT_DOUBLE_EQ(rect.value("ypos", -1.0), y);
        EXPECT_DOUBLE_EQ(rect.value("width", -1.0), w + 40) << "bottom-right corner resize did not grow width correctly";
        EXPECT_DOUBLE_EQ(rect.value("height", -1.0), h + 25);
    }

    // 4. Resize, out of bounds (grow): drag the bottom-right corner far past the map edge; it
    // must clamp to the map's last valid pixel, not escape, and stay above the minimum size
    // (ResizeableRectItem::kMinRectSize).
    {
        const nlohmann::json before = getMapObject();
        const double x = before.at("xpos").get<double>();
        const double y = before.at("ypos").get<double>();
        const double w = before.at("width").get<double>();
        const double h = before.at("height").get<double>();
        DragView(client, SceneToView(client, x + w, y + h), SceneToView(client, 1'000'000, 1'000'000));

        const nlohmann::json rect = getMapObject();
        EXPECT_DOUBLE_EQ(rect.value("xpos", -1.0), x) << "resize moved the anchored top-left corner";
        EXPECT_DOUBLE_EQ(rect.value("ypos", -1.0), y);
        EXPECT_LT(rect.value("width", 0.0) + x, 100000) << "box escaped the map bounds";
        EXPECT_LT(rect.value("height", 0.0) + y, 100000) << "box escaped the map bounds";
        EXPECT_GE(rect.value("width", 0.0), 10) << "box shrank below the minimum size while clamping";
        EXPECT_GE(rect.value("height", 0.0), 10);
    }

    // 5. Resize, out of bounds (shrink past the anchor): drag the bottom-right corner back past
    // the top-left corner - width/height must clamp to the minimum size, not go to zero or
    // negative (pre-existing kMinRectSize behavior; regression-checked since the new bounds
    // clamp shares this code path).
    {
        const nlohmann::json before = getMapObject();
        const double x = before.at("xpos").get<double>();
        const double y = before.at("ypos").get<double>();
        const double w = before.at("width").get<double>();
        const double h = before.at("height").get<double>();
        DragView(client, SceneToView(client, x + w, y + h), SceneToView(client, x - 500, y - 500));

        const nlohmann::json rect = getMapObject();
        EXPECT_GE(rect.value("width", -1.0), 10) << "box did not stay at/above the minimum size";
        EXPECT_GE(rect.value("height", -1.0), 10);
    }

    // 6. Resize, out of bounds (top-left corner toward negative): the opposite corner (bottom-
    // right) is the anchor here, so this covers the ClampX/ClampY (not ClampRangeStart) side of
    // the resize clamp.
    {
        const nlohmann::json before = getMapObject();
        const double anchorRight = before.at("xpos").get<double>() + before.at("width").get<double>();
        const double anchorBottom = before.at("ypos").get<double>() + before.at("height").get<double>();
        DragView(client, SceneToView(client, before.at("xpos").get<double>(), before.at("ypos").get<double>()), SceneToView(client, -1'000'000, -1'000'000));

        const nlohmann::json rect = getMapObject();
        EXPECT_DOUBLE_EQ(rect.value("xpos", -1.0), 0) << "top-left corner should clamp to the map's origin";
        EXPECT_DOUBLE_EQ(rect.value("ypos", -1.0), 0);
        EXPECT_DOUBLE_EQ(rect.value("xpos", -1.0) + rect.value("width", -1.0), anchorRight) << "resize moved the anchored bottom-right corner";
        EXPECT_DOUBLE_EQ(rect.value("ypos", -1.0) + rect.value("height", -1.0), anchorBottom);
    }

    // 7. Move, out of bounds: drag the body far off the map; it must clamp to stay fully in
    // bounds while keeping its exact size (not get resized in the process). Runs last since
    // clamping it against an edge leaves no room for a further "in bounds" test afterward.
    {
        const nlohmann::json before = getMapObject();
        const double x = before.at("xpos").get<double>();
        const double y = before.at("ypos").get<double>();
        const double w = before.at("width").get<double>();
        const double h = before.at("height").get<double>();
        DragView(client, SceneToView(client, x + w / 2, y + h / 2), SceneToView(client, 1'000'000, 1'000'000));

        const nlohmann::json rect = getMapObject();
        EXPECT_LT(rect.value("xpos", -1.0), 100000) << "box escaped the map bounds";
        EXPECT_LT(rect.value("ypos", -1.0), 100000) << "box escaped the map bounds";
        EXPECT_DOUBLE_EQ(rect.value("width", -1.0), w) << "out-of-bounds move resized the box";
        EXPECT_DOUBLE_EQ(rect.value("height", -1.0), h);
    }

    editor.terminate();
    ASSERT_TRUE(editor.waitForFinished(5000)) << "editor did not exit after terminate()";
}

// Exercises the actual save/load round-trip (JSON on disk), not just in-memory state: create a
// collision line and a map object, move each to a non-default position, save to a temp file
// (bypassing the real QFileDialog-based Save As via the "save_path_as" domain command - see
// AutomationSceneCommands.cpp), then open that same file into a new tab ("open_path", same
// idea for the Open flow) and verify the reloaded positions match exactly. QTemporaryDir
// deletes the file (and its containing directory) automatically once it goes out of scope,
// regardless of how the test exits.
TEST(EditorAutomation, SavePathThenReopenPreservesPositions)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid()) << "failed to create a temp directory";
    const QString savePath = tempDir.filePath("relive_editor_test_level.json");

    QProcess editor;
    AutomationClient client;
    QString socketName;
    ASSERT_TRUE(LaunchEditorAndConnect(editor, client, socketName));

    ASSERT_TRUE(CreateNewPath(client));
    ASSERT_TRUE(AddCollisionLine(client));
    ASSERT_TRUE(AddMapObject(client));

    // Move both items to non-default positions, so this actually verifies arbitrary state
    // round-trips through JSON rather than just the fixed creation position.
    {
        const nlohmann::json line = FindKind(GetSceneItems(client), "collision_line");
        DragView(client, SceneToView(client, line.at("x2").get<int>(), line.at("y2").get<int>()),
                 SceneToView(client, line.at("x2").get<int>() + 60, line.at("y2").get<int>() + 35));
    }
    {
        const nlohmann::json rect = FindKind(GetSceneItems(client), "map_object");
        const double midX = rect.at("xpos").get<double>() + rect.at("width").get<double>() / 2;
        const double midY = rect.at("ypos").get<double>() + rect.at("height").get<double>() / 2;
        DragView(client, SceneToView(client, midX, midY), SceneToView(client, midX + 45, midY - 25));
    }

    const nlohmann::json itemsBeforeSave = GetSceneItems(client);
    const nlohmann::json lineBeforeSave = FindKind(itemsBeforeSave, "collision_line");
    const nlohmann::json rectBeforeSave = FindKind(itemsBeforeSave, "map_object");

    {
        const auto resp = client.Call({{"cmd", "save_path_as"}, {"path", savePath.toStdString()}});
        ASSERT_TRUE(resp.value("ok", false));
        ASSERT_TRUE(resp.at("result").value("saved", false)) << "save_path_as reported failure";
    }

    ASSERT_TRUE(QFile::exists(savePath)) << "save_path_as did not create a file at " << savePath.toStdString();
    EXPECT_GT(QFileInfo(savePath).size(), 0) << "saved file is empty";

    {
        const auto resp = client.Call({{"cmd", "open_path"}, {"path", savePath.toStdString()}});
        ASSERT_TRUE(resp.value("ok", false));
        ASSERT_TRUE(resp.at("result").value("opened", false)) << "open_path reported failure";
    }

    // open_path makes the reopened file the active tab, so get_scene_items now reads it back.
    const nlohmann::json itemsAfterReopen = GetSceneItems(client);
    const nlohmann::json lineAfterReopen = FindKind(itemsAfterReopen, "collision_line");
    const nlohmann::json rectAfterReopen = FindKind(itemsAfterReopen, "map_object");

    EXPECT_EQ(lineAfterReopen.value("x1", -1), lineBeforeSave.value("x1", -2)) << "collision line x1 did not round-trip";
    EXPECT_EQ(lineAfterReopen.value("y1", -1), lineBeforeSave.value("y1", -2)) << "collision line y1 did not round-trip";
    EXPECT_EQ(lineAfterReopen.value("x2", -1), lineBeforeSave.value("x2", -2)) << "collision line x2 did not round-trip";
    EXPECT_EQ(lineAfterReopen.value("y2", -1), lineBeforeSave.value("y2", -2)) << "collision line y2 did not round-trip";

    EXPECT_DOUBLE_EQ(rectAfterReopen.value("xpos", -1.0), rectBeforeSave.value("xpos", -2.0)) << "map object xpos did not round-trip";
    EXPECT_DOUBLE_EQ(rectAfterReopen.value("ypos", -1.0), rectBeforeSave.value("ypos", -2.0)) << "map object ypos did not round-trip";
    EXPECT_DOUBLE_EQ(rectAfterReopen.value("width", -1.0), rectBeforeSave.value("width", -2.0)) << "map object width did not round-trip";
    EXPECT_DOUBLE_EQ(rectAfterReopen.value("height", -1.0), rectBeforeSave.value("height", -2.0)) << "map object height did not round-trip";

    editor.terminate();
    ASSERT_TRUE(editor.waitForFinished(5000)) << "editor did not exit after terminate()";
}

// Regression test: Model::ToJson() used to only serialize a camera (and, with it, that
// camera's whole "map_objects" array) when EditorCamera::mName was non-empty. mName is only
// ever set once a camera is actually given an image via CameraManager::CreateCamera /
// NewCameraCommand - but AddObjectDialog lets a map object be added to a camera grid cell
// before it has an image at all, pushing straight onto that still-nameless camera's
// mMapObjects. So a map object added to such an "empty" camera (no image ever set) would
// vanish silently on the very next save_path_as + open_path round-trip, with no error either
// way - it was just gone.
TEST(EditorAutomation, AddMapObjectToEmptyCameraPersistsThroughSaveAndReload)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid()) << "failed to create a temp directory";
    const QString savePath = tempDir.filePath("relive_editor_test_level.json");

    QProcess editor;
    AutomationClient client;
    QString socketName;
    ASSERT_TRUE(LaunchEditorAndConnect(editor, client, socketName));

    ASSERT_TRUE(CreateNewPath(client));

    // Deliberately never call set_camera_image - the only camera in this fresh 1x1 path stays
    // "empty" (EditorCamera::mName unset) for the whole test.
    {
        const nlohmann::json cam = FindKind(GetSceneItems(client), "camera");
        ASSERT_TRUE(cam.value("camName", std::string()).empty()) << "camera should have no name/image yet";
        ASSERT_FALSE(cam.value("hasMainImage", false)) << "camera should have no image yet";
    }

    ASSERT_TRUE(AddMapObject(client));
    ASSERT_EQ(CountKind(GetSceneItems(client), "map_object"), 1);

    const nlohmann::json objBeforeSave = FindKind(GetSceneItems(client), "map_object");

    {
        const auto resp = client.Call({{"cmd", "save_path_as"}, {"path", savePath.toStdString()}});
        ASSERT_TRUE(resp.value("ok", false));
        ASSERT_TRUE(resp.at("result").value("saved", false)) << "save_path_as reported failure";
    }
    ASSERT_TRUE(QFile::exists(savePath)) << "save_path_as did not create a file at " << savePath.toStdString();

    // The saved JSON itself should list the camera (still nameless) and its map object -
    // catches the bug at the source, rather than only through the editor's own in-memory state.
    {
        QFile f(savePath);
        ASSERT_TRUE(f.open(QFile::ReadOnly | QFile::Text));
        const auto j = nlohmann::json::parse(f.readAll().toStdString());
        const auto& cameras = j.at("map").at("cameras");
        ASSERT_EQ(cameras.size(), 1u) << "the nameless camera itself was dropped from the saved JSON: " << j.dump();
        EXPECT_EQ(cameras.at(0).at("map_objects").size(), 1u) << "the map object was dropped from the saved JSON: " << j.dump();
    }

    {
        const auto resp = client.Call({{"cmd", "open_path"}, {"path", savePath.toStdString()}});
        ASSERT_TRUE(resp.value("ok", false));
        ASSERT_TRUE(resp.at("result").value("opened", false)) << "open_path reported failure";
    }

    // open_path makes the reopened file the active tab, so get_scene_items now reads it back.
    const nlohmann::json itemsAfterReopen = GetSceneItems(client);
    EXPECT_EQ(CountKind(itemsAfterReopen, "map_object"), 1) << "map object added to an empty camera did not survive a save+reload round-trip";

    const nlohmann::json objAfterReopen = FindKind(itemsAfterReopen, "map_object");
    EXPECT_DOUBLE_EQ(objAfterReopen.value("xpos", -1.0), objBeforeSave.value("xpos", -2.0)) << "map object xpos did not round-trip";
    EXPECT_DOUBLE_EQ(objAfterReopen.value("ypos", -1.0), objBeforeSave.value("ypos", -2.0)) << "map object ypos did not round-trip";

    editor.terminate();
    ASSERT_TRUE(editor.waitForFinished(5000)) << "editor did not exit after terminate()";
}

// A brand new path now defaults to a 4x4 camera grid rather than 1x1, so there's actually room
// to lay a level out without immediately needing "Edit map size" - see Model::CreateAsNewPath's
// header default. Exercises actionNew_path's own "width"/"height" QInputDialogs by accepting
// their defaults (Return, same idiom as the path id/game dialogs before them), rather than
// setting them via CreateNewPath's xSize/ySize args as every other test here does, so this is
// also the one place the production default itself - not just a test-chosen size - is checked.
TEST(EditorAutomation, NewPathViaMenuDefaultsToFourByFourGrid)
{
    QProcess editor;
    AutomationClient client;
    QString socketName;
    ASSERT_TRUE(LaunchEditorAndConnect(editor, client, socketName));

    const int newPathClickId = client.SendCommand({{"cmd", "click"}, {"target", "actionNew_path"}});
    ASSERT_TRUE(client.Call({{"cmd", "send_key"}, {"target", "@active_modal"}, {"key", "Return"}}).value("ok", false)) << "no active modal for path id dialog";
    ASSERT_TRUE(client.Call({{"cmd", "send_key"}, {"target", "@active_modal"}, {"key", "Return"}}).value("ok", false)) << "no active modal for game dialog";
    ASSERT_TRUE(client.Call({{"cmd", "send_key"}, {"target", "@active_modal"}, {"key", "Return"}}).value("ok", false)) << "no active modal for width dialog";
    ASSERT_TRUE(client.Call({{"cmd", "send_key"}, {"target", "@active_modal"}, {"key", "Return"}}).value("ok", false)) << "no active modal for height dialog";
    ASSERT_TRUE(client.WaitForResponse(newPathClickId, 5000).value("ok", false));

    EXPECT_EQ(CountKind(GetSceneItems(client), "camera"), 16) << "New Path did not default to a 4x4 grid";

    editor.terminate();
    ASSERT_TRUE(editor.waitForFinished(5000)) << "editor did not exit after terminate()";
}

// Regression test: Model::ToJson() never actually persisted the grid's own x_size/y_size -
// Model::LoadJsonFromString instead re-derived them from the highest x/y among whichever
// cameras got saved (CalculateMapSize). That happened to round-trip a wholly untouched 1x1 path
// correctly only by coincidence (0 cameras saved -> derived size 0,0 -> clamped up to 1x1), but
// silently shrank any bigger grid with an unused cell past its last saved camera - e.g. this
// 4x4 path, whose corner camera (3,3) never gets touched - down to whatever smaller area its
// saved cameras still spanned.
TEST(EditorAutomation, GridSizePersistsThroughSaveAndReloadEvenWithUntouchedCells)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid()) << "failed to create a temp directory";
    const QString savePath = tempDir.filePath("relive_editor_test_level.json");

    QProcess editor;
    AutomationClient client;
    QString socketName;
    ASSERT_TRUE(LaunchEditorAndConnect(editor, client, socketName));

    ASSERT_TRUE(CreateNewPath(client, 4, 4));
    ASSERT_EQ(CountKind(GetSceneItems(client), "camera"), 16) << "test setup: expected a 4x4 grid";

    {
        const auto resp = client.Call({{"cmd", "save_path_as"}, {"path", savePath.toStdString()}});
        ASSERT_TRUE(resp.value("ok", false));
        ASSERT_TRUE(resp.at("result").value("saved", false)) << "save_path_as reported failure";
    }

    // The saved JSON itself should record the grid size explicitly, not leave it to be inferred.
    {
        QFile f(savePath);
        ASSERT_TRUE(f.open(QFile::ReadOnly | QFile::Text));
        const auto j = nlohmann::json::parse(f.readAll().toStdString());
        EXPECT_EQ(j.at("map").value("x_size", -1), 4) << "grid width was not saved: " << j.dump();
        EXPECT_EQ(j.at("map").value("y_size", -1), 4) << "grid height was not saved: " << j.dump();
    }

    {
        const auto resp = client.Call({{"cmd", "open_path"}, {"path", savePath.toStdString()}});
        ASSERT_TRUE(resp.value("ok", false));
        ASSERT_TRUE(resp.at("result").value("opened", false)) << "open_path reported failure";
    }

    EXPECT_EQ(CountKind(GetSceneItems(client), "camera"), 16) << "grid shrank after a save+reload round-trip despite an untouched corner cell";

    editor.terminate();
    ASSERT_TRUE(editor.waitForFinished(5000)) << "editor did not exit after terminate()";
}

// Regression test for dragging an already-multi-selected group (a collision line + a map
// object, selected together via a rubber-band drag): only the one item actually grabbed used
// to get clamped to the map bounds (to its own bounds) - Qt's default QGraphicsItem::
// mouseMoveEvent moves every other selected item by the raw delta with no clamping at all, so
// the rest of the selection could end up outside the map, or (if clamped independently by the
// old per-item logic) with its shape/relative layout distorted. EditorGraphicsScene now
// clamps the union of the whole selection as a rigid group after the drag completes.
