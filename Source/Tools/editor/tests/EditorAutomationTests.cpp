// End-to-end regression test for relive-editor's automation channel: launches the real
// editor binary with an explicit --automation-socket, drives it over that socket exactly
// the way an external tool would (AutomationCli, or me via Bash), and asserts it can open
// the About dialog, close it, and shut the app down cleanly.
//
// This intentionally talks the wire protocol directly via AutomationProtocol.hpp rather than
// shelling out to relive-editor-automation-cli, so a bug in the CLI can't mask a protocol bug.

#include "AutomationProtocol.hpp"

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QColor>
#include <QDateTime>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QLocalSocket>
#include <QPoint>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QThread>

#include <algorithm>

#include <map>
#include <string>
#include <vector>

namespace
{
    // Sends JSON commands to an editor automation socket and matches responses back to
    // requests by "id". Responses are not guaranteed to arrive in request order - a click
    // that opens a modal dialog blocks its own response until the dialog closes, so a
    // command sent afterwards (e.g. to dismiss that dialog) can be answered first. Any
    // response that arrives before the caller asks for its id is stashed in mPending.
    class AutomationClient final
    {
    public:
        bool ConnectWithRetry(const QString& name, int timeoutMs)
        {
            QElapsedTimer timer;
            timer.start();
            while (timer.elapsed() < timeoutMs)
            {
                mSocket.connectToServer(name);
                if (mSocket.waitForConnected(50))
                {
                    return true;
                }
                mSocket.abort();
                QThread::msleep(20);
            }
            return false;
        }

        int SendCommand(nlohmann::json cmd)
        {
            const int id = mNextId++;
            cmd["id"] = id;
            Automation::WriteFrame(&mSocket, cmd);
            return id;
        }

        nlohmann::json WaitForResponse(int id, int timeoutMs)
        {
            if (const auto it = mPending.find(id); it != mPending.end())
            {
                nlohmann::json result = it->second;
                mPending.erase(it);
                return result;
            }

            QElapsedTimer timer;
            timer.start();
            while (timer.elapsed() < timeoutMs)
            {
                if (auto frame = mReader.TryTakeFrame())
                {
                    if (frame->is_discarded())
                    {
                        continue;
                    }
                    const int frameId = frame->value("id", -1);
                    if (frameId == id)
                    {
                        return *frame;
                    }
                    mPending[frameId] = *frame;
                    continue;
                }

                if (mSocket.waitForReadyRead(50))
                {
                    mReader.Append(mSocket.readAll());
                }
            }

            ADD_FAILURE() << "timed out waiting for response id=" << id;
            return nlohmann::json::object();
        }

        // Sends a command and waits for its response in one call - most call sites don't need
        // the id split out, they just want "did this succeed" or the result payload.
        nlohmann::json Call(nlohmann::json cmd, int timeoutMs = 5000)
        {
            return WaitForResponse(SendCommand(std::move(cmd)), timeoutMs);
        }

    private:
        QLocalSocket mSocket;
        Automation::FrameReader mReader;
        std::map<int, nlohmann::json> mPending;
        int mNextId = 1;
    };

    // Launches relive-editor with a fresh, PID-qualified automation socket name and connects
    // to it. ASSERTs (via the returned bool) that both steps succeed.
    bool LaunchEditorAndConnect(QProcess& editor, AutomationClient& client, QString& outSocketName)
    {
        outSocketName = QStringLiteral("relive-editor-automation-test-%1-%2")
                            .arg(QCoreApplication::applicationPid())
                            .arg(QDateTime::currentMSecsSinceEpoch());

        // This build may be compiled with AddressSanitizer/LeakSanitizer. Showing any Qt
        // dialog on this system lazily initializes a GTK theme plugin that LSan reports as a
        // leak (reproduced independent of anything this test does: a run that never opens a
        // dialog exits clean). That's a third-party leak outside what these tests verify -
        // UI automation and clean shutdown - so leak detection is disabled for the child
        // process only; it stays on for everything else.
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("ASAN_OPTIONS", "detect_leaks=0");
        // Qt's "offscreen" platform plugin renders to an in-memory backing store instead of a
        // real window - QWidget::grab() (screenshot) and synthesized mouse/key events both
        // still work exactly the same, but nothing appears on the real display or steals
        // keyboard/mouse focus from whatever else is running there while these tests run.
        env.insert("QT_QPA_PLATFORM", "offscreen");
        editor.setProcessEnvironment(env);

        editor.start(QString::fromUtf8(RELIVE_EDITOR_PATH), {QStringLiteral("--automation-socket=%1").arg(outSocketName)});
        if (!editor.waitForStarted(5000))
        {
            ADD_FAILURE() << "editor failed to start: " << editor.errorString().toStdString();
            return false;
        }

        if (!client.ConnectWithRetry(outSocketName, 10000))
        {
            ADD_FAILURE() << "failed to connect to automation socket";
            return false;
        }

        return true;
    }

    // Recursively searches a get_state tree (see AutomationCommands.cpp's DescribeObject) for
    // a node with the given className.
    bool ContainsClassName(const nlohmann::json& node, const std::string& className)
    {
        if (node.value("className", std::string()) == className)
        {
            return true;
        }
        for (const auto& child : node.value("children", nlohmann::json::array()))
        {
            if (ContainsClassName(child, className))
            {
                return true;
            }
        }
        return false;
    }

    // Creates a new path via actionNew_path, accepting both of its sequential "path id"/"game"
    // QInputDialogs with their defaults (0, AO) via the "@active_modal" target - they have no
    // objectName of their own, unlike AboutDialog.
    bool CreateNewPath(AutomationClient& client)
    {
        const int newPathClickId = client.SendCommand({{"cmd", "click"}, {"target", "actionNew_path"}});
        if (!client.Call({{"cmd", "send_key"}, {"target", "@active_modal"}, {"key", "Return"}}).value("ok", false))
        {
            ADD_FAILURE() << "no active modal for path id dialog";
            return false;
        }
        if (!client.Call({{"cmd", "send_key"}, {"target", "@active_modal"}, {"key", "Return"}}).value("ok", false))
        {
            ADD_FAILURE() << "no active modal for game dialog";
            return false;
        }
        return client.WaitForResponse(newPathClickId, 5000).value("ok", false);
    }

    // Adds a collision line: no dialog, pushes straight onto the undo stack.
    bool AddCollisionLine(AutomationClient& client)
    {
        return client.Call({{"cmd", "click"}, {"target", "actionAdd_collision"}}).value("ok", false);
    }

    // Adds a map object via AddObjectDialog: select the first entry in the (unfiltered) type
    // list and accept.
    bool AddMapObject(AutomationClient& client)
    {
        const int addObjectClickId = client.SendCommand({{"cmd", "click"}, {"target", "actionAdd_object"}});
        if (!client.Call({{"cmd", "set_value"}, {"target", "lstObjects"}, {"value", 0}}).value("ok", false))
        {
            ADD_FAILURE() << "failed to select an object type";
            return false;
        }
        if (!client.Call({{"cmd", "send_key"}, {"target", "@active_modal"}, {"key", "Return"}}).value("ok", false))
        {
            ADD_FAILURE() << "no active modal for AddObjectDialog";
            return false;
        }
        return client.WaitForResponse(addObjectClickId, 5000).value("ok", false);
    }

    // Returns get_scene_items' "items" array (collision lines, map objects, cameras, ...).
    nlohmann::json GetSceneItems(AutomationClient& client)
    {
        const auto resp = client.Call({{"cmd", "get_scene_items"}});
        EXPECT_TRUE(resp.value("ok", false));
        return resp.at("result").value("items", nlohmann::json::array());
    }

    // Finds the first item of a given "kind" (e.g. "collision_line", "map_object") in a
    // get_scene_items result. Fails the test if none match.
    nlohmann::json FindKind(const nlohmann::json& items, const std::string& kind)
    {
        for (const auto& item : items)
        {
            if (item.value("kind", std::string()) == kind)
            {
                return item;
            }
        }
        ADD_FAILURE() << "no '" << kind << "' item in scene: " << items.dump();
        return nlohmann::json::object();
    }

    // The undo stack's list of command description strings, in oldest-to-newest order, as
    // shown in the QUndoView ("undoView") - e.g. "Add collision line", "Move 3 item(s)". Works
    // because AutomationCommands.cpp special-cases any QAbstractItemView (QUndoView included)
    // and dumps column-0 display text per row into the "items" field.
    std::vector<std::string> GetUndoWidgetTextList(AutomationClient& client)
    {
        const auto resp = client.Call({{"cmd", "get_state"}, {"target", "undoView"}});
        EXPECT_TRUE(resp.value("ok", false));

        std::vector<std::string> items;
        for (const auto& item : resp.at("result").value("items", nlohmann::json::array()))
        {
            items.push_back(item.get<std::string>());
        }
        return items;
    }

    bool UndoContains(const std::vector<std::string>& items, const std::string& text)
    {
        return std::find(items.begin(), items.end(), text) != items.end();
    }

    // True if any entry starts with the given prefix - for descriptions that embed a
    // generated/variable suffix, e.g. "Add new object <TypeName>".
    bool UndoContainsPrefix(const std::vector<std::string>& items, const std::string& prefix)
    {
        return std::any_of(items.begin(), items.end(), [&](const std::string& item)
                            { return item.rfind(prefix, 0) == 0; });
    }

    QPoint SceneToView(AutomationClient& client, double x, double y)
    {
        const auto resp = client.Call({{"cmd", "scene_to_view"}, {"x", x}, {"y", y}});
        EXPECT_TRUE(resp.value("ok", false));
        return QPoint(resp.at("result").value("x", 0), resp.at("result").value("y", 0));
    }

    void DragView(AutomationClient& client, QPoint from, QPoint to)
    {
        EXPECT_TRUE(client.Call({
                                    {"cmd", "drag"}, {"target", "graphicsView"},
                                    {"from", {{"x", from.x()}, {"y", from.y()}}},
                                    {"to", {{"x", to.x()}, {"y", to.y()}}},
                                })
                        .value("ok", false));
    }

    // Sends a single press/move/release event to graphicsView - unlike DragView (which does all
    // three as one atomic "drag" call), these let a test inspect state *between* them (e.g. a
    // get_scene_items call after MouseMove but before MouseUp, to check something is clamped
    // live during a drag rather than only once released).
    bool SendMouseEvent(AutomationClient& client, const char* phase, QPoint pos)
    {
        return client.Call({{"cmd", "mouse_event"}, {"target", "graphicsView"}, {"phase", phase}, {"x", pos.x()}, {"y", pos.y()}}).value("ok", false);
    }

    // Opens ChangeMapSizeDialog (actionEdit_map_size), sets both spinboxes and accepts via
    // Return - same "@active_modal" idiom as CreateNewPath/AddMapObject's dialogs.
    bool SetMapSize(AutomationClient& client, int x, int y)
    {
        const int clickId = client.SendCommand({{"cmd", "click"}, {"target", "actionEdit_map_size"}});
        if (!client.Call({{"cmd", "set_value"}, {"target", "spnXSize"}, {"value", x}}).value("ok", false))
        {
            ADD_FAILURE() << "failed to set spnXSize";
            return false;
        }
        if (!client.Call({{"cmd", "set_value"}, {"target", "spnYSize"}, {"value", y}}).value("ok", false))
        {
            ADD_FAILURE() << "failed to set spnYSize";
            return false;
        }
        if (!client.Call({{"cmd", "send_key"}, {"target", "@active_modal"}, {"key", "Return"}}).value("ok", false))
        {
            ADD_FAILURE() << "no active modal for change map size dialog";
            return false;
        }
        return client.WaitForResponse(clickId, 5000).value("ok", false);
    }

    int CountKind(const nlohmann::json& items, const std::string& kind)
    {
        return static_cast<int>(std::count_if(items.begin(), items.end(), [&](const nlohmann::json& item)
                                               { return item.value("kind", std::string()) == kind; }));
    }

    // Finds the selected item of a given kind - for telling two same-kind items apart (e.g. a
    // pasted copy, which PasteItemsCommand leaves selected, from the original it was copied
    // from). Fails the test if none match.
    nlohmann::json FindSelectedKind(const nlohmann::json& items, const std::string& kind)
    {
        for (const auto& item : items)
        {
            if (item.value("kind", std::string()) == kind && item.value("selected", false))
            {
                return item;
            }
        }
        ADD_FAILURE() << "no selected '" << kind << "' item in scene: " << items.dump();
        return nlohmann::json::object();
    }
}

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

// Regression test for dragging an already-multi-selected group (a collision line + a map
// object, selected together via a rubber-band drag): only the one item actually grabbed used
// to get clamped to the map bounds (to its own bounds) - Qt's default QGraphicsItem::
// mouseMoveEvent moves every other selected item by the raw delta with no clamping at all, so
// the rest of the selection could end up outside the map, or (if clamped independently by the
// old per-item logic) with its shape/relative layout distorted. EditorGraphicsScene now
// clamps the union of the whole selection as a rigid group after the drag completes.
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

// Regression test for two camera bugs: CameraManager::CreateCamera used to save a brand new
// camera's image under the wrong (still-empty) name, orphaning it from the camera being
// created; and Model::ToJson() used mId != 0 as a "does this camera exist" check, which
// silently dropped a legitimately-new camera whose id happens to be 0 (the very first camera
// in a path whose path id is also 0) from the saved json entirely. Also covers the FG1
// "<camName>.json" layers sidecar, which nothing ever wrote before (CameraGraphicsItem::Load
// only ever reads it) - saves a small sample image generated here rather than depending on a
// checked-in test asset.
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

    editor.terminate();
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

    editor.terminate();
    ASSERT_TRUE(editor.waitForFinished(5000)) << "editor did not exit after terminate()";
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
