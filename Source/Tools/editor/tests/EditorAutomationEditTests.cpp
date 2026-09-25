// Editor automation tests: cameras, paste, connecting/adding collision lines and deleting.
// Helpers and main() are in EditorAutomationTestHelpers.cpp.

#include "EditorAutomationTestHelpers.hpp"

using namespace AutomationTest;

TEST(EditorAutomation, CameraAddSaveAndFg1LayerRoundTrip)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid()) << "failed to create a temp directory";
    const QString savePath = tempDir.filePath("relive_editor_test_level.json");

    const QString mainImagePath = tempDir.filePath("sample_main.png");
    const QString fgImagePath = tempDir.filePath("sample_fg.png");
    {
        QImage main(64, 24, QImage::Format_RGB32);
        main.fill(QColor(200, 40, 40));
        ASSERT_TRUE(main.save(mainImagePath)) << "failed to write the sample main camera image";

        QImage fg(64, 24, QImage::Format_ARGB32);
        fg.fill(QColor(40, 200, 40, 128));
        ASSERT_TRUE(fg.save(fgImagePath)) << "failed to write the sample FG1 layer image";
    }

    QProcess editor;
    AutomationClient client;
    QString socketName;
    ASSERT_TRUE(LaunchEditorAndConnect(editor, client, socketName));

    ASSERT_TRUE(CreateNewPath(client));

    // Establish a real on-disk directory before creating the camera: camera images/layers save
    // next to EditorTab::mPathDirectory, which is only set from the tab's own file path.
    {
        const auto resp = client.Call({{"cmd", "save_path_as"}, {"path", savePath.toStdString()}});
        ASSERT_TRUE(resp.value("ok", false));
        ASSERT_TRUE(resp.at("result").value("saved", false)) << "save_path_as reported failure";
    }

    // Create a new camera at grid (0,0) - the only cell in a fresh 1x1 path - with the main
    // image. NewCameraCommand names the very first camera created in a fresh path "0".
    {
        const auto resp = client.Call({{"cmd", "set_camera_image"}, {"x", 0}, {"y", 0}, {"layer", "main"}, {"image_path", mainImagePath.toStdString()}});
        ASSERT_TRUE(resp.value("ok", false));
        ASSERT_TRUE(resp.at("result").value("ok", false)) << "failed to create the camera";
    }

    const QString pathDir = QFileInfo(savePath).path();
    ASSERT_TRUE(QFile::exists(pathDir + "/0.png")) << "camera main image was not saved to disk";

    // Add an FG1 foreground layer to the same camera.
    {
        const auto resp = client.Call({{"cmd", "set_camera_image"}, {"x", 0}, {"y", 0}, {"layer", "foreground"}, {"image_path", fgImagePath.toStdString()}});
        ASSERT_TRUE(resp.value("ok", false));
        ASSERT_TRUE(resp.at("result").value("ok", false)) << "failed to set the foreground layer";
    }

    ASSERT_TRUE(QFile::exists(pathDir + "/0fg.png")) << "camera foreground layer image was not saved to disk";
    ASSERT_TRUE(QFile::exists(pathDir + "/0.json")) << "FG1 layers sidecar json was not saved to disk";

    // The sidecar must actually list the foreground layer - this is what the engine reads to
    // know an FG1 layer exists at all.
    {
        QFile f(pathDir + "/0.json");
        ASSERT_TRUE(f.open(QFile::ReadOnly | QFile::Text));
        const auto j = nlohmann::json::parse(f.readAll().toStdString());
        bool foundFg = false;
        for (const auto& layer : j.at("layers"))
        {
            if (layer.get<std::string>().find("fg") != std::string::npos)
            {
                foundFg = true;
            }
        }
        EXPECT_TRUE(foundFg) << "layers sidecar does not list the foreground layer: " << j.dump();
    }

    {
        const nlohmann::json cam = FindKind(GetSceneItems(client), "camera");
        EXPECT_TRUE(cam.value("hasMainImage", false));
        EXPECT_TRUE(cam.value("hasForegroundLayer", false));
    }

    // Save the path itself and reopen it fresh - the camera and its FG1 layer must still be
    // there, proving this round-trips through disk rather than only surviving in memory.
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
        const nlohmann::json cam = FindKind(GetSceneItems(client), "camera");
        EXPECT_EQ(cam.value("camName", std::string()), "0") << "camera did not round-trip after reopening the path";
        EXPECT_TRUE(cam.value("hasMainImage", false)) << "camera main image did not round-trip after reopening";
        EXPECT_TRUE(cam.value("hasForegroundLayer", false)) << "camera FG1 foreground layer did not round-trip after reopening";
    }

    StopEditor(editor);
    ASSERT_TRUE(editor.waitForFinished(5000)) << "editor did not exit after terminate()";
}

// Regression test for two copy/paste bugs: paste applied no bounds clamp at all (a fixed
// +50/+50 offset from ClipBoard::CloneMapObjects, unlike every drag/resize/map-resize path,
// which all go through the same GridPlacement clamp helpers); and MapObjectBase::Clone() left
// the clone's mBaseTlv pointer aliasing the *source* object's own mTlv (the compiler-generated
// copy constructor copies that pointer verbatim), so moving/resizing what looked like the
// pasted copy actually mutated the original, and saving produced an internally-inconsistent
// bottom_right_x/y that came back "huge or broken" on reload.
TEST(EditorAutomation, PasteClampsBoundsAndPreservesSizeThroughEditAndReload)
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

    // Pin the object hard against the map's bottom-right edge - the same out-of-bounds clamp
    // MapObjectDragResizeBoundsAndSnap already exercises for a single item.
    {
        const nlohmann::json before = FindKind(GetSceneItems(client), "map_object");
        const double x = before.at("xpos").get<double>();
        const double y = before.at("ypos").get<double>();
        const double w = before.at("width").get<double>();
        const double h = before.at("height").get<double>();
        DragView(client, SceneToView(client, x + w / 2, y + h / 2), SceneToView(client, 1'000'000, 1'000'000));
    }
    const nlohmann::json original = FindKind(GetSceneItems(client), "map_object");

    // Select it (rubber-band around it - it isn't necessarily still selected merely from being
    // dragged: a single-item drag doesn't require prior selection at all in Qt/QGraphicsView),
    // then copy and paste it.
    {
        const double x = original.at("xpos").get<double>();
        const double y = original.at("ypos").get<double>();
        const double w = original.at("width").get<double>();
        const double h = original.at("height").get<double>();
        DragView(client, SceneToView(client, x - 20, y - 20), SceneToView(client, x + w + 20, y + h + 20));
    }
    ASSERT_TRUE(FindKind(GetSceneItems(client), "map_object").value("selected", false)) << "rubber-band select did not select the object";

    ASSERT_TRUE(client.Call({{"cmd", "click"}, {"target", "actionCopy"}}).value("ok", false));
    ASSERT_TRUE(client.Call({{"cmd", "click"}, {"target", "actionPaste"}}).value("ok", false));

    // PasteItemsCommand clears any prior selection and selects exactly what it just pasted, so
    // the pasted copy is whichever map object now reports selected=true.
    ASSERT_EQ(CountKind(GetSceneItems(client), "map_object"), 2) << "paste did not add a second map object";
    const nlohmann::json pasted = FindSelectedKind(GetSceneItems(client), "map_object");

    // 1. Bounds: the pasted copy must be within the map, not just offset by a fixed +50/+50
    // from an already edge-pinned original.
    EXPECT_LE(pasted.value("xpos", -1.0) + pasted.value("width", 0.0), 100000) << "pasted object escaped the map bounds";
    EXPECT_LE(pasted.value("ypos", -1.0) + pasted.value("height", 0.0), 100000) << "pasted object escaped the map bounds";
    EXPECT_GE(pasted.value("xpos", -1.0), 0);
    EXPECT_GE(pasted.value("ypos", -1.0), 0);

    // 2. Aliasing: moving/resizing the pasted copy must not silently mutate the original.
    {
        const double x = pasted.at("xpos").get<double>();
        const double y = pasted.at("ypos").get<double>();
        const double w = pasted.at("width").get<double>();
        const double h = pasted.at("height").get<double>();
        DragView(client, SceneToView(client, x + w / 2, y + h / 2), SceneToView(client, x + w / 2 - 30, y + h / 2 - 15));
    }
    {
        const nlohmann::json movedPasted = FindSelectedKind(GetSceneItems(client), "map_object");
        const double x = movedPasted.at("xpos").get<double>();
        const double y = movedPasted.at("ypos").get<double>();
        const double w = movedPasted.at("width").get<double>();
        const double h = movedPasted.at("height").get<double>();
        DragView(client, SceneToView(client, x + w, y + h), SceneToView(client, x + w + 25, y + h + 10));
    }

    nlohmann::json movedOriginal;
    nlohmann::json movedPasted;
    {
        const nlohmann::json items = GetSceneItems(client);
        for (const auto& item : items)
        {
            if (item.value("kind", std::string()) != "map_object")
            {
                continue;
            }
            if (item.value("selected", false))
            {
                movedPasted = item;
            }
            else
            {
                movedOriginal = item;
            }
        }
    }
    ASSERT_FALSE(movedOriginal.empty());
    ASSERT_FALSE(movedPasted.empty());

    EXPECT_DOUBLE_EQ(movedOriginal.value("xpos", -1.0), original.at("xpos").get<double>()) << "moving the pasted copy also moved the original (Clone() aliasing bug)";
    EXPECT_DOUBLE_EQ(movedOriginal.value("ypos", -1.0), original.at("ypos").get<double>());
    EXPECT_DOUBLE_EQ(movedOriginal.value("width", -1.0), original.at("width").get<double>());
    EXPECT_DOUBLE_EQ(movedOriginal.value("height", -1.0), original.at("height").get<double>());

    // 3. Save/reload: the moved+resized pasted copy's size must round-trip correctly rather
    // than come back "huge or broken".
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

    int reopenedCount = 0;
    bool foundMatchingPasted = false;
    bool foundMatchingOriginal = false;
    for (const auto& item : GetSceneItems(client))
    {
        if (item.value("kind", std::string()) != "map_object")
        {
            continue;
        }
        reopenedCount++;
        const double w = item.value("width", -1.0);
        const double h = item.value("height", -1.0);
        EXPECT_LT(w, 100000) << "map object width became huge after save/reopen";
        EXPECT_LT(h, 100000) << "map object height became huge after save/reopen";
        EXPECT_GT(w, 0) << "map object width became zero/negative after save/reopen";
        EXPECT_GT(h, 0) << "map object height became zero/negative after save/reopen";

        if (std::abs(item.value("xpos", -1.0) - movedPasted.value("xpos", -2.0)) < 0.5 && std::abs(item.value("width", -1.0) - movedPasted.value("width", -2.0)) < 0.5)
        {
            foundMatchingPasted = true;
        }
        if (std::abs(item.value("xpos", -1.0) - movedOriginal.value("xpos", -2.0)) < 0.5 && std::abs(item.value("width", -1.0) - movedOriginal.value("width", -2.0)) < 0.5)
        {
            foundMatchingOriginal = true;
        }
    }
    EXPECT_EQ(reopenedCount, 2) << "expected both the original and pasted object to still be there after reopening";
    EXPECT_TRUE(foundMatchingPasted) << "pasted object's position/size did not round-trip through save/reopen";
    EXPECT_TRUE(foundMatchingOriginal) << "original object's position/size did not round-trip through save/reopen";

    StopEditor(editor);
    ASSERT_TRUE(editor.waitForFinished(5000)) << "editor did not exit after terminate()";
}

// Regression test for pasting a collision line across paths where the destination path's map is
// smaller than the one the line was copied from: PasteItemsCommand only ever repositioned a
// pasted line (ClampRangeStartX/Y), never shrank one that's wider/taller than the destination
// map itself, so it could still stick out past the far edge even after "clamping". Fixed by
// having ResizeableArrowItem self-correct (shrink-then-reposition, same idea as
// ResizeableRectItem::SyncFromMapObject) in its constructor - which MakeResizeableArrowItem
// calls for every pasted line, same as it always has for every loaded one.
TEST(EditorAutomation, PasteOfOversizedCollisionLineIntoSmallerPathShrinksToFit)
{
    QProcess editor;
    AutomationClient client;
    QString socketName;
    ASSERT_TRUE(LaunchEditorAndConnect(editor, client, socketName));

    // Path A: grow to 3 cameras wide (AO's camera grid cell is 1024x480 - see
    // Model::CameraGridWidth) and stretch a collision line across most of that width, well past
    // the size of a single (default, 1x1) map.
    ASSERT_TRUE(CreateNewPath(client));
    ASSERT_TRUE(SetMapSize(client, 3, 1));
    ASSERT_TRUE(AddCollisionLine(client));
    {
        const nlohmann::json before = FindKind(GetSceneItems(client), "collision_line");
        DragView(client, SceneToView(client, before.at("x2").get<int>(), before.at("y2").get<int>()),
                  SceneToView(client, 3000, before.at("y2").get<int>()));
    }
    const nlohmann::json oversized = FindKind(GetSceneItems(client), "collision_line");
    const int oversizedWidth = std::abs(oversized.at("x2").get<int>() - oversized.at("x1").get<int>());
    ASSERT_GT(oversizedWidth, 1024) << "test setup failed to make the line wider than a single camera";

    // Select it (rubber-band, same idiom as PasteClampsBoundsAndPreservesSizeThroughEditAndReload
    // - a freshly click-placed line isn't necessarily left selected) then copy it.
    {
        const int minX = std::min(oversized.at("x1").get<int>(), oversized.at("x2").get<int>()) - 20;
        const int minY = std::min(oversized.at("y1").get<int>(), oversized.at("y2").get<int>()) - 20;
        const int maxX = std::max(oversized.at("x1").get<int>(), oversized.at("x2").get<int>()) + 20;
        const int maxY = std::max(oversized.at("y1").get<int>(), oversized.at("y2").get<int>()) + 20;
        DragView(client, SceneToView(client, minX, minY), SceneToView(client, maxX, maxY));
    }
    ASSERT_TRUE(FindKind(GetSceneItems(client), "collision_line").value("selected", false)) << "rubber-band select did not select the line";
    ASSERT_TRUE(client.Call({{"cmd", "click"}, {"target", "actionCopy"}}).value("ok", false));

    // Path B: a brand new, still-default (1 camera, 1024x480) path - smaller than the map the
    // line was copied from. CreateNewPath opens it as a new tab and makes it the active one.
    ASSERT_TRUE(CreateNewPath(client));
    ASSERT_EQ(CountKind(GetSceneItems(client), "collision_line"), 0) << "new path already has a collision line";

    ASSERT_TRUE(client.Call({{"cmd", "click"}, {"target", "actionPaste"}}).value("ok", false));

    const nlohmann::json pasted = FindKind(GetSceneItems(client), "collision_line");
    ASSERT_FALSE(pasted.empty()) << "paste did not add a collision line to the smaller path";

    const int pastedWidth = std::abs(pasted.at("x2").get<int>() - pasted.at("x1").get<int>());
    const int pastedMinX = std::min(pasted.at("x1").get<int>(), pasted.at("x2").get<int>());
    const int pastedMinY = std::min(pasted.at("y1").get<int>(), pasted.at("y2").get<int>());
    const int pastedMaxX = std::max(pasted.at("x1").get<int>(), pasted.at("x2").get<int>());
    const int pastedMaxY = std::max(pasted.at("y1").get<int>(), pasted.at("y2").get<int>());

    EXPECT_LE(pastedWidth, 1024) << "pasted line was not shrunk to fit the smaller destination map";
    EXPECT_GE(pastedMinX, 0) << "pasted line escaped the destination map's left edge";
    EXPECT_GE(pastedMinY, 0) << "pasted line escaped the destination map's top edge";
    EXPECT_LE(pastedMaxX, 1024) << "pasted line escaped the destination map's right edge";
    EXPECT_LE(pastedMaxY, 480) << "pasted line escaped the destination map's bottom edge";

    StopEditor(editor);
    ASSERT_TRUE(editor.waitForFinished(5000)) << "editor did not exit after terminate()";
}

// Regression test for opening a path whose saved JSON already contains an out-of-bounds
// collision line (e.g. hand-edited, or from before paste/map-resize bounds clamping existed):
// the line must be moved back within the map's bounds as soon as the path is opened, and that
// correction must not be undoable - undoing it back to the out-of-bounds error condition would
// defeat the point of fixing it on load. EditorTab's constructor builds every collision line via
// MakeResizeableArrowItem, whose ResizeableArrowItem ctor now self-clamps before any QUndoCommand
// exists, so there's nothing on the undo stack that could ever put it back.
TEST(EditorAutomation, OpeningPathWithOutOfBoundsCollisionLineClampsOnLoadNonUndoably)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid()) << "failed to create a temp directory";
    const QString savePath = tempDir.filePath("relive_editor_test_oob_level.json");

    QProcess editor;
    AutomationClient client;
    QString socketName;
    ASSERT_TRUE(LaunchEditorAndConnect(editor, client, socketName));

    ASSERT_TRUE(CreateNewPath(client));
    ASSERT_TRUE(AddCollisionLine(client));

    {
        const auto resp = client.Call({{"cmd", "save_path_as"}, {"path", savePath.toStdString()}});
        ASSERT_TRUE(resp.value("ok", false));
        ASSERT_TRUE(resp.at("result").value("saved", false)) << "save_path_as reported failure";
    }

    // Hand-edit the saved JSON so its one collision line's raw x/w (CollisionObject::X1/X2) is
    // far outside the still-1x1 (1024x480) map's bounds - PathLine's on-disk field names are
    // x/y/w/h (see relive_tlvs_serialization.cpp's to_json/from_json for PathLine), which map to
    // X1/Y1/X2/Y2 respectively (see CollisionObject.hpp).
    {
        QFile file(savePath);
        ASSERT_TRUE(file.open(QIODevice::ReadOnly)) << "failed to reopen saved file for editing";
        const QByteArray contents = file.readAll();
        file.close();

        nlohmann::json root = nlohmann::json::parse(contents.toStdString());
        nlohmann::json& collisions = root.at("map").at("collisions");
        ASSERT_EQ(collisions.size(), 1u) << "expected exactly one collision line in the saved file";
        collisions[0]["x"] = 900;
        collisions[0]["y"] = 100;
        collisions[0]["w"] = 5000;
        collisions[0]["h"] = 100;

        ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Truncate)) << "failed to rewrite saved file";
        const std::string rewritten = root.dump();
        ASSERT_EQ(file.write(rewritten.data(), static_cast<qint64>(rewritten.size())), static_cast<qint64>(rewritten.size()));
        file.close();
    }

    // Close the tab that's still open on savePath first - onOpenPath() treats a file already
    // open in a tab as "just switch to it", skipping the disk read entirely, which would leave
    // this test looking at the pre-edit, still-in-bounds in-memory state rather than actually
    // exercising the load path.
    ASSERT_TRUE(client.Call({{"cmd", "click"}, {"target", "action_close_path"}}).value("ok", false));

    {
        const auto resp = client.Call({{"cmd", "open_path"}, {"path", savePath.toStdString()}});
        ASSERT_TRUE(resp.value("ok", false));
        ASSERT_TRUE(resp.at("result").value("opened", false)) << "open_path reported failure";
    }

    const nlohmann::json afterOpen = FindKind(GetSceneItems(client), "collision_line");
    ASSERT_FALSE(afterOpen.empty()) << "collision line missing after opening the path";

    const int afterOpenMaxX = std::max(afterOpen.at("x1").get<int>(), afterOpen.at("x2").get<int>());
    EXPECT_LE(afterOpenMaxX, 1024) << "out-of-bounds line was not clamped to the map's bounds on load";
    EXPECT_GE(std::min(afterOpen.at("x1").get<int>(), afterOpen.at("x2").get<int>()), 0);

    // Nothing should be on the undo stack for a freshly opened path - clicking undo must be a
    // no-op, not a way back to the on-disk out-of-bounds values.
    ASSERT_TRUE(client.Call({{"cmd", "click"}, {"target", "action_undo"}}).value("ok", false));

    const nlohmann::json afterUndo = FindKind(GetSceneItems(client), "collision_line");
    EXPECT_EQ(afterUndo.value("x1", -1), afterOpen.value("x1", -2)) << "undo changed the clamped-on-load line - the load-time fix must not be undoable";
    EXPECT_EQ(afterUndo.value("y1", -1), afterOpen.value("y1", -2));
    EXPECT_EQ(afterUndo.value("x2", -1), afterOpen.value("x2", -2));
    EXPECT_EQ(afterUndo.value("y2", -1), afterOpen.value("y2", -2));
    const int afterUndoMaxX = std::max(afterUndo.at("x1").get<int>(), afterUndo.at("x2").get<int>());
    EXPECT_LE(afterUndoMaxX, 1024) << "undo put the line back outside the map's bounds";

    StopEditor(editor);
    ASSERT_TRUE(editor.waitForFinished(5000)) << "editor did not exit after terminate()";
}

// Regression test for "Connect collisions": it used to only link two lines whose endpoints
// already coincided exactly (practically never true for two independently-drawn lines, so this
// almost never actually connected anything), never moved anything to make ends meet, and had no
// guard against connecting more than two lines at once. Now it snaps whichever pair of endpoints
// (out of the 4 possible pairings between two lines) is physically nearest together to a shared
// midpoint, and links mNext/mPrevious accordingly - fully undoable/redoable.
TEST(EditorAutomation, ConnectCollisionsSnapsNearestEndsAndLinksNextPrevious)
{
    QProcess editor;
    AutomationClient client;
    QString socketName;
    ASSERT_TRUE(LaunchEditorAndConnect(editor, client, socketName));

    ASSERT_TRUE(CreateNewPath(client));
    ASSERT_TRUE(AddCollisionLine(client));
    ASSERT_TRUE(AddCollisionLine(client));

    // AddCollisionCommand always places a new line at the same fixed offset from the view, so
    // both lines start out fully coincident - id order tells them apart (NextCollisionId()
    // increments), not position, until line 2 is moved below.
    int id1 = -1;
    int id2 = -1;
    {
        const nlohmann::json items = GetSceneItems(client);
        std::vector<int> ids;
        for (const auto& item : items)
        {
            if (item.value("kind", std::string()) == "collision_line")
            {
                ids.push_back(item.value("id", -1));
            }
        }
        ASSERT_EQ(ids.size(), 2u) << "expected exactly two collision lines";
        id1 = std::min(ids[0], ids[1]);
        id2 = std::max(ids[0], ids[1]);
    }

    const nlohmann::json line1Baseline = FindCollisionById(GetSceneItems(client), id1);
    const nlohmann::json line2Baseline = FindCollisionById(GetSceneItems(client), id2);
    ASSERT_EQ(line1Baseline.at("x1").get<int>(), line2Baseline.at("x1").get<int>()) << "test setup expected both freshly-added lines to start coincident";

    // Move line 2 (the most recently added, so still the only thing selected) past line 1's far
    // end, offset by just a little more than line 1's own length (both lines are the same fixed
    // 100px length - see AddCollisionCommand::MakeNewCollision) plus a small extra nudge. A
    // *smaller* shift would be ambiguous: sliding a line along its own direction by less than its
    // length leaves both same-side ends (P1~P1 and P2~P2) nearly equidistant, a near-tie that
    // doesn't actually exercise picking a specific pairing. Shifting past the far end instead
    // makes line 1's P2 and line 2's P1 unambiguously the closest pair of all four.
    {
        const double midX = (line2Baseline.at("x1").get<double>() + line2Baseline.at("x2").get<double>()) / 2;
        const double midY = line2Baseline.at("y1").get<double>();
        const double lineLength = line2Baseline.at("x2").get<double>() - line2Baseline.at("x1").get<double>();
        DragView(client, SceneToView(client, midX, midY), SceneToView(client, midX + lineLength + 10, midY + 5));
    }

    const nlohmann::json line1BeforeConnect = FindCollisionById(GetSceneItems(client), id1);
    const nlohmann::json line2BeforeConnect = FindCollisionById(GetSceneItems(client), id2);
    ASSERT_NE(line1BeforeConnect.at("x2").get<int>(), line2BeforeConnect.at("x1").get<int>()) << "test setup failed to separate the two lines' nearest ends";

    // Select both lines via rubber-band, then connect them.
    {
        const double minX = std::min({line1BeforeConnect.at("x1").get<double>(), line1BeforeConnect.at("x2").get<double>(), line2BeforeConnect.at("x1").get<double>(), line2BeforeConnect.at("x2").get<double>()});
        const double minY = std::min({line1BeforeConnect.at("y1").get<double>(), line1BeforeConnect.at("y2").get<double>(), line2BeforeConnect.at("y1").get<double>(), line2BeforeConnect.at("y2").get<double>()});
        const double maxX = std::max({line1BeforeConnect.at("x1").get<double>(), line1BeforeConnect.at("x2").get<double>(), line2BeforeConnect.at("x1").get<double>(), line2BeforeConnect.at("x2").get<double>()});
        const double maxY = std::max({line1BeforeConnect.at("y1").get<double>(), line1BeforeConnect.at("y2").get<double>(), line2BeforeConnect.at("y1").get<double>(), line2BeforeConnect.at("y2").get<double>()});
        DragView(client, SceneToView(client, minX - 20, minY - 20), SceneToView(client, maxX + 20, maxY + 20));
    }
    {
        const nlohmann::json selected = GetSceneItems(client);
        ASSERT_TRUE(FindCollisionById(selected, id1).value("selected", false)) << "rubber-band drag did not select line 1";
        ASSERT_TRUE(FindCollisionById(selected, id2).value("selected", false)) << "rubber-band drag did not select line 2";
    }

    ASSERT_TRUE(client.Call({{"cmd", "click"}, {"target", "actionConnect_collisions"}}).value("ok", false));
    EXPECT_TRUE(UndoContains(GetUndoWidgetTextList(client), "Connected collisions"));

    const nlohmann::json line1AfterConnect = FindCollisionById(GetSceneItems(client), id1);
    const nlohmann::json line2AfterConnect = FindCollisionById(GetSceneItems(client), id2);

    // The nearest ends (line 1's P2, line 2's P1) must now physically coincide exactly...
    EXPECT_EQ(line1AfterConnect.at("x2").get<int>(), line2AfterConnect.at("x1").get<int>()) << "connected endpoints do not share the same x";
    EXPECT_EQ(line1AfterConnect.at("y2").get<int>(), line2AfterConnect.at("y1").get<int>()) << "connected endpoints do not share the same y";
    // ...the midpoint of where they used to be, specifically (not e.g. one end snapping to the
    // other's original position).
    EXPECT_EQ(line1AfterConnect.at("x2").get<int>(), (line1BeforeConnect.at("x2").get<int>() + line2BeforeConnect.at("x1").get<int>()) / 2);
    EXPECT_EQ(line1AfterConnect.at("y2").get<int>(), (line1BeforeConnect.at("y2").get<int>() + line2BeforeConnect.at("y1").get<int>()) / 2);
    // ...while each line's *far* end - the one that wasn't part of the nearest pair - stays put.
    EXPECT_EQ(line1AfterConnect.at("x1").get<int>(), line1BeforeConnect.at("x1").get<int>()) << "the unconnected end of line 1 moved";
    EXPECT_EQ(line2AfterConnect.at("x2").get<int>(), line2BeforeConnect.at("x2").get<int>()) << "the unconnected end of line 2 moved";

    // Next/previous linkage: line 1's P2 connects, so line 1's Next is line 2; line 2's P1
    // connects, so line 2's Previous is line 1.
    EXPECT_EQ(line1AfterConnect.at("next").get<int>(), id2);
    EXPECT_EQ(line2AfterConnect.at("previous").get<int>(), id1);
    EXPECT_EQ(line1AfterConnect.at("previous").get<int>(), -1) << "line 1's unrelated Previous field should be untouched";
    EXPECT_EQ(line2AfterConnect.at("next").get<int>(), -1) << "line 2's unrelated Next field should be untouched";

    // Undo must restore both lines' exact pre-connect positions and next/previous fields.
    ASSERT_TRUE(client.Call({{"cmd", "click"}, {"target", "action_undo"}}).value("ok", false));
    {
        const nlohmann::json line1AfterUndo = FindCollisionById(GetSceneItems(client), id1);
        const nlohmann::json line2AfterUndo = FindCollisionById(GetSceneItems(client), id2);
        EXPECT_EQ(line1AfterUndo.at("x2").get<int>(), line1BeforeConnect.at("x2").get<int>());
        EXPECT_EQ(line1AfterUndo.at("y2").get<int>(), line1BeforeConnect.at("y2").get<int>());
        EXPECT_EQ(line2AfterUndo.at("x1").get<int>(), line2BeforeConnect.at("x1").get<int>());
        EXPECT_EQ(line2AfterUndo.at("y1").get<int>(), line2BeforeConnect.at("y1").get<int>());
        EXPECT_EQ(line1AfterUndo.at("next").get<int>(), -1) << "undo did not restore line 1's original Next";
        EXPECT_EQ(line2AfterUndo.at("previous").get<int>(), -1) << "undo did not restore line 2's original Previous";
    }

    // Redo must re-apply the exact same connection.
    ASSERT_TRUE(client.Call({{"cmd", "click"}, {"target", "action_redo"}}).value("ok", false));
    {
        const nlohmann::json line1AfterRedo = FindCollisionById(GetSceneItems(client), id1);
        const nlohmann::json line2AfterRedo = FindCollisionById(GetSceneItems(client), id2);
        EXPECT_EQ(line1AfterRedo.at("x2").get<int>(), line1AfterConnect.at("x2").get<int>());
        EXPECT_EQ(line1AfterRedo.at("y2").get<int>(), line1AfterConnect.at("y2").get<int>());
        EXPECT_EQ(line2AfterRedo.at("x1").get<int>(), line2AfterConnect.at("x1").get<int>());
        EXPECT_EQ(line2AfterRedo.at("y1").get<int>(), line2AfterConnect.at("y1").get<int>());
        EXPECT_EQ(line1AfterRedo.at("next").get<int>(), id2);
        EXPECT_EQ(line2AfterRedo.at("previous").get<int>(), id1);
    }

    StopEditor(editor);
    ASSERT_TRUE(editor.waitForFinished(5000)) << "editor did not exit after terminate()";
}

// "Connect collisions" only makes sense for exactly two lines - selecting more (or fewer) must
// show a clear error instead of silently doing nothing or connecting an arbitrary subset.
TEST(EditorAutomation, ConnectCollisionsRequiresExactlyTwoSelected)
{
    QProcess editor;
    AutomationClient client;
    QString socketName;
    ASSERT_TRUE(LaunchEditorAndConnect(editor, client, socketName));

    ASSERT_TRUE(CreateNewPath(client));
    ASSERT_TRUE(AddCollisionLine(client));
    ASSERT_TRUE(AddCollisionLine(client));
    ASSERT_TRUE(AddCollisionLine(client));

    // All three lines are coincident (see the comment in the test above) - a rubber-band drag
    // around that single point selects all of them at once.
    const nlohmann::json line = FindKind(GetSceneItems(client), "collision_line");
    const QPoint at = SceneToView(client, line.at("x1").get<int>(), line.at("y1").get<int>());
    DragView(client, QPoint(at.x() - 20, at.y() - 20), QPoint(at.x() + 20, at.y() + 20));
    {
        int selectedCount = 0;
        for (const auto& item : GetSceneItems(client))
        {
            if (item.value("kind", std::string()) == "collision_line" && item.value("selected", false))
            {
                selectedCount++;
            }
        }
        ASSERT_EQ(selectedCount, 3) << "rubber-band drag did not select all three lines";
    }

    const int clickId = client.SendCommand({{"cmd", "click"}, {"target", "actionConnect_collisions"}});
    ASSERT_TRUE(client.Call({{"cmd", "get_state"}, {"target", "@active_modal"}}).value("ok", false)) << "no error dialog shown for more than two selected lines";
    ASSERT_TRUE(client.Call({{"cmd", "click"}, {"target", "@active_modal_button:OK"}}).value("ok", false));
    EXPECT_TRUE(client.WaitForResponse(clickId, 5000).value("ok", false));

    EXPECT_FALSE(UndoContains(GetUndoWidgetTextList(client), "Connected collisions")) << "an invalid selection should not have pushed a connect command";

    StopEditor(editor);
    ASSERT_TRUE(editor.waitForFinished(5000)) << "editor did not exit after terminate()";
}

// Feature test for the click-to-place "add collision" tool: actionAdd_collision no longer
// creates a line immediately - the first click sets one endpoint (showing a ghost preview line
// that follows the mouse without a button held, via mouse_event's "move" phase), and the second
// click commits the real collision line at the two clicked points.
TEST(EditorAutomation, AddCollisionClickToPlaceShowsGhostAndCreatesLine)
{
    QProcess editor;
    AutomationClient client;
    QString socketName;
    ASSERT_TRUE(LaunchEditorAndConnect(editor, client, socketName));

    ASSERT_TRUE(CreateNewPath(client));
    ASSERT_TRUE(client.Call({{"cmd", "click"}, {"target", "actionAdd_collision"}}).value("ok", false));

    // Nothing exists yet - not even a ghost - until the first click.
    {
        const nlohmann::json items = GetSceneItems(client);
        EXPECT_EQ(CountKind(items, "collision_line"), 0);
        EXPECT_EQ(CountKind(items, "collision_ghost_line"), 0);
    }

    const QPoint p1 = SceneToView(client, 100, 100);
    const QPoint p2a = SceneToView(client, 250, 120); // an intermediate mouse position
    const QPoint p2 = SceneToView(client, 300, 180);  // where the second click actually lands

    // First click: places the first endpoint and shows the ghost, but creates no real line yet.
    ASSERT_TRUE(SendMouseEvent(client, "down", p1));
    ASSERT_TRUE(SendMouseEvent(client, "up", p1));
    {
        const nlohmann::json items = GetSceneItems(client);
        EXPECT_EQ(CountKind(items, "collision_line"), 0) << "a real collision line was created after only one click";
        const nlohmann::json ghost = FindKind(items, "collision_ghost_line");
        EXPECT_DOUBLE_EQ(ghost.value("x1", -1.0), 100);
        EXPECT_DOUBLE_EQ(ghost.value("y1", -1.0), 100);
        EXPECT_DOUBLE_EQ(ghost.value("x2", -1.0), 100) << "ghost should start collapsed to the first click point";
        EXPECT_DOUBLE_EQ(ghost.value("y2", -1.0), 100);
    }

    // Moving the mouse (no button held) before the second click updates the ghost's far end to
    // follow the cursor, without touching the first endpoint.
    ASSERT_TRUE(SendMouseEvent(client, "move", p2a));
    {
        const nlohmann::json ghost = FindKind(GetSceneItems(client), "collision_ghost_line");
        EXPECT_DOUBLE_EQ(ghost.value("x1", -1.0), 100);
        EXPECT_DOUBLE_EQ(ghost.value("y1", -1.0), 100);
        EXPECT_DOUBLE_EQ(ghost.value("x2", -1.0), 250);
        EXPECT_DOUBLE_EQ(ghost.value("y2", -1.0), 120);
    }
    ASSERT_TRUE(SendMouseEvent(client, "move", p2));
    {
        const nlohmann::json ghost = FindKind(GetSceneItems(client), "collision_ghost_line");
        EXPECT_DOUBLE_EQ(ghost.value("x2", -1.0), 300);
        EXPECT_DOUBLE_EQ(ghost.value("y2", -1.0), 180);
    }

    // Second click: commits the real line at the two clicked points and removes the ghost.
    ASSERT_TRUE(SendMouseEvent(client, "down", p2));
    ASSERT_TRUE(SendMouseEvent(client, "up", p2));

    {
        const nlohmann::json items = GetSceneItems(client);
        EXPECT_EQ(CountKind(items, "collision_ghost_line"), 0) << "ghost line was not removed after the second click";
        const nlohmann::json line = FindKind(items, "collision_line");
        EXPECT_EQ(line.value("x1", -1), 100);
        EXPECT_EQ(line.value("y1", -1), 100);
        EXPECT_EQ(line.value("x2", -1), 300);
        EXPECT_EQ(line.value("y2", -1), 180);
    }
    // "Add collision line" must be the *last* entry pushed - the mouse release that follows this
    // same click arrives after placement has already ended (during the preceding press), so it
    // falls through to the scene's normal, non-placing release handling; if that stale selection-
    // change detection isn't reset when placement starts/commits, it spuriously "sees" the
    // selection AddCollisionCommand::redo() just made and pushes an extra SetSelectionCommand
    // ("Select 1 item(s)") on top, which would make the *next* undo below undo the wrong thing.
    {
        const auto undoItems = GetUndoWidgetTextList(client);
        EXPECT_TRUE(UndoContains(undoItems, "Add collision line"));
        ASSERT_FALSE(undoItems.empty());
        EXPECT_EQ(undoItems.back(), "Add collision line") << "an extra command was pushed after committing the line";
    }

    // Undo/redo still work normally for the committed line.
    ASSERT_TRUE(client.Call({{"cmd", "click"}, {"target", "action_undo"}}).value("ok", false));
    EXPECT_EQ(CountKind(GetSceneItems(client), "collision_line"), 0) << "undo did not remove the added line";

    ASSERT_TRUE(client.Call({{"cmd", "click"}, {"target", "action_redo"}}).value("ok", false));
    {
        const nlohmann::json line = FindKind(GetSceneItems(client), "collision_line");
        EXPECT_EQ(line.value("x1", -1), 100);
        EXPECT_EQ(line.value("y2", -1), 180);
    }

    StopEditor(editor);
    ASSERT_TRUE(editor.waitForFinished(5000)) << "editor did not exit after terminate()";
}

// Escape must cancel an in-progress placement cleanly: remove the ghost, push no command, and
// leave the tool usable again afterward (not "stuck" half-placed).
TEST(EditorAutomation, AddCollisionClickToPlaceCancelledByEscape)
{
    QProcess editor;
    AutomationClient client;
    QString socketName;
    ASSERT_TRUE(LaunchEditorAndConnect(editor, client, socketName));

    ASSERT_TRUE(CreateNewPath(client));
    ASSERT_TRUE(client.Call({{"cmd", "click"}, {"target", "actionAdd_collision"}}).value("ok", false));

    const QPoint p1 = SceneToView(client, 100, 100);
    ASSERT_TRUE(SendMouseEvent(client, "down", p1));
    ASSERT_TRUE(SendMouseEvent(client, "up", p1));
    ASSERT_EQ(CountKind(GetSceneItems(client), "collision_ghost_line"), 1) << "test setup: expected the ghost to appear after the first click";

    ASSERT_TRUE(client.Call({{"cmd", "send_key"}, {"target", "graphicsView"}, {"key", "Escape"}}).value("ok", false));

    {
        const nlohmann::json items = GetSceneItems(client);
        EXPECT_EQ(CountKind(items, "collision_ghost_line"), 0) << "Escape did not remove the ghost line";
        EXPECT_EQ(CountKind(items, "collision_line"), 0) << "Escape should not have created a real collision line";
    }
    EXPECT_FALSE(UndoContains(GetUndoWidgetTextList(client), "Add collision line")) << "cancelling placement should not push an undo command";

    // A fresh "add collision" afterward should work normally, proving cancellation didn't leave
    // the tool stuck in a half-placed state.
    ASSERT_TRUE(client.Call({{"cmd", "click"}, {"target", "actionAdd_collision"}}).value("ok", false));
    const QPoint q1 = SceneToView(client, 50, 60);
    const QPoint q2 = SceneToView(client, 150, 60);
    ASSERT_TRUE(SendMouseEvent(client, "down", q1));
    ASSERT_TRUE(SendMouseEvent(client, "up", q1));
    ASSERT_TRUE(SendMouseEvent(client, "down", q2));
    ASSERT_TRUE(SendMouseEvent(client, "up", q2));
    EXPECT_EQ(CountKind(GetSceneItems(client), "collision_line"), 1) << "tool did not work correctly after a cancelled placement";

    StopEditor(editor);
    ASSERT_TRUE(editor.waitForFinished(5000)) << "editor did not exit after terminate()";
}

// Regression test for two more click-to-place bugs: (1) only the scene's own click handling was
// intercepted, which doesn't stop QGraphicsView's independent RubberBandDrag machinery - so a
// click that turns into a drag before release (including the *completing* click specifically,
// since ending placement restores normal drag mode as part of handling that very press, before
// QGraphicsView's own rubber-band-start check runs) could still show/perform a selection
// rectangle while placing; and (2) actionAdd_collision now stays visibly "depressed" (checked)
// for as long as placement is active, like the grid-snap toggle actions.
TEST(EditorAutomation, AddCollisionClickToPlaceSuppressesRubberBandAndTogglesButton)
{
    QProcess editor;
    AutomationClient client;
    QString socketName;
    ASSERT_TRUE(LaunchEditorAndConnect(editor, client, socketName));

    ASSERT_TRUE(CreateNewPath(client));
    ASSERT_TRUE(AddMapObject(client));

    auto findAddCollisionAction = [&]() -> nlohmann::json
    {
        const auto resp = client.Call({{"cmd", "list_actions"}});
        EXPECT_TRUE(resp.value("ok", false));
        for (const auto& action : resp.at("result"))
        {
            if (action.value("objectName", std::string()) == "actionAdd_collision")
            {
                return action;
            }
        }
        ADD_FAILURE() << "actionAdd_collision not found in list_actions result";
        return nlohmann::json::object();
    };

    {
        const nlohmann::json action = findAddCollisionAction();
        EXPECT_TRUE(action.value("checkable", false)) << "actionAdd_collision should be checkable so it can show a depressed state";
        EXPECT_FALSE(action.value("checked", true)) << "should not start checked";
    }

    const nlohmann::json mapObject = FindKind(GetSceneItems(client), "map_object");

    ASSERT_TRUE(client.Call({{"cmd", "click"}, {"target", "actionAdd_collision"}}).value("ok", false));
    EXPECT_TRUE(findAddCollisionAction().value("checked", false)) << "button did not depress once placement started";

    // First point: a clean click.
    const QPoint p1 = SceneToView(client, 20, 20);
    ASSERT_TRUE(SendMouseEvent(client, "down", p1));
    ASSERT_TRUE(SendMouseEvent(client, "up", p1));

    // Second point: a click that turns into a drag across the map object before releasing -
    // exactly the gesture that used to leak through to rubber-band selection.
    const double objX = mapObject.at("xpos").get<double>();
    const double objY = mapObject.at("ypos").get<double>();
    const double objW = mapObject.at("width").get<double>();
    const double objH = mapObject.at("height").get<double>();
    const QPoint dragFrom = SceneToView(client, objX - 20, objY - 20);
    const QPoint dragTo = SceneToView(client, objX + objW + 20, objY + objH + 20);
    ASSERT_TRUE(SendMouseEvent(client, "down", dragFrom));
    ASSERT_TRUE(SendMouseEvent(client, "move", dragTo));
    ASSERT_TRUE(SendMouseEvent(client, "up", dragTo));

    {
        const nlohmann::json items = GetSceneItems(client);
        EXPECT_FALSE(FindKind(items, "map_object").value("selected", false)) << "rubber-band selection was not suppressed while placing a collision line";

        const nlohmann::json line = FindKind(items, "collision_line");
        EXPECT_DOUBLE_EQ(line.at("x1").get<double>(), 20);
        EXPECT_DOUBLE_EQ(line.at("y1").get<double>(), 20);
        // Placement acts on the *press*, not a completed drag gesture - the line's second point
        // is wherever dragFrom landed, not dragTo.
        EXPECT_DOUBLE_EQ(line.value("x2", -1.0), objX - 20);
        EXPECT_DOUBLE_EQ(line.value("y2", -1.0), objY - 20);
    }
    EXPECT_FALSE(findAddCollisionAction().value("checked", true)) << "button should un-depress once placement finished";

    StopEditor(editor);
    ASSERT_TRUE(editor.waitForFinished(5000)) << "editor did not exit after terminate()";
}

// Regression coverage for object deletion: the Delete key removes the selected map object via
// DeleteItemsCommand, undo restores it exactly, redo re-deletes it, and saving+reopening the
// path leaves it gone rather than resurrected from a stale in-memory reference.
TEST(EditorAutomation, DeleteObjectUndoRedoAndSaveRemovesIt)
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

    const nlohmann::json before = FindKind(GetSceneItems(client), "map_object");

    // Rubber-band select it - same idiom as PasteClampsBoundsAndPreservesSizeThroughEditAndReload.
    {
        const double x = before.at("xpos").get<double>();
        const double y = before.at("ypos").get<double>();
        const double w = before.at("width").get<double>();
        const double h = before.at("height").get<double>();
        DragView(client, SceneToView(client, x - 20, y - 20), SceneToView(client, x + w + 20, y + h + 20));
    }
    ASSERT_TRUE(FindKind(GetSceneItems(client), "map_object").value("selected", false)) << "rubber-band select did not select the object";

    ASSERT_TRUE(client.Call({{"cmd", "send_key"}, {"target", "graphicsView"}, {"key", "Delete"}}).value("ok", false));

    EXPECT_EQ(CountKind(GetSceneItems(client), "map_object"), 0) << "Delete key did not remove the selected object";
    EXPECT_TRUE(UndoContains(GetUndoWidgetTextList(client), "Delete 1 item(s)")) << "no undo entry recorded for the deletion";

    // Undo restores it, at exactly its pre-delete position/size/type.
    ASSERT_TRUE(client.Call({{"cmd", "click"}, {"target", "action_undo"}}).value("ok", false));
    {
        const nlohmann::json afterUndo = FindKind(GetSceneItems(client), "map_object");
        EXPECT_DOUBLE_EQ(afterUndo.value("xpos", -1.0), before.at("xpos").get<double>());
        EXPECT_DOUBLE_EQ(afterUndo.value("ypos", -1.0), before.at("ypos").get<double>());
        EXPECT_DOUBLE_EQ(afterUndo.value("width", -1.0), before.at("width").get<double>());
        EXPECT_DOUBLE_EQ(afterUndo.value("height", -1.0), before.at("height").get<double>());
        EXPECT_EQ(afterUndo.value("tlvType", -1), before.at("tlvType").get<int>());
    }

    // Redo re-deletes it.
    ASSERT_TRUE(client.Call({{"cmd", "click"}, {"target", "action_redo"}}).value("ok", false));
    EXPECT_EQ(CountKind(GetSceneItems(client), "map_object"), 0) << "redo did not reapply the deletion";

    // Save while the object is deleted, then reopen: it must not come back.
    {
        const auto resp = client.Call({{"cmd", "save_path_as"}, {"path", savePath.toStdString()}});
        ASSERT_TRUE(resp.value("ok", false));
        ASSERT_TRUE(resp.at("result").value("saved", false)) << "save_path_as reported failure";
    }
    {
        const auto resp = client.Call({{"cmd", "open_path"}, {"path", savePath.toStdString()}});
        ASSERT_TRUE(resp.value("ok", false));
        ASSERT_TRUE(resp.at("result").value("opened", false)) << "open_path reported failure";
    }

    EXPECT_EQ(CountKind(GetSceneItems(client), "map_object"), 0) << "deleted object reappeared after saving and reopening the path";

    // Check the on-disk json directly too, not just what the reopened in-memory scene shows -
    // belt and suspenders against a bug that only affects one of the two.
    {
        QFile f(savePath);
        ASSERT_TRUE(f.open(QFile::ReadOnly | QFile::Text));
        const auto j = nlohmann::json::parse(f.readAll().toStdString());
        for (const auto& camera : j.at("map").at("cameras"))
        {
            EXPECT_TRUE(camera.value("map_objects", nlohmann::json::array()).empty())
                << "saved json still contains a map object in camera " << camera.dump();
        }
    }

    StopEditor(editor);
    ASSERT_TRUE(editor.waitForFinished(5000)) << "editor did not exit after terminate()";
}

// Regression coverage for camera deletion: DeleteCameraCommand must delete the camera's own map
// objects along with it - not just visually hide the camera while quietly keeping its objects
// around under an untracked "blank" camera - undo must restore the camera and its objects
// together, redo must re-delete both, and saving+reopening the path must not resurrect either
// the camera or its objects. Drives deletion via the real UI path (right-click the camera ->
// "Edit camera" -> "Delete camera" - see DeleteCameraViaContextMenu) using the generic
// "context_menu" automation command, rather than a domain-specific automation-only bypass.
TEST(EditorAutomation, DeleteCameraUndoRedoAndSaveRemovesCameraAndItsObjects)
{
    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid()) << "failed to create a temp directory";
    const QString savePath = tempDir.filePath("relive_editor_test_level.json");
    const QString mainImagePath = tempDir.filePath("sample_main.png");
    {
        QImage main(64, 24, QImage::Format_RGB32);
        main.fill(QColor(80, 80, 200));
        ASSERT_TRUE(main.save(mainImagePath)) << "failed to write the sample main camera image";
    }

    QProcess editor;
    AutomationClient client;
    QString socketName;
    ASSERT_TRUE(LaunchEditorAndConnect(editor, client, socketName));

    ASSERT_TRUE(CreateNewPath(client));

    // set_camera_image (and the FG1 sidecar/image saves DeleteCameraCommand doesn't touch, but
    // EditorTab::mPathDirectory is what matters here) only work once the tab has a real on-disk
    // path - same prerequisite as CameraAddSaveAndFg1LayerRoundTrip.
    {
        const auto resp = client.Call({{"cmd", "save_path_as"}, {"path", savePath.toStdString()}});
        ASSERT_TRUE(resp.value("ok", false));
        ASSERT_TRUE(resp.at("result").value("saved", false));
    }

    // Turn the only camera in this fresh 1x1 path into a real, named camera.
    {
        const auto resp = client.Call({{"cmd", "set_camera_image"}, {"x", 0}, {"y", 0}, {"layer", "main"}, {"image_path", mainImagePath.toStdString()}});
        ASSERT_TRUE(resp.value("ok", false));
        ASSERT_TRUE(resp.at("result").value("ok", false)) << "failed to create the camera";
    }
    ASSERT_EQ(FindKind(GetSceneItems(client), "camera").value("camName", std::string()), "0");

    // Add a map object - on a 1x1 map it lands inside the only (now real) camera.
    ASSERT_TRUE(AddMapObject(client));
    const nlohmann::json objectBefore = FindKind(GetSceneItems(client), "map_object");

    // Right-click inside the camera cell to delete it, exactly the way a user would.
    ASSERT_TRUE(DeleteCameraViaContextMenu(client, SceneToView(client, 10, 10)));

    {
        const nlohmann::json items = GetSceneItems(client);
        EXPECT_EQ(CountKind(items, "map_object"), 0) << "deleting the camera did not remove its map object";
        EXPECT_TRUE(FindKind(items, "camera").value("camName", std::string("unset")).empty()) << "camera still present after deletion";
    }
    EXPECT_TRUE(UndoContainsPrefix(GetUndoWidgetTextList(client), "Delete camera at")) << "no undo entry recorded for the camera deletion";

    // Undo restores both the camera and its map object, at the object's exact pre-delete
    // position/size.
    ASSERT_TRUE(client.Call({{"cmd", "click"}, {"target", "action_undo"}}).value("ok", false));
    {
        const nlohmann::json items = GetSceneItems(client);
        EXPECT_EQ(FindKind(items, "camera").value("camName", std::string()), "0") << "undo did not restore the camera";

        const nlohmann::json obj = FindKind(items, "map_object");
        EXPECT_DOUBLE_EQ(obj.value("xpos", -1.0), objectBefore.at("xpos").get<double>()) << "undo did not restore the object inside the deleted camera";
        EXPECT_DOUBLE_EQ(obj.value("ypos", -1.0), objectBefore.at("ypos").get<double>());
        EXPECT_DOUBLE_EQ(obj.value("width", -1.0), objectBefore.at("width").get<double>());
        EXPECT_DOUBLE_EQ(obj.value("height", -1.0), objectBefore.at("height").get<double>());
    }

    // Redo re-deletes both.
    ASSERT_TRUE(client.Call({{"cmd", "click"}, {"target", "action_redo"}}).value("ok", false));
    {
        const nlohmann::json items = GetSceneItems(client);
        EXPECT_EQ(CountKind(items, "map_object"), 0) << "redo did not reapply the object deletion";
        EXPECT_TRUE(FindKind(items, "camera").value("camName", std::string("unset")).empty()) << "redo did not reapply the camera deletion";
    }

    // Save while both are deleted, then reopen: neither the camera nor its object should come
    // back.
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
        const nlohmann::json items = GetSceneItems(client);
        EXPECT_EQ(CountKind(items, "map_object"), 0) << "deleted object reappeared after saving and reopening the path";
        EXPECT_TRUE(FindKind(items, "camera").value("camName", std::string("unset")).empty()) << "deleted camera reappeared after saving and reopening the path";
    }

    // Check the on-disk json directly too.
    {
        QFile f(savePath);
        ASSERT_TRUE(f.open(QFile::ReadOnly | QFile::Text));
        const auto j = nlohmann::json::parse(f.readAll().toStdString());
        for (const auto& camera : j.at("map").at("cameras"))
        {
            EXPECT_NE(camera.value("name", std::string()), "0") << "saved json still contains the deleted camera";
            EXPECT_TRUE(camera.value("map_objects", nlohmann::json::array()).empty())
                << "saved json still contains a map object in camera " << camera.dump();
        }
    }

    StopEditor(editor);
    ASSERT_TRUE(editor.waitForFinished(5000)) << "editor did not exit after terminate()";
}

// Regression test: the properties panel let a map object's width/height/xpos be set to an
// arbitrary value with no bounds checking at all - BasicTypeProperty's spin box only limits the
// value to the field's own integer range (e.g. s32), and ChangeBasicTypePropertyCommand writes
// it straight into the model via a raw pointer. Every mouse-driven path (resize handles, drag,
// paste, map-resize) goes through IGridPointSnapper's clamp instead; ResizeableRectItem::
// SyncFromMapObject - the one place that turns a model change back into on-screen geometry -
// now applies the same clamp regardless of how the model got mutated. Drives the panel via the
// "click_tree_item" automation command (which spawns a property row's transient editor widget
// exactly like a real click does) and "set_value" on the resulting BigSpinBox.
