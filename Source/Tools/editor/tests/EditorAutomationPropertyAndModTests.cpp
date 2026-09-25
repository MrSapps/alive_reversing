// Editor automation tests: the property panel, creating mods and the mod tree.
// Helpers and main() are in EditorAutomationTestHelpers.cpp.

#include "EditorAutomationTestHelpers.hpp"

using namespace AutomationTest;

TEST(EditorAutomation, PropertyPanelEditIsClampedToMapBoundsWithUndoAndSaveReload)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid()) << "failed to create a temp directory";
    const QString savePath = tempDir.filePath("relive_editor_test_level.json");

    QProcess editor;
    AutomationClient client;
    QString socketName;
    ASSERT_TRUE(LaunchEditorAndConnect(editor, client, socketName));

    ASSERT_TRUE(CreateNewPath(client));
    ASSERT_TRUE(AddMapObject(client));

    const nlohmann::json original = FindKind(GetSceneItems(client), "map_object");

    // Select it (rubber-band select) so the properties panel populates for it.
    {
        const double x = original.at("xpos").get<double>();
        const double y = original.at("ypos").get<double>();
        const double w = original.at("width").get<double>();
        const double h = original.at("height").get<double>();
        DragView(client, SceneToView(client, x - 20, y - 20), SceneToView(client, x + w + 20, y + h + 20));
    }
    ASSERT_TRUE(FindKind(GetSceneItems(client), "map_object").value("selected", false)) << "rubber-band select did not select the object";

    // Clicks into a property row (spawning its editor widget if not already open) and types a
    // new value into it - mirrors "click column 1 of this row, then type a value and commit".
    auto setProperty = [&](const std::string& fieldName, qint64 value) -> bool
    {
        if (!client.Call({{"cmd", "click_tree_item"}, {"target", "treeWidget"}, {"row_text", fieldName}}).value("ok", false))
        {
            ADD_FAILURE() << "failed to click the '" << fieldName << "' property row";
            return false;
        }
        return client.Call({{"cmd", "set_value"}, {"target", "propertyEditor_" + fieldName}, {"value", value}}).value("ok", false);
    };

    // A wildly oversized width must clamp to the map's own bounds (AO's 1x1 default map is
    // 1024x480 - Model::CameraGridWidth/Height), the same invariant MapObjectDragResizeBoundsAndSnap
    // already checks for the drag-resize path - not just accept it raw.
    ASSERT_TRUE(setProperty("width", 999999));
    {
        const nlohmann::json afterWidth = FindKind(GetSceneItems(client), "map_object");
        EXPECT_LE(afterWidth.at("xpos").get<double>() + afterWidth.at("width").get<double>(), 1024)
            << "properties panel let width push the object past the map's right edge";
    }

    // Undo restores the exact pre-edit width...
    ASSERT_TRUE(client.Call({{"cmd", "click"}, {"target", "action_undo"}}).value("ok", false));
    {
        const nlohmann::json afterUndo = FindKind(GetSceneItems(client), "map_object");
        EXPECT_DOUBLE_EQ(afterUndo.value("width", -1.0), original.at("width").get<double>()) << "undo did not restore the pre-edit width";
    }
    // ...and redo re-applies the clamp, not the raw typed value.
    ASSERT_TRUE(client.Call({{"cmd", "click"}, {"target", "action_redo"}}).value("ok", false));
    {
        const nlohmann::json afterRedo = FindKind(GetSceneItems(client), "map_object");
        EXPECT_LE(afterRedo.at("xpos").get<double>() + afterRedo.at("width").get<double>(), 1024) << "redo did not reapply the clamp";
    }

    // Same oversized check on the other axis.
    ASSERT_TRUE(setProperty("height", 999999));
    {
        const nlohmann::json afterHeight = FindKind(GetSceneItems(client), "map_object");
        EXPECT_LE(afterHeight.at("ypos").get<double>() + afterHeight.at("height").get<double>(), 480)
            << "properties panel let height push the object past the map's bottom edge";
    }

    // A negative/undersized value must still respect the minimum rect size the resize handles
    // enforce (ResizeableRectItem::kMinRectSize == 10), not just "not below zero".
    ASSERT_TRUE(setProperty("height", -500));
    EXPECT_GE(FindKind(GetSceneItems(client), "map_object").at("height").get<double>(), 10)
        << "a negative height was not clamped to the minimum rect size";

    // xpos (a position, not a size) must reposition the object back into bounds the same way.
    ASSERT_TRUE(setProperty("xpos", 999999));
    {
        const nlohmann::json afterXpos = FindKind(GetSceneItems(client), "map_object");
        EXPECT_LE(afterXpos.at("xpos").get<double>() + afterXpos.at("width").get<double>(), 1024)
            << "properties panel let xpos push the object past the map's right edge";
    }

    // Save while every field above is still (correctly) clamped, then reopen: the corrected
    // values - not the raw out-of-bounds ones that were typed - must be what's on disk, proving
    // the fix writes the correction back into the model rather than only patching the on-screen
    // rect.
    {
        const auto resp = client.Call({{"cmd", "save_path_as"}, {"path", savePath.toStdString()}});
        ASSERT_TRUE(resp.value("ok", false));
        ASSERT_TRUE(resp.at("result").value("saved", false));
    }
    {
        const auto resp = client.Call({{"cmd", "open_path"}, {"path", savePath.toStdString()}});
        ASSERT_TRUE(resp.value("ok", false));
        ASSERT_TRUE(resp.at("result").value("opened", false));
    }
    {
        const nlohmann::json reopened = FindKind(GetSceneItems(client), "map_object");
        EXPECT_LE(reopened.at("xpos").get<double>() + reopened.at("width").get<double>(), 1024) << "oversized width/xpos survived a save/reopen round-trip";
        EXPECT_LE(reopened.at("ypos").get<double>() + reopened.at("height").get<double>(), 480) << "oversized height survived a save/reopen round-trip";
        EXPECT_GE(reopened.at("width").get<double>(), 10);
        EXPECT_GE(reopened.at("height").get<double>(), 10);
    }

    StopEditor(editor);
    ASSERT_TRUE(editor.waitForFinished(5000)) << "editor did not exit after terminate()";
}

// Regression test for a follow-up to the properties-panel clamp bug above: the spin box's own
// up/down arrows (QAbstractSpinBox::stepDown(), which the actual on-screen arrow buttons call)
// can't be typed past kMinRectSize like a raw value can, but repeatedly clicking down past it
// used to leave the *displayed* number drifting further and further from reality (e.g. "-176")
// while the real, correctly-clamped width sat at 10 underneath - because
// ChangeBasicTypePropertyCommand refreshed the property row's text/spin box *before*
// SyncInternalObject had a chance to correct the value it had just written. Confirmed by
// reverting just the display-refresh-ordering fix and re-running this: the object's own width
// still floored at 10 (proving the earlier clamp fix holds), but the spin box's line edit showed
// a wrong, ever-decreasing negative number instead.
TEST(EditorAutomation, PropertyPanelSpinBoxArrowsStayInSyncAtMinRectSize)
{
    QProcess editor;
    AutomationClient client;
    QString socketName;
    ASSERT_TRUE(LaunchEditorAndConnect(editor, client, socketName));

    ASSERT_TRUE(CreateNewPath(client));
    ASSERT_TRUE(AddMapObject(client));

    const nlohmann::json before = FindKind(GetSceneItems(client), "map_object");
    {
        const double x = before.at("xpos").get<double>();
        const double y = before.at("ypos").get<double>();
        const double w = before.at("width").get<double>();
        const double h = before.at("height").get<double>();
        DragView(client, SceneToView(client, x - 20, y - 20), SceneToView(client, x + w + 20, y + h + 20));
    }
    ASSERT_TRUE(FindKind(GetSceneItems(client), "map_object").value("selected", false)) << "rubber-band select did not select the object";

    const size_t undoCountBeforeEdits = GetUndoWidgetTextList(client).size();

    ASSERT_TRUE(client.Call({{"cmd", "click_tree_item"}, {"target", "treeWidget"}, {"row_text", "width"}}).value("ok", false));

    // Click the down arrow far more times than needed to cross the minimum - if the display
    // were drifting unboundedly (the bug), this is what would produce a wildly negative number.
    ASSERT_TRUE(client.Call({{"cmd", "spin_step"}, {"target", "propertyEditor_width"}, {"direction", "down"}, {"count", 200}}).value("ok", false));

    {
        const nlohmann::json afterDown = FindKind(GetSceneItems(client), "map_object");
        EXPECT_DOUBLE_EQ(afterDown.at("width").get<double>(), 10) << "width was not floored at the minimum rect size";
        EXPECT_EQ(SpinBoxDisplayedText(client, "propertyEditor_width"), "10")
            << "spin box display drifted away from the actual (correctly clamped) width";
    }
    // The undo entry must describe what actually happened (ending at the floor, 10) rather than
    // the raw, never-actually-applied value the spin box last requested (a wildly negative
    // number after 200 down-clicks) - ChangeBasicTypePropertyCommand::redo() now re-reads the
    // post-clamp value before building its "Change property X from A to B" text.
    const std::vector<std::string> undoAfterInitialEdit = GetUndoWidgetTextList(client);
    EXPECT_TRUE(UndoContains(undoAfterInitialEdit, "Change property width from 24 to 10"))
        << "undo entry describes the raw requested value instead of the actual clamped result";
    // All 200 clicks - the genuine 24->10 transition plus 186 further no-ops once already at the
    // floor - must collapse into exactly that one entry: the merge across edits to the same
    // field within a second already coalesced the *effective* ones, and PushIfEffective now
    // skips pushing anything at all for a click that the clamp fully absorbs (old value ==
    // corrected new value), so no run of no-op clicks should ever grow the stack.
    EXPECT_EQ(undoAfterInitialEdit.size(), undoCountBeforeEdits + 1)
        << "no-op down-clicks at the floor added their own undo entries instead of being discarded";

    // Clicking down further while already pinned at the floor - each one individually a no-op -
    // must not add any more undo entries at all (this is what "10 to 10" showing up in the undo
    // list, or the stack growing on every held-down click, would look like).
    ASSERT_TRUE(client.Call({{"cmd", "spin_step"}, {"target", "propertyEditor_width"}, {"direction", "down"}, {"count", 50}}).value("ok", false));
    {
        const nlohmann::json afterMoreDown = FindKind(GetSceneItems(client), "map_object");
        EXPECT_DOUBLE_EQ(afterMoreDown.at("width").get<double>(), 10) << "width moved away from the floor on a further no-op down-click";
        EXPECT_EQ(SpinBoxDisplayedText(client, "propertyEditor_width"), "10");
    }
    EXPECT_EQ(GetUndoWidgetTextList(client), undoAfterInitialEdit)
        << "further no-op down-clicks at the floor changed the undo stack";

    // Recovery check: since the display is back in sync at the floor, clicking up must
    // immediately start growing the object again - not silently do nothing while the display
    // slowly climbs back out of whatever negative range it had drifted into.
    ASSERT_TRUE(client.Call({{"cmd", "spin_step"}, {"target", "propertyEditor_width"}, {"direction", "up"}, {"count", 5}}).value("ok", false));
    {
        const nlohmann::json afterUp = FindKind(GetSceneItems(client), "map_object");
        EXPECT_DOUBLE_EQ(afterUp.at("width").get<double>(), 15) << "up arrow did not immediately resume growing the object from the floor";
        EXPECT_EQ(SpinBoxDisplayedText(client, "propertyEditor_width"), "15");
    }

    StopEditor(editor);
    ASSERT_TRUE(editor.waitForFinished(5000)) << "editor did not exit after terminate()";
}

// Regression/feature test for the Mod project system: File > Mod > New Mod... (NewModDialog)
// should write modinfo.json with the fields entered, and ModTreeWidget (the new left-docked
// tree) should show just the mod's root item - no levels yet, since nothing's been created
// inside it.
TEST(EditorAutomation, CreateModWritesModInfoAndShowsEmptyTree)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid()) << "failed to create a temp directory";
    const QString modDir = tempDir.filePath("MyMod");

    QProcess editor;
    AutomationClient client;
    QString socketName;
    ASSERT_TRUE(LaunchEditorAndConnect(editor, client, socketName));

    ASSERT_TRUE(CreateNewMod(client, modDir, "My Mod", "Tester"));

    ASSERT_TRUE(QFile::exists(modDir + "/modinfo.json")) << "New Mod did not write modinfo.json";
    {
        QFile f(modDir + "/modinfo.json");
        ASSERT_TRUE(f.open(QIODevice::ReadOnly));
        const nlohmann::json modInfo = nlohmann::json::parse(f.readAll().toStdString());
        EXPECT_EQ(modInfo.value("name", std::string()), "My Mod");
        EXPECT_EQ(modInfo.value("author", std::string()), "Tester");
        EXPECT_EQ(modInfo.value("target_game", std::string()), "AO") << "cmbGame's default (index 0) should be AO";
    }

    const auto treeResp = client.Call({{"cmd", "get_tree_items"}, {"target", "modTreeWidget"}});
    ASSERT_TRUE(treeResp.value("ok", false));
    const auto& items = treeResp.at("result").at("items");
    ASSERT_EQ(items.size(), 1u) << "expected just the mod root item";
    EXPECT_EQ(items[0].value("text", std::string()), "My Mod");
    EXPECT_EQ(items[0].at("children").size(), 0u) << "a brand new mod should have no levels yet";

    StopEditor(editor);
    ASSERT_TRUE(editor.waitForFinished(5000)) << "editor did not exit after terminate()";
}

// "Base this mod on an existing project": creating a mod with chkBaseOn checked and txtBaseOn
// pointing at another mod's root should copy that mod's whole levels/ tree (paths, cameras)
// into the new mod, giving the user a starting point instead of an empty project.
TEST(EditorAutomation, NewModBasedOnExistingModCopiesItsLevels)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid()) << "failed to create a temp directory";
    const QString sourceModDir = tempDir.filePath("SourceMod");
    const QString newModDir = tempDir.filePath("NewMod");

    QProcess editor;
    AutomationClient client;
    QString socketName;
    ASSERT_TRUE(LaunchEditorAndConnect(editor, client, socketName));

    // Build a source mod with one level/path/map object to copy from.
    ASSERT_TRUE(CreateModWithLevelAndOpenPath(client, sourceModDir));
    ASSERT_TRUE(AddMapObject(client));
    {
        const auto resp = client.Call({{"cmd", "save_path_as"}, {"path", (sourceModDir + "/levels/MI/0/path.json").toStdString()}});
        ASSERT_TRUE(resp.value("ok", false));
        ASSERT_TRUE(resp.at("result").value("saved", false)) << "failed to save the source mod's path";
    }
    ASSERT_TRUE(QFile::exists(sourceModDir + "/levels/MI/0/path.json")) << "test setup: source mod's path was not written";

    // Create a new mod based on it.
    const int clickId = client.SendCommand({{"cmd", "click"}, {"target", "actionNewMod"}});
    ASSERT_TRUE(client.Call({{"cmd", "set_value"}, {"target", "txtName"}, {"value", "New Mod"}}).value("ok", false));
    ASSERT_TRUE(client.Call({{"cmd", "set_value"}, {"target", "txtAuthor"}, {"value", "Tester"}}).value("ok", false));
    ASSERT_TRUE(client.Call({{"cmd", "set_value"}, {"target", "txtDirectory"}, {"value", newModDir.toStdString()}}).value("ok", false));
    ASSERT_TRUE(client.Call({{"cmd", "set_value"}, {"target", "chkBaseOn"}, {"value", true}}).value("ok", false))
        << "failed to check chkBaseOn";
    ASSERT_TRUE(client.Call({{"cmd", "set_value"}, {"target", "txtBaseOn"}, {"value", sourceModDir.toStdString()}}).value("ok", false))
        << "failed to set txtBaseOn";
    ASSERT_TRUE(client.Call({{"cmd", "send_key"}, {"target", "@active_modal"}, {"key", "Return"}}).value("ok", false))
        << "no active modal for NewModDialog";
    ASSERT_TRUE(client.WaitForResponse(clickId, 5000).value("ok", false));

    // The new mod's levels/ tree should match the source mod's, byte for byte.
    ASSERT_TRUE(QFile::exists(newModDir + "/levels/MI/level_info.json")) << "level_info.json was not copied";
    ASSERT_TRUE(QFile::exists(newModDir + "/levels/MI/0/path.json")) << "path.json was not copied";
    {
        QFile sourceFile(sourceModDir + "/levels/MI/0/path.json");
        QFile destFile(newModDir + "/levels/MI/0/path.json");
        ASSERT_TRUE(sourceFile.open(QIODevice::ReadOnly));
        ASSERT_TRUE(destFile.open(QIODevice::ReadOnly));
        EXPECT_EQ(sourceFile.readAll(), destFile.readAll()) << "copied path.json does not match the source mod's";
    }

    // The copy should also be reflected in the tree once the new mod is opened (it already is,
    // as the just-switched-to current mod).
    const auto treeResp = client.Call({{"cmd", "get_tree_items"}, {"target", "modTreeWidget"}});
    ASSERT_TRUE(treeResp.value("ok", false));
    const auto& items = treeResp.at("result").at("items");
    ASSERT_EQ(items.size(), 1u);
    EXPECT_EQ(items[0].value("text", std::string()), "New Mod");
    ASSERT_EQ(items[0].at("children").size(), 1u) << "expected the copied MI level";
    EXPECT_EQ(items[0].at("children")[0].value("text", std::string()), "MI");

    StopEditor(editor);
    ASSERT_TRUE(editor.waitForFinished(5000)) << "editor did not exit after terminate()";
}

// Right-click the mod root -> New Level..., right-click that level -> New Path... must create
// the on-disk layout EditorMod expects (levels/<level>/level_info.json + <id>/path.json), open
// the new path as a tab, and populate the tree down to that path's Cameras/Collisions groups
// immediately (RefreshPathSubtree runs as soon as NotifyTabOpened links the freshly-opened tab
// to its tree node - see ModTreeWidget::PromptNewPath).
TEST(EditorAutomation, NewLevelAndNewPathViaTreeContextMenuCreatesFilesAndOpensPath)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid()) << "failed to create a temp directory";
    const QString modDir = tempDir.filePath("MyMod");

    QProcess editor;
    AutomationClient client;
    QString socketName;
    ASSERT_TRUE(LaunchEditorAndConnect(editor, client, socketName));

    ASSERT_TRUE(CreateNewMod(client, modDir, "My Mod", "Tester"));

    // A right-click opens the tree's context menu via QMenu::exec() - a nested, blocking event
    // loop (same as EditorGraphicsView's own context menu - see DeleteCameraViaContextMenu's
    // comment above), so it's fired via SendCommand and its response only collected via
    // WaitForResponse once the action that dismisses the menu has actually happened.
    const int rightClickModRootId = client.SendCommand({{"cmd", "click_tree_item"}, {"target", "modTreeWidget"}, {"column", 0}, {"row_text", "My Mod"}, {"right_click", true}});
    {
        const int clickId = client.SendCommand({{"cmd", "click"}, {"target", "actionNewLevel"}});
        ASSERT_TRUE(client.Call({{"cmd", "set_value"}, {"target", "@active_modal_input"}, {"value", "MI"}}).value("ok", false));
        ASSERT_TRUE(client.Call({{"cmd", "send_key"}, {"target", "@active_modal"}, {"key", "Return"}}).value("ok", false));
        ASSERT_TRUE(client.WaitForResponse(clickId, 5000).value("ok", false));
    }
    ASSERT_TRUE(DismissMenuIfStillOpen(client, "modTreeContextMenu"));
    ASSERT_TRUE(client.WaitForResponse(rightClickModRootId, 5000).value("ok", false));

    ASSERT_TRUE(QDir(modDir + "/levels/MI").exists()) << "New Level did not create the level's folder";
    {
        const auto treeResp = client.Call({{"cmd", "get_tree_items"}, {"target", "modTreeWidget"}});
        ASSERT_TRUE(treeResp.value("ok", false));
        const auto& items = treeResp.at("result").at("items");
        ASSERT_EQ(items[0].at("children").size(), 1u) << "expected exactly one level after New Level";
        EXPECT_EQ(items[0].at("children")[0].value("text", std::string()), "MI");
    }

    const int rightClickLevelId = client.SendCommand({{"cmd", "click_tree_item"}, {"target", "modTreeWidget"}, {"column", 0}, {"row_text", "MI"}, {"right_click", true}});
    {
        const int clickId = client.SendCommand({{"cmd", "click"}, {"target", "actionNewPath"}});
        // Path id defaults to the lowest unused one (0 for a brand new level) - accept as-is.
        ASSERT_TRUE(client.Call({{"cmd", "send_key"}, {"target", "@active_modal"}, {"key", "Return"}}).value("ok", false));
        ASSERT_TRUE(client.WaitForResponse(clickId, 5000).value("ok", false));
    }
    ASSERT_TRUE(DismissMenuIfStillOpen(client, "modTreeContextMenu"));
    ASSERT_TRUE(client.WaitForResponse(rightClickLevelId, 5000).value("ok", false));

    ASSERT_TRUE(QFile::exists(modDir + "/levels/MI/0/path.json")) << "New Path did not write path.json";
    ASSERT_TRUE(QFile::exists(modDir + "/levels/MI/level_info.json")) << "New Path did not update level_info.json";
    {
        QFile f(modDir + "/levels/MI/level_info.json");
        ASSERT_TRUE(f.open(QIODevice::ReadOnly));
        const nlohmann::json levelInfo = nlohmann::json::parse(f.readAll().toStdString());
        ASSERT_EQ(levelInfo.at("paths").size(), 1u);
        EXPECT_EQ(levelInfo.at("paths")[0].value("path_id", std::string()), "0");
    }

    // The new path should already be open as a tab (showing the default 4x4 camera grid - see
    // Model::CreateAsNewPath's header default).
    ASSERT_EQ(CountKind(GetSceneItems(client), "camera"), 16) << "new path did not open as a tab";

    const auto treeResp = client.Call({{"cmd", "get_tree_items"}, {"target", "modTreeWidget"}});
    ASSERT_TRUE(treeResp.value("ok", false));
    const auto& level = treeResp.at("result").at("items")[0].at("children")[0];
    ASSERT_EQ(level.at("children").size(), 1u);
    const auto& pathItem = level.at("children")[0];
    EXPECT_EQ(pathItem.value("text", std::string()), "Path 0");
    ASSERT_EQ(pathItem.at("children").size(), 2u) << "expected Cameras and Collisions group headers";
    EXPECT_EQ(pathItem.at("children")[0].value("text", std::string()), "Cameras");
    EXPECT_EQ(pathItem.at("children")[1].value("text", std::string()), "Collisions");

    StopEditor(editor);
    ASSERT_TRUE(editor.waitForFinished(5000)) << "editor did not exit after terminate()";
}

// Double-clicking a LevelTreeItem toggles its expansion; double-clicking a PathTreeItem opens
// it (or refocuses its tab if already open, via the same onOpenPath dedup-by-filename every
// other way of opening a path already relies on) rather than ever duplicating the tab.
TEST(EditorAutomation, ModTreeDoubleClickTogglesLevelAndOpensPathWithoutDuplicating)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid()) << "failed to create a temp directory";
    const QString modDir = tempDir.filePath("MyMod");

    QProcess editor;
    AutomationClient client;
    QString socketName;
    ASSERT_TRUE(LaunchEditorAndConnect(editor, client, socketName));

    ASSERT_TRUE(CreateModWithLevelAndOpenPath(client, modDir));

    auto levelExpanded = [&]() -> bool
    {
        const auto resp = client.Call({{"cmd", "get_tree_items"}, {"target", "modTreeWidget"}});
        return resp.at("result").at("items")[0].at("children")[0].value("expanded", false);
    };
    ASSERT_TRUE(levelExpanded()) << "test setup: a just-created level should start expanded";

    ASSERT_TRUE(client.Call({{"cmd", "click_tree_item"}, {"target", "modTreeWidget"}, {"column", 0}, {"row_text", "MI"}, {"double_click", true}}).value("ok", false));
    EXPECT_FALSE(levelExpanded()) << "double-clicking an expanded level did not collapse it";

    ASSERT_TRUE(client.Call({{"cmd", "click_tree_item"}, {"target", "modTreeWidget"}, {"column", 0}, {"row_text", "MI"}, {"double_click", true}}).value("ok", false));
    EXPECT_TRUE(levelExpanded()) << "double-clicking a collapsed level did not re-expand it";

    // Close the path's tab, then double-click it in the tree to reopen it. GetSceneItems needs
    // an active tab (get_scene_items errors with none open), so check the tab count via
    // tabWidget's own children instead of the scene here.
    ASSERT_TRUE(client.Call({{"cmd", "click"}, {"target", "action_close_path"}}).value("ok", false));
    {
        const auto tabsResp = client.Call({{"cmd", "get_state"}, {"target", "tabWidget"}});
        ASSERT_TRUE(tabsResp.value("ok", false));
        ASSERT_EQ(CountClassName(tabsResp.at("result"), "EditorTab"), 0) << "test setup: path should be closed";
    }

    ASSERT_TRUE(client.Call({{"cmd", "click_tree_item"}, {"target", "modTreeWidget"}, {"column", 0}, {"row_text", "Path 0"}, {"double_click", true}}).value("ok", false));
    ASSERT_EQ(CountKind(GetSceneItems(client), "camera"), 16) << "double-clicking the path did not reopen it";

    const auto tabsResp = client.Call({{"cmd", "get_state"}, {"target", "tabWidget"}});
    ASSERT_TRUE(tabsResp.value("ok", false));
    ASSERT_EQ(CountClassName(tabsResp.at("result"), "EditorTab"), 1) << "test setup: exactly one tab should be open";

    // Double-clicking the same path again must not open a second tab for the same file.
    ASSERT_TRUE(client.Call({{"cmd", "click_tree_item"}, {"target", "modTreeWidget"}, {"column", 0}, {"row_text", "Path 0"}, {"double_click", true}}).value("ok", false));
    const auto tabsRespAfter = client.Call({{"cmd", "get_state"}, {"target", "tabWidget"}});
    ASSERT_TRUE(tabsRespAfter.value("ok", false));
    EXPECT_EQ(CountClassName(tabsRespAfter.at("result"), "EditorTab"), 1) << "double-clicking an already-open path duplicated its tab";

    StopEditor(editor);
    ASSERT_TRUE(editor.waitForFinished(5000)) << "editor did not exit after terminate()";
}

// The tree's Cameras/Collisions listing for an open path must track every add/undo/redo, not
// just what it looked like when the path was first opened - ModTreeWidget connects to
// EditorTab::GetUndoStack()'s indexChanged for exactly this, once when a path node is first
// linked to its tab (see ModTreeWidget::NotifyTabOpened), rather than every individual command
// (DeleteItemsCommand, AddCollisionCommand, ...) needing to know about the tree directly.
TEST(EditorAutomation, ModTreeStaysInSyncWithAddAndUndoInOpenPath)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid()) << "failed to create a temp directory";
    const QString modDir = tempDir.filePath("MyMod");

    QProcess editor;
    AutomationClient client;
    QString socketName;
    ASSERT_TRUE(LaunchEditorAndConnect(editor, client, socketName));

    ASSERT_TRUE(CreateModWithLevelAndOpenPath(client, modDir));

    auto mapObjectNodeCount = [&]() -> int
    {
        const auto resp = client.Call({{"cmd", "get_tree_items"}, {"target", "modTreeWidget"}});
        const auto& camerasGroup = resp.at("result").at("items")[0].at("children")[0].at("children")[0].at("children")[0];
        int total = 0;
        for (const auto& camera : camerasGroup.at("children"))
        {
            total += static_cast<int>(camera.at("children").size());
        }
        return total;
    };
    ASSERT_EQ(mapObjectNodeCount(), 0) << "test setup: freshly created path should have no map objects";

    ASSERT_TRUE(AddMapObject(client));
    EXPECT_EQ(mapObjectNodeCount(), 1) << "tree did not pick up the newly added map object";

    ASSERT_TRUE(client.Call({{"cmd", "click"}, {"target", "action_undo"}}).value("ok", false));
    EXPECT_EQ(mapObjectNodeCount(), 0) << "tree did not remove the map object again after undo";

    StopEditor(editor);
    ASSERT_TRUE(editor.waitForFinished(5000)) << "editor did not exit after terminate()";
}

// Regression test: RefreshPathSubtree used to skip listing a camera in the mod tree entirely
// whenever it had neither a name (no image set yet) nor any map objects - so a still-imageless
// camera on a freshly created path had no row in the tree at all, and so no way to right-click
// it to open CameraManager and give it an image (unlike the scene view, where every grid cell -
// imageless or not - can always be right-clicked for "Edit camera"). The tree now lists every
// camera cell, labelling an unset one "x,y @ empty" (CameraManager's own list does the same),
// and right-clicking it offers the same "Edit camera" action, wired to open the same
// CameraManager dialog, as the scene view's own context menu.
TEST(EditorAutomation, EmptyCameraListedInTreeAndEditCameraOpensCameraManager)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid()) << "failed to create a temp directory";
    const QString modDir = tempDir.filePath("MyMod");

    QProcess editor;
    AutomationClient client;
    QString socketName;
    ASSERT_TRUE(LaunchEditorAndConnect(editor, client, socketName));

    ASSERT_TRUE(CreateModWithLevelAndOpenPath(client, modDir));

    // The fresh path's default 4x4 = 16 cameras have never been given an image, so they're all
    // still unnamed - yet every one of them should already have a row in the tree, not just the
    // ones that happen to hold a map object.
    {
        const auto treeResp = client.Call({{"cmd", "get_tree_items"}, {"target", "modTreeWidget"}});
        ASSERT_TRUE(treeResp.value("ok", false));
        const auto& camerasGroup = treeResp.at("result").at("items")[0].at("children")[0].at("children")[0].at("children")[0];
        EXPECT_EQ(camerasGroup.value("text", std::string()), "Cameras");
        ASSERT_EQ(camerasGroup.at("children").size(), 16u) << "not every still-imageless camera has a row in the tree";
        bool foundEmptyCamAtOrigin = false;
        for (const auto& camera : camerasGroup.at("children"))
        {
            if (camera.value("text", std::string()) == "0,0 @ empty")
            {
                foundEmptyCamAtOrigin = true;
                break;
            }
        }
        EXPECT_TRUE(foundEmptyCamAtOrigin) << "no row for the still-imageless camera at 0,0";
    }

    // Right-click that row and use its "Edit camera" action - same idiom as
    // CreateModWithLevelAndOpenPath's own right-clicks, and as DeleteCameraViaContextMenu's for
    // the scene view's equivalent menu: QMenu::exec() blocks in a nested event loop, so the
    // right-click itself is fired via SendCommand and only collected once the action that
    // dismisses the menu (opening CameraManager) has happened.
    const int rightClickCameraId = client.SendCommand({{"cmd", "click_tree_item"}, {"target", "modTreeWidget"}, {"column", 0}, {"row_text", "0,0 @ empty"}, {"right_click", true}});
    const int editCameraClickId = client.SendCommand({{"cmd", "click"}, {"target", "actionEditCamera"}});

    ASSERT_TRUE(client.Call({{"cmd", "get_state"}, {"target", "CameraManager"}}).value("ok", false))
        << "CameraManager dialog did not open from the tree's context menu";
    ASSERT_TRUE(client.Call({{"cmd", "close"}, {"target", "CameraManager"}}).value("ok", false))
        << "failed to close the CameraManager dialog";
    ASSERT_TRUE(client.WaitForResponse(editCameraClickId, 5000).value("ok", false))
        << "click on actionEditCamera did not report success";

    ASSERT_TRUE(DismissMenuIfStillOpen(client, "modTreeContextMenu"));
    ASSERT_TRUE(client.WaitForResponse(rightClickCameraId, 5000).value("ok", false));

    StopEditor(editor);
    ASSERT_TRUE(editor.waitForFinished(5000)) << "editor did not exit after terminate()";
}

// Regression test: RefreshPathSubtree rebuilds a CameraTreeItem from scratch after any
// undoable command touches the model, including setting a camera's main image
// (NewCameraCommand) - so the row genuinely was being recreated, but its label formula
// (CameraLabel) only ever depended on the camera's name/position, never whether it actually has
// an image, so the "refresh" was invisible for anything image-related. That matters beyond
// cosmetics: CameraGraphicsItem::Load silently leaves mCameraImage null if "<name>.png" is
// missing from disk when a path is (re)opened (e.g. the file got deleted/moved outside the
// editor) - a named camera with no image is a real, reachable state, and the tree previously had
// no way to show it. CameraLabel now appends "(no image)" whenever a named camera has none.
TEST(EditorAutomation, SetCameraImageUpdatesTreeRowLabel)
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

    ASSERT_TRUE(CreateModWithLevelAndOpenPath(client, modDir));

    // Model::CreateEmptyCameras always fills the grid in x-major, y-minor order starting at
    // (0,0), so the camera at (0,0) is always the Cameras group's first child regardless of its
    // current name/label.
    auto cameraRowText = [&]() -> std::string
    {
        const auto treeResp = client.Call({{"cmd", "get_tree_items"}, {"target", "modTreeWidget"}});
        const auto& camerasGroup = treeResp.at("result").at("items")[0].at("children")[0].at("children")[0].at("children")[0];
        return camerasGroup.at("children")[0].value("text", std::string());
    };
    ASSERT_EQ(cameraRowText(), "0,0 @ empty") << "test setup: camera at 0,0 should start with no name/image";

    {
        const auto resp = client.Call({{"cmd", "set_camera_image"}, {"x", 0}, {"y", 0}, {"layer", "main"}, {"image_path", mainImagePath.toStdString()}});
        ASSERT_TRUE(resp.value("ok", false));
        ASSERT_TRUE(resp.at("result").value("ok", false)) << "failed to create the camera";
    }
    EXPECT_EQ(cameraRowText(), "0 @ 0,0") << "tree row did not update after the camera was named/given an image";

    // Simulate the image going missing from disk (moved/deleted outside the editor) by removing
    // it before reopening the path - CameraGraphicsItem::Load then leaves mCameraImage null
    // while mName ("0", loaded straight from the JSON) stays set.
    const QString pngPath = modDir + "/levels/MI/0/0.png";
    ASSERT_TRUE(QFile::exists(pngPath)) << "test setup: camera image was not saved to disk";
    ASSERT_TRUE(QFile::remove(pngPath));

    // Save first so the tab has no unsaved changes left (set_camera_image only wrote the image
    // bytes to disk directly - the model's own path.json, which records the camera's name/id,
    // still needs this) - otherwise closing the tab below blocks on an unsaved-changes prompt.
    {
        const auto resp = client.Call({{"cmd", "save_path_as"}, {"path", (modDir + "/levels/MI/0/path.json").toStdString()}});
        ASSERT_TRUE(resp.value("ok", false));
        ASSERT_TRUE(resp.at("result").value("saved", false)) << "save_path_as reported failure";
    }

    // open_path dedups against an already-open tab by filename and just focuses it rather than
    // reloading - close the tab first so reopening it actually re-reads the (now image-less)
    // path from disk instead of leaving the in-memory camera (still holding the image) untouched.
    ASSERT_TRUE(client.Call({{"cmd", "click"}, {"target", "action_close_path"}}).value("ok", false));
    {
        const auto resp = client.Call({{"cmd", "open_path"}, {"path", (modDir + "/levels/MI/0/path.json").toStdString()}});
        ASSERT_TRUE(resp.value("ok", false));
        ASSERT_TRUE(resp.at("result").value("opened", false)) << "open_path reported failure";
    }
    EXPECT_EQ(cameraRowText(), "0 @ 0,0 (no image)") << "tree row does not reflect the camera's missing image";

    StopEditor(editor);
    ASSERT_TRUE(editor.waitForFinished(5000)) << "editor did not exit after terminate()";
}

// Regression test: the tree used to have no idea what was selected in the scene at all (no
// itemSelectionChanged handling either direction), and RefreshPathSubtree's full rebuild (which
// used to run on every single selection change too, not just structural edits) would have wiped
// any tree selection state anyway even if something had set it. Adding a map object selects it
// (AddNewObjectCommand::redo()) without going through SetSelectionCommand/the custom
// EditorGraphicsScene::SelectionChanged signal - it's picked up purely because
// RefreshPathSubtree now ends with SyncTreeSelectionFromScene, which reads the scene's actual
// selectedItems() rather than relying on that signal.
