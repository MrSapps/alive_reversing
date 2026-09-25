// Editor automation tests: mod tree <-> scene selection sync, tree expansion and restart behaviour.
// Helpers and main() are in EditorAutomationTestHelpers.cpp.

#include "EditorAutomationTestHelpers.hpp"

using namespace AutomationTest;

TEST(EditorAutomation, SceneSelectionSyncsToTreeSelection)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid()) << "failed to create a temp directory";
    const QString modDir = tempDir.filePath("MyMod");

    QProcess editor;
    AutomationClient client;
    QString socketName;
    ASSERT_TRUE(LaunchEditorAndConnect(editor, client, socketName));

    ASSERT_TRUE(CreateModWithLevelAndOpenPath(client, modDir));
    ASSERT_TRUE(AddMapObject(client));
    ASSERT_TRUE(AddMapObject(client));

    // The second AddMapObject leaves only the newly added object selected in the scene - the
    // tree should show exactly one selected row, and it should be a map object, not the first.
    const auto treeResp = client.Call({{"cmd", "get_tree_items"}, {"target", "modTreeWidget"}});
    ASSERT_TRUE(treeResp.value("ok", false));
    const auto& camerasGroup = treeResp.at("result").at("items")[0].at("children")[0].at("children")[0].at("children")[0];

    int selectedMapObjectRows = 0;
    for (const auto& camera : camerasGroup.at("children"))
    {
        for (const auto& mapObject : camera.at("children"))
        {
            if (mapObject.value("selected", false))
            {
                selectedMapObjectRows++;
            }
        }
    }
    EXPECT_EQ(selectedMapObjectRows, 1) << "tree selection does not match the scene's actual selection";

    StopEditor(editor);
    ASSERT_TRUE(editor.waitForFinished(5000)) << "editor did not exit after terminate()";
}

// The reverse direction: selecting a map object's row in the tree should select the matching
// item in the scene (ModTreeWidget::onItemSelectionChanged), the same way clicking it in the
// scene directly would.
TEST(EditorAutomation, TreeSelectionSyncsToSceneSelection)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid()) << "failed to create a temp directory";
    const QString modDir = tempDir.filePath("MyMod");

    QProcess editor;
    AutomationClient client;
    QString socketName;
    ASSERT_TRUE(LaunchEditorAndConnect(editor, client, socketName));

    ASSERT_TRUE(CreateModWithLevelAndOpenPath(client, modDir));
    ASSERT_TRUE(AddMapObject(client));

    const nlohmann::json objBefore = FindKind(GetSceneItems(client), "map_object");
    ASSERT_TRUE(objBefore.value("selected", false)) << "test setup: adding an object should leave it selected";

    // Deselect everything (click empty scene space, well away from the new object - which
    // AddObjectDialog places dead center of the view - so the upcoming tree click is what
    // actually does the selecting, not just leaving AddMapObject's own selection untouched).
    const QPoint emptySpot = SceneToView(client, 5, 5);
    ASSERT_TRUE(SendMouseEvent(client, "down", emptySpot));
    ASSERT_TRUE(SendMouseEvent(client, "up", emptySpot));
    ASSERT_FALSE(FindKind(GetSceneItems(client), "map_object").value("selected", false)) << "test setup: click on empty space should deselect";

    // Read the row's actual label straight from the tree rather than trying to reconstruct
    // MapObjectTreeItem's "TypeName (x, y)" format independently here.
    std::string rowText;
    {
        const auto treeResp = client.Call({{"cmd", "get_tree_items"}, {"target", "modTreeWidget"}});
        ASSERT_TRUE(treeResp.value("ok", false));
        const auto& camerasGroup = treeResp.at("result").at("items")[0].at("children")[0].at("children")[0].at("children")[0];
        for (const auto& camera : camerasGroup.at("children"))
        {
            if (!camera.at("children").empty())
            {
                rowText = camera.at("children")[0].value("text", std::string());
                break;
            }
        }
    }
    ASSERT_FALSE(rowText.empty()) << "test setup: no map object row found in the tree";

    ASSERT_TRUE(client.Call({{"cmd", "click_tree_item"}, {"target", "modTreeWidget"}, {"column", 0}, {"row_text", rowText}}).value("ok", false));

    const nlohmann::json objAfter = FindKind(GetSceneItems(client), "map_object");
    EXPECT_TRUE(objAfter.value("selected", false)) << "selecting the object's row in the tree did not select it in the scene";

    StopEditor(editor);
    ASSERT_TRUE(editor.waitForFinished(5000)) << "editor did not exit after terminate()";
}

// Regression test: selecting a row in the tree used to mutate the scene's selection directly,
// entirely bypassing the undo stack (unlike a real click in the scene, which always goes through
// SetSelectionCommand). That left the undo stack's own record of "what's selected" stale the
// moment a tree click changed it - so undoing some later, unrelated command could restore a
// selection from *before* the tree click, silently reintroducing an object the user had since
// deselected via the tree, or the other way around. onItemSelectionChanged now pushes a
// SetSelectionCommand of its own, the same way the scene's own mouse handlers do.
TEST(EditorAutomation, TreeSelectionIsUndoable)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid()) << "failed to create a temp directory";
    const QString modDir = tempDir.filePath("MyMod");

    QProcess editor;
    AutomationClient client;
    QString socketName;
    ASSERT_TRUE(LaunchEditorAndConnect(editor, client, socketName));

    ASSERT_TRUE(CreateModWithLevelAndOpenPath(client, modDir));
    ASSERT_TRUE(AddMapObject(client));

    // Deselect (see TreeSelectionSyncsToSceneSelection for why an empty click has to land well
    // away from the object AddObjectDialog placed dead center of the view).
    const QPoint emptySpot = SceneToView(client, 5, 5);
    ASSERT_TRUE(SendMouseEvent(client, "down", emptySpot));
    ASSERT_TRUE(SendMouseEvent(client, "up", emptySpot));
    ASSERT_FALSE(FindKind(GetSceneItems(client), "map_object").value("selected", false)) << "test setup: click on empty space should deselect";

    std::string rowText;
    {
        const auto treeResp = client.Call({{"cmd", "get_tree_items"}, {"target", "modTreeWidget"}});
        ASSERT_TRUE(treeResp.value("ok", false));
        const auto& camerasGroup = treeResp.at("result").at("items")[0].at("children")[0].at("children")[0].at("children")[0];
        for (const auto& camera : camerasGroup.at("children"))
        {
            if (!camera.at("children").empty())
            {
                rowText = camera.at("children")[0].value("text", std::string());
                break;
            }
        }
    }
    ASSERT_FALSE(rowText.empty()) << "test setup: no map object row found in the tree";

    ASSERT_TRUE(client.Call({{"cmd", "click_tree_item"}, {"target", "modTreeWidget"}, {"column", 0}, {"row_text", rowText}}).value("ok", false));
    ASSERT_TRUE(FindKind(GetSceneItems(client), "map_object").value("selected", false)) << "test setup: tree click should have selected the object";

    EXPECT_TRUE(UndoContainsPrefix(GetUndoWidgetTextList(client), "Select ")) << "the tree-driven selection was not recorded on the undo stack";

    ASSERT_TRUE(client.Call({{"cmd", "click"}, {"target", "action_undo"}}).value("ok", false));
    EXPECT_FALSE(FindKind(GetSceneItems(client), "map_object").value("selected", false)) << "undo did not revert the selection the tree click made";

    StopEditor(editor);
    ASSERT_TRUE(editor.waitForFinished(5000)) << "editor did not exit after terminate()";
}

// Regression test: RefreshPathSubtree tears down and recreates every row from scratch on any
// structural change (e.g. adding a map object) - a brand new QTreeWidgetItem always starts
// collapsed, so without explicitly restoring expansion state, something as unremarkable as
// adding one more object to a camera the user had already expanded to look at collapsed the
// whole "Cameras" group (and that camera's own row) shut around them.
TEST(EditorAutomation, TreeExpansionSurvivesAddingMapObject)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid()) << "failed to create a temp directory";
    const QString modDir = tempDir.filePath("MyMod");

    QProcess editor;
    AutomationClient client;
    QString socketName;
    ASSERT_TRUE(LaunchEditorAndConnect(editor, client, socketName));

    ASSERT_TRUE(CreateModWithLevelAndOpenPath(client, modDir));
    ASSERT_TRUE(AddMapObject(client));

    auto findCamerasGroupAndCamera = [&](const nlohmann::json& treeResp) -> std::pair<nlohmann::json, nlohmann::json>
    {
        const auto& camerasGroup = treeResp.at("result").at("items")[0].at("children")[0].at("children")[0].at("children")[0];
        for (const auto& camera : camerasGroup.at("children"))
        {
            if (!camera.at("children").empty())
            {
                return {camerasGroup, camera};
            }
        }
        return {camerasGroup, nlohmann::json::object()};
    };

    // Expand the "Cameras" group and the camera row holding the object added above by clicking
    // the object's own (nested) row - click_tree_item auto-expands every ancestor of whatever
    // row it targets so the click can actually land on it (see its own comment), which as a side
    // effect is also the only way available here to expand a GroupTreeItem/CameraTreeItem row at
    // all: ModTreeWidget deliberately disables Qt's built-in double-click-to-expand
    // (setExpandsOnDoubleClick(false)) and only handles a double-click itself for Level/Path rows.
    std::string objectRowText;
    {
        const auto treeResp = client.Call({{"cmd", "get_tree_items"}, {"target", "modTreeWidget"}});
        ASSERT_TRUE(treeResp.value("ok", false));
        const auto [camerasGroup, camera] = findCamerasGroupAndCamera(treeResp);
        ASSERT_FALSE(camera.at("children").empty()) << "test setup: no camera with a map object found";
        objectRowText = camera.at("children")[0].value("text", std::string());
    }
    ASSERT_TRUE(client.Call({{"cmd", "click_tree_item"}, {"target", "modTreeWidget"}, {"column", 0}, {"row_text", objectRowText}}).value("ok", false));
    {
        const auto treeResp = client.Call({{"cmd", "get_tree_items"}, {"target", "modTreeWidget"}});
        ASSERT_TRUE(treeResp.value("ok", false));
        const auto [camerasGroup, camera] = findCamerasGroupAndCamera(treeResp);
        ASSERT_TRUE(camerasGroup.value("expanded", false)) << "test setup: clicking the object row should expand its ancestors, including Cameras";
        ASSERT_TRUE(camera.value("expanded", false)) << "test setup: clicking the object row should expand its ancestors, including the camera";
    }

    // Adding a second object is a genuine structural change - RefreshPathSubtree really does
    // tear the whole subtree down and rebuild it - so this is exercising expansion-state
    // preservation across that rebuild, not just proving nothing happened at all.
    ASSERT_TRUE(AddMapObject(client));

    const auto treeResp = client.Call({{"cmd", "get_tree_items"}, {"target", "modTreeWidget"}});
    ASSERT_TRUE(treeResp.value("ok", false));
    const auto [camerasGroup, camera] = findCamerasGroupAndCamera(treeResp);
    EXPECT_TRUE(camerasGroup.value("expanded", false)) << "adding a map object collapsed the Cameras group";
    EXPECT_TRUE(camera.value("expanded", false)) << "adding a map object collapsed the camera row the new object landed in";

    StopEditor(editor);
    ASSERT_TRUE(editor.waitForFinished(5000)) << "editor did not exit after terminate()";
}

// Regression test: adding the very first object to a camera that had nothing in it before (so
// its row - and the "Cameras" group above it - had never been expanded, nothing to preserve)
// used to leave the new object sitting selected but invisible, collapsed away under rows nobody
// had a reason to open yet. AddNewObjectCommand selects the object it just created, and
// SyncTreeSelectionFromScene now expands a selected row's ancestors as part of syncing the
// tree's highlighting to match (see ExpandAncestors) - not just preserving expansion state that
// already existed, which is all TreeExpansionSurvivesAddingMapObject covers.
TEST(EditorAutomation, AddingFirstMapObjectAutoExpandsTreeToRevealIt)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid()) << "failed to create a temp directory";
    const QString modDir = tempDir.filePath("MyMod");

    QProcess editor;
    AutomationClient client;
    QString socketName;
    ASSERT_TRUE(LaunchEditorAndConnect(editor, client, socketName));

    ASSERT_TRUE(CreateModWithLevelAndOpenPath(client, modDir));

    const auto treeRespBefore = client.Call({{"cmd", "get_tree_items"}, {"target", "modTreeWidget"}});
    ASSERT_TRUE(treeRespBefore.value("ok", false));
    ASSERT_FALSE(treeRespBefore.at("result").at("items")[0].at("children")[0].at("children")[0].at("children")[0].value("expanded", false))
        << "test setup: Cameras group should start collapsed";

    ASSERT_TRUE(AddMapObject(client));

    const auto treeResp = client.Call({{"cmd", "get_tree_items"}, {"target", "modTreeWidget"}});
    ASSERT_TRUE(treeResp.value("ok", false));
    const auto& camerasGroup = treeResp.at("result").at("items")[0].at("children")[0].at("children")[0].at("children")[0];
    ASSERT_TRUE(camerasGroup.value("expanded", false)) << "adding the first map object did not expand the Cameras group";

    bool foundExpandedCameraWithObject = false;
    for (const auto& camera : camerasGroup.at("children"))
    {
        if (!camera.at("children").empty())
        {
            foundExpandedCameraWithObject = camera.value("expanded", false);
            break;
        }
    }
    EXPECT_TRUE(foundExpandedCameraWithObject) << "adding the first map object did not expand the camera row it landed in";

    StopEditor(editor);
    ASSERT_TRUE(editor.waitForFinished(5000)) << "editor did not exit after terminate()";
}

// Same regression as AddingFirstMapObjectAutoExpandsTreeToRevealIt, for collision lines -
// AddCollisionCommand selects the line it just created the same way AddNewObjectCommand does for
// map objects.
TEST(EditorAutomation, AddingFirstCollisionLineAutoExpandsTreeToRevealIt)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid()) << "failed to create a temp directory";
    const QString modDir = tempDir.filePath("MyMod");

    QProcess editor;
    AutomationClient client;
    QString socketName;
    ASSERT_TRUE(LaunchEditorAndConnect(editor, client, socketName));

    ASSERT_TRUE(CreateModWithLevelAndOpenPath(client, modDir));

    const auto treeRespBefore = client.Call({{"cmd", "get_tree_items"}, {"target", "modTreeWidget"}});
    ASSERT_TRUE(treeRespBefore.value("ok", false));
    ASSERT_FALSE(treeRespBefore.at("result").at("items")[0].at("children")[0].at("children")[0].at("children")[1].value("expanded", false))
        << "test setup: Collisions group should start collapsed";

    ASSERT_TRUE(AddCollisionLine(client));

    const auto treeResp = client.Call({{"cmd", "get_tree_items"}, {"target", "modTreeWidget"}});
    ASSERT_TRUE(treeResp.value("ok", false));
    const auto& collisionsGroup = treeResp.at("result").at("items")[0].at("children")[0].at("children")[0].at("children")[1];
    EXPECT_TRUE(collisionsGroup.value("expanded", false)) << "adding the first collision line did not expand the Collisions group";

    StopEditor(editor);
    ASSERT_TRUE(editor.waitForFinished(5000)) << "editor did not exit after terminate()";
}

// Double-clicking a camera's row should center the scene view on it, the same way double-
// clicking a path row opens it - verified by converting the camera's known center (grid index *
// cell size + half a cell) back to view coordinates via scene_to_view and checking it lands at
// the viewport's own center pixel, which is exactly what QGraphicsView::centerOn guarantees.
TEST(EditorAutomation, DoubleClickCameraRowCentersView)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid()) << "failed to create a temp directory";
    const QString modDir = tempDir.filePath("MyMod");

    QProcess editor;
    AutomationClient client;
    QString socketName;
    ASSERT_TRUE(LaunchEditorAndConnect(editor, client, socketName));

    ASSERT_TRUE(CreateModWithLevelAndOpenPath(client, modDir));

    // Pick a camera that isn't already at the view's default center (0,0 almost certainly is,
    // given a freshly opened path) so this only passes if the double-click actually moved
    // something - x=3,y=3 is the far corner of the default 4x4 grid.
    const int camX = 3;
    const int camY = 3;
    std::string cameraRowText;
    {
        const auto treeResp = client.Call({{"cmd", "get_tree_items"}, {"target", "modTreeWidget"}});
        ASSERT_TRUE(treeResp.value("ok", false));
        const auto& camerasGroup = treeResp.at("result").at("items")[0].at("children")[0].at("children")[0].at("children")[0];
        cameraRowText = QString("%1,%2 @ empty").arg(camX).arg(camY).toStdString();
        bool found = false;
        for (const auto& camera : camerasGroup.at("children"))
        {
            if (camera.value("text", std::string()) == cameraRowText)
            {
                found = true;
                break;
            }
        }
        ASSERT_TRUE(found) << "test setup: no row found for the camera at " << camX << "," << camY;
    }

    ASSERT_TRUE(client.Call({{"cmd", "click_tree_item"}, {"target", "modTreeWidget"}, {"column", 0}, {"row_text", cameraRowText}, {"double_click", true}}).value("ok", false));

    const auto camResp = client.Call({{"cmd", "get_scene_items"}});
    ASSERT_TRUE(camResp.value("ok", false));
    QPointF expectedCenter(-1.0, -1.0);
    for (const auto& item : camResp.at("result").at("items"))
    {
        if (item.value("kind", std::string()) == "camera" && item.value("gridX", -1) == camX && item.value("gridY", -1) == camY)
        {
            expectedCenter = QPointF(item.value("sceneX", 0.0) + item.value("width", 0.0) / 2.0,
                                      item.value("sceneY", 0.0) + item.value("height", 0.0) / 2.0);
            break;
        }
    }
    ASSERT_GE(expectedCenter.x(), 0.0) << "test setup: could not find the camera's own scene rect";
    const QPoint viewPoint = SceneToView(client, expectedCenter.x(), expectedCenter.y());

    const auto viewResp = client.Call({{"cmd", "get_state"}, {"target", "graphicsView"}});
    ASSERT_TRUE(viewResp.value("ok", false));
    const auto& geometry = viewResp.at("result").at("geometry");
    const QPoint viewportCenter(geometry.value("w", 0) / 2, geometry.value("h", 0) / 2);

    // Generous slack for QGraphicsView::centerOn's own scrollbar-step rounding (it snaps to
    // whatever discrete scrollbar positions are available, not an exact pixel) - this only needs
    // to catch centering not happening at all (which would be off by hundreds of pixels, not a
    // handful), not assert pixel-perfect placement.
    EXPECT_NEAR(viewPoint.x(), viewportCenter.x(), 20) << "double-clicking the camera row did not center the view horizontally";
    EXPECT_NEAR(viewPoint.y(), viewportCenter.y(), 20) << "double-clicking the camera row did not center the view vertically";

    StopEditor(editor);
    ASSERT_TRUE(editor.waitForFinished(5000)) << "editor did not exit after terminate()";
}

// Regression test: double-clicking a camera row only centered the view - if that camera
// belonged to a path open in a background tab (not the currently active one), the centering
// happened somewhere the user couldn't see, in a tab they weren't even looking at.
TEST(EditorAutomation, DoubleClickCameraRowMakesItsTabCurrent)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid()) << "failed to create a temp directory";
    const QString modDir = tempDir.filePath("MyMod");
    const QString mainImagePath = tempDir.filePath("sample_main.png");
    {
        QImage main(64, 24, QImage::Format_RGB32);
        main.fill(QColor(200, 40, 40));
        ASSERT_TRUE(main.save(mainImagePath)) << "failed to write the sample main camera image";
    }

    QProcess editor;
    AutomationClient client;
    QString socketName;
    ASSERT_TRUE(LaunchEditorAndConnect(editor, client, socketName));

    // Path 0, opened by this helper, becomes the active tab.
    ASSERT_TRUE(CreateModWithLevelAndOpenPath(client, modDir));

    // Path 1: another path in the same level, via the tree's own "New Path..." - same flow
    // CreateModWithLevelAndOpenPath already used once for Path 0. Creating/opening it makes it
    // the new active tab.
    {
        const int rightClickLevelId = client.SendCommand({{"cmd", "click_tree_item"}, {"target", "modTreeWidget"}, {"column", 0}, {"row_text", "MI"}, {"right_click", true}});
        const int clickId = client.SendCommand({{"cmd", "click"}, {"target", "actionNewPath"}});
        ASSERT_TRUE(client.Call({{"cmd", "send_key"}, {"target", "@active_modal"}, {"key", "Return"}}).value("ok", false));
        ASSERT_TRUE(client.WaitForResponse(clickId, 5000).value("ok", false));
        ASSERT_TRUE(DismissMenuIfStillOpen(client, "modTreeContextMenu"));
        ASSERT_TRUE(client.WaitForResponse(rightClickLevelId, 5000).value("ok", false));
    }

    // Give Path 1's camera at (0,0) a name/image, purely so its tree row ("0 @ 0,0") reads
    // differently from Path 0's still-untouched camera at the same grid position ("0,0 @
    // empty") - otherwise both paths would have an identically-labelled "0,0 @ empty" row and
    // click_tree_item's row_text lookup (which isn't scoped to one path's subtree) couldn't
    // tell them apart.
    {
        const auto resp = client.Call({{"cmd", "set_camera_image"}, {"x", 0}, {"y", 0}, {"layer", "main"}, {"image_path", mainImagePath.toStdString()}});
        ASSERT_TRUE(resp.value("ok", false));
        ASSERT_TRUE(resp.at("result").value("ok", false)) << "failed to create the camera";
    }

    auto findCameraAtOrigin = [&]() -> nlohmann::json
    {
        for (const auto& item : GetSceneItems(client))
        {
            if (item.value("kind", std::string()) == "camera" && item.value("gridX", -1) == 0 && item.value("gridY", -1) == 0)
            {
                return item;
            }
        }
        ADD_FAILURE() << "no camera at grid (0,0) in the current tab's scene";
        return nlohmann::json::object();
    };

    // Switch back to Path 0 so Path 1 is now the backgrounded tab this test is actually about.
    ASSERT_TRUE(client.Call({{"cmd", "click_tree_item"}, {"target", "modTreeWidget"}, {"column", 0}, {"row_text", "Path 0"}, {"double_click", true}}).value("ok", false));
    ASSERT_FALSE(findCameraAtOrigin().value("hasMainImage", true)) << "test setup: switched back to the wrong path (Path 0's camera should still be imageless)";

    ASSERT_TRUE(client.Call({{"cmd", "click_tree_item"}, {"target", "modTreeWidget"}, {"column", 0}, {"row_text", "0 @ 0,0"}, {"double_click", true}}).value("ok", false));

    EXPECT_TRUE(findCameraAtOrigin().value("hasMainImage", false)) << "double-clicking Path 1's camera row did not bring Path 1's tab to the front";

    StopEditor(editor);
    ASSERT_TRUE(editor.waitForFinished(5000)) << "editor did not exit after terminate()";
}

// Generalizes DoubleClickCameraRowMakesItsTabCurrent: selecting *any* row under a path -
// map-object/collision-line rows included, via a plain (single) click, not just double-clicking
// a camera row - should bring that path's tab to the front if it's open in the background.
TEST(EditorAutomation, SelectingMapObjectInTreeMakesItsTabCurrent)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid()) << "failed to create a temp directory";
    const QString modDir = tempDir.filePath("MyMod");

    QProcess editor;
    AutomationClient client;
    QString socketName;
    ASSERT_TRUE(LaunchEditorAndConnect(editor, client, socketName));

    // Path 0, opened by this helper, becomes the active tab.
    ASSERT_TRUE(CreateModWithLevelAndOpenPath(client, modDir));

    // Path 1: becomes the active tab once opened, and gets the one map object in this test - see
    // DoubleClickCameraRowMakesItsTabCurrent for why a second path is the only way to actually
    // exercise "bring a *background* tab to the front".
    {
        const int rightClickLevelId = client.SendCommand({{"cmd", "click_tree_item"}, {"target", "modTreeWidget"}, {"column", 0}, {"row_text", "MI"}, {"right_click", true}});
        const int clickId = client.SendCommand({{"cmd", "click"}, {"target", "actionNewPath"}});
        ASSERT_TRUE(client.Call({{"cmd", "send_key"}, {"target", "@active_modal"}, {"key", "Return"}}).value("ok", false));
        ASSERT_TRUE(client.WaitForResponse(clickId, 5000).value("ok", false));
        ASSERT_TRUE(DismissMenuIfStillOpen(client, "modTreeContextMenu"));
        ASSERT_TRUE(client.WaitForResponse(rightClickLevelId, 5000).value("ok", false));
    }
    ASSERT_TRUE(AddMapObject(client));

    std::string objectRowText;
    {
        const auto treeResp = client.Call({{"cmd", "get_tree_items"}, {"target", "modTreeWidget"}});
        ASSERT_TRUE(treeResp.value("ok", false));
        // Path 1 is the second child under the level (Path 0 was created first).
        const auto& camerasGroup = treeResp.at("result").at("items")[0].at("children")[0].at("children")[1].at("children")[0];
        for (const auto& camera : camerasGroup.at("children"))
        {
            if (!camera.at("children").empty())
            {
                objectRowText = camera.at("children")[0].value("text", std::string());
                break;
            }
        }
    }
    ASSERT_FALSE(objectRowText.empty()) << "test setup: no map object row found under Path 1";

    // Switch back to Path 0 so Path 1 is the backgrounded tab this test is actually about.
    ASSERT_TRUE(client.Call({{"cmd", "click_tree_item"}, {"target", "modTreeWidget"}, {"column", 0}, {"row_text", "Path 0"}, {"double_click", true}}).value("ok", false));
    ASSERT_EQ(CountKind(GetSceneItems(client), "map_object"), 0) << "test setup: switched back to the wrong path (Path 0 should have no map objects)";

    ASSERT_TRUE(client.Call({{"cmd", "click_tree_item"}, {"target", "modTreeWidget"}, {"column", 0}, {"row_text", objectRowText}}).value("ok", false));

    EXPECT_EQ(CountKind(GetSceneItems(client), "map_object"), 1) << "selecting Path 1's map object row in the tree did not bring Path 1's tab to the front";

    StopEditor(editor);
    ASSERT_TRUE(editor.waitForFinished(5000)) << "editor did not exit after terminate()";
}

// Regression test: selecting a row in the tree updated the scene's own selection, but the view
// itself never moved - a map object scrolled off screen (e.g. dragged to a far corner, then
// deselected) stayed off screen after selecting its row in the tree, selected but invisible.
TEST(EditorAutomation, SelectingMapObjectInTreeCentersView)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid()) << "failed to create a temp directory";
    const QString modDir = tempDir.filePath("MyMod");

    QProcess editor;
    AutomationClient client;
    QString socketName;
    ASSERT_TRUE(LaunchEditorAndConnect(editor, client, socketName));

    ASSERT_TRUE(CreateModWithLevelAndOpenPath(client, modDir));
    ASSERT_TRUE(AddMapObject(client));

    // Drag the object away from the view's default center (AddObjectDialog places a new object
    // dead center of the view) so centering on it afterward actually has to move something.
    {
        const nlohmann::json rect = FindKind(GetSceneItems(client), "map_object");
        const double midX = rect.at("xpos").get<double>() + rect.at("width").get<double>() / 2;
        const double midY = rect.at("ypos").get<double>() + rect.at("height").get<double>() / 2;
        DragView(client, SceneToView(client, midX, midY), SceneToView(client, midX + 400, midY + 300));
    }

    // Deselect (click well away from the object's new position) so the tree click below is what
    // actually does the selecting/centering, not just leaving the drag's own selection alone.
    const QPoint emptySpot = SceneToView(client, 5, 5);
    ASSERT_TRUE(SendMouseEvent(client, "down", emptySpot));
    ASSERT_TRUE(SendMouseEvent(client, "up", emptySpot));
    ASSERT_FALSE(FindKind(GetSceneItems(client), "map_object").value("selected", false)) << "test setup: click on empty space should deselect";

    const nlohmann::json objAfterDrag = FindKind(GetSceneItems(client), "map_object");
    const QPointF expectedCenter(objAfterDrag.at("xpos").get<double>() + objAfterDrag.at("width").get<double>() / 2,
                                  objAfterDrag.at("ypos").get<double>() + objAfterDrag.at("height").get<double>() / 2);

    std::string rowText;
    {
        const auto treeResp = client.Call({{"cmd", "get_tree_items"}, {"target", "modTreeWidget"}});
        ASSERT_TRUE(treeResp.value("ok", false));
        const auto& camerasGroup = treeResp.at("result").at("items")[0].at("children")[0].at("children")[0].at("children")[0];
        for (const auto& camera : camerasGroup.at("children"))
        {
            if (!camera.at("children").empty())
            {
                rowText = camera.at("children")[0].value("text", std::string());
                break;
            }
        }
    }
    ASSERT_FALSE(rowText.empty()) << "test setup: no map object row found in the tree";

    ASSERT_TRUE(client.Call({{"cmd", "click_tree_item"}, {"target", "modTreeWidget"}, {"column", 0}, {"row_text", rowText}}).value("ok", false));

    const QPoint viewPoint = SceneToView(client, expectedCenter.x(), expectedCenter.y());
    const auto viewResp = client.Call({{"cmd", "get_state"}, {"target", "graphicsView"}});
    ASSERT_TRUE(viewResp.value("ok", false));
    const auto& geometry = viewResp.at("result").at("geometry");
    const QPoint viewportCenter(geometry.value("w", 0) / 2, geometry.value("h", 0) / 2);

    // Generous slack, same reasoning as DoubleClickCameraRowCentersView.
    EXPECT_NEAR(viewPoint.x(), viewportCenter.x(), 20) << "selecting the object's row in the tree did not center the view horizontally";
    EXPECT_NEAR(viewPoint.y(), viewportCenter.y(), 20) << "selecting the object's row in the tree did not center the view vertically";

    StopEditor(editor);
    ASSERT_TRUE(editor.waitForFinished(5000)) << "editor did not exit after terminate()";
}

// Regression test: EditorTab's own QUndoStack member is destroyed implicitly right after
// ~EditorTab()'s body runs, and can still emit signals of its own accord while doing so
// (clearing its commands changes its index) - well before QObject's own destructor gets a
// chance to sever connections on its way out. ModTreeWidget listens to a still-open tab's
// indexChanged for as long as the tab is tracked as open, and reacts to it by touching that
// tab's model/scene - both already gone by the time this fires if the tab is mid-teardown,
// crashing (this is what the app's own shutdown path exercises: exiting with an unsaved mod/path
// still open, rather than closing each tab first through the usual "unsaved changes" prompt).
TEST(EditorAutomation, ExitWhileModPathOpenWithUndoHistoryDoesNotCrash)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid()) << "failed to create a temp directory";
    const QString modDir = tempDir.filePath("MyMod");

    QProcess editor;
    AutomationClient client;
    QString socketName;
    ASSERT_TRUE(LaunchEditorAndConnect(editor, client, socketName));

    ASSERT_TRUE(CreateModWithLevelAndOpenPath(client, modDir));
    ASSERT_TRUE(AddMapObject(client));
    ASSERT_TRUE(AddCollisionLine(client));

    // Real, unsaved changes are pending (unlike ExitActionClosesWindow's blank-state exit), so
    // this hits the same unsaved-changes prompt NewPathAddCollisionAddObject's own close does -
    // fired via SendCommand and only collected once the prompt's been dismissed.
    const int exitClickId = client.SendCommand({{"cmd", "click"}, {"target", "action_exit_application"}});
    ASSERT_TRUE(client.Call({{"cmd", "click"}, {"target", "@active_modal_button:Discard"}}).value("ok", false))
        << "no unsaved-changes prompt to dismiss";
    EXPECT_TRUE(client.WaitForResponse(exitClickId, 5000).value("ok", false));

    ASSERT_TRUE(editor.waitForFinished(10000)) << "editor did not exit after clicking Exit";
    EXPECT_EQ(editor.exitStatus(), QProcess::NormalExit);
    EXPECT_EQ(editor.exitCode(), 0);
}

// The mod tree dock has nothing useful to show (and no mod-scoped actions to offer) until a mod
// is actually open - it should start hidden and only appear once one is.
TEST(EditorAutomation, ModTreeDockHiddenUntilModOpen)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid()) << "failed to create a temp directory";
    const QString modDir = tempDir.filePath("MyMod");

    QProcess editor;
    AutomationClient client;
    QString socketName;
    ASSERT_TRUE(LaunchEditorAndConnect(editor, client, socketName));

    {
        const auto resp = client.Call({{"cmd", "get_state"}, {"target", "modTreeDockWidget"}});
        ASSERT_TRUE(resp.value("ok", false));
        EXPECT_FALSE(resp.at("result").value("visible", true)) << "mod tree dock should start hidden with no mod open";
    }

    ASSERT_TRUE(CreateNewMod(client, modDir, "My Mod", "Tester"));

    {
        const auto resp = client.Call({{"cmd", "get_state"}, {"target", "modTreeDockWidget"}});
        ASSERT_TRUE(resp.value("ok", false));
        EXPECT_TRUE(resp.at("result").value("visible", false)) << "mod tree dock should become visible once a mod is open";
    }

    StopEditor(editor);
    ASSERT_TRUE(editor.waitForFinished(5000)) << "editor did not exit after terminate()";
}

// The editor should remember the last mod that was open (QSettings "last_open_mod_dir", same
// Editor.ini-backed idiom as "last_open_dir"/"theme"/"windowState") and auto-reopen it on the
// next launch - simulated here via two separate LaunchEditorAndConnect processes sharing the
// same working directory (and therefore the same Editor.ini, since QSettings resolves that
// relative path from the process's cwd).
TEST(EditorAutomation, RestartReopensLastMod)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid()) << "failed to create a temp directory";
    const QString modDir = tempDir.filePath("MyMod");

    {
        QProcess editor;
        AutomationClient client;
        QString socketName;
        ASSERT_TRUE(LaunchEditorAndConnect(editor, client, socketName));
        ASSERT_TRUE(CreateNewMod(client, modDir, "My Mod", "Tester"));
        StopEditor(editor);
        ASSERT_TRUE(editor.waitForFinished(5000)) << "first editor instance did not exit after terminate()";
    }

    {
        QProcess editor;
        AutomationClient client;
        QString socketName;
        ASSERT_TRUE(LaunchEditorAndConnect(editor, client, socketName));

        const auto treeResp = client.Call({{"cmd", "get_tree_items"}, {"target", "modTreeWidget"}});
        ASSERT_TRUE(treeResp.value("ok", false));
        const auto& items = treeResp.at("result").at("items");
        ASSERT_EQ(items.size(), 1u) << "the previously open mod was not automatically reopened";
        EXPECT_EQ(items[0].value("text", std::string()), "My Mod");

        StopEditor(editor);
        ASSERT_TRUE(editor.waitForFinished(5000)) << "second editor instance did not exit after terminate()";
    }
}
