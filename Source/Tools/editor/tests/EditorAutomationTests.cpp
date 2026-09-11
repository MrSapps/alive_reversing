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
#include <QDateTime>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QLocalSocket>
#include <QPoint>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTemporaryDir>
#include <QThread>

#include <algorithm>

#include <map>

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
}

TEST(EditorAutomation, OpenCloseAboutAndExit)
{
    QProcess editor;
    AutomationClient client;
    QString socketName;
    ASSERT_TRUE(LaunchEditorAndConnect(editor, client, socketName));

    {
        const int id = client.SendCommand({{"cmd", "ping"}});
        ASSERT_TRUE(client.WaitForResponse(id, 5000).value("ok", false));
    }

    {
        const int id = client.SendCommand({{"cmd", "list_actions"}});
        const auto resp = client.WaitForResponse(id, 5000);
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
        const int id = client.SendCommand({{"cmd", "get_state"}, {"target", "AboutDialog"}});
        const auto resp = client.WaitForResponse(id, 5000);
        ASSERT_TRUE(resp.value("ok", false)) << "AboutDialog did not open: " << resp.value("error", std::string());
    }

    {
        const int id = client.SendCommand({{"cmd", "close"}, {"target", "AboutDialog"}});
        EXPECT_TRUE(client.WaitForResponse(id, 5000).value("ok", false));
    }

    EXPECT_TRUE(client.WaitForResponse(clickAboutId, 5000).value("ok", false));

    {
        const int id = client.SendCommand({{"cmd", "get_state"}});
        const auto resp = client.WaitForResponse(id, 5000);
        ASSERT_TRUE(resp.value("ok", false));
        for (const auto& child : resp.at("result").value("children", nlohmann::json::array()))
        {
            EXPECT_NE(child.value("objectName", std::string()), "AboutDialog") << "AboutDialog still present after close";
        }
    }

    {
        const int id = client.SendCommand({{"cmd", "close"}});
        EXPECT_TRUE(client.WaitForResponse(id, 5000).value("ok", false));
    }

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
        const int id = client.SendCommand({{"cmd", "list_actions"}});
        const auto resp = client.WaitForResponse(id, 5000);
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

    {
        const int id = client.SendCommand({{"cmd", "click"}, {"target", "action_exit_application"}});
        EXPECT_TRUE(client.WaitForResponse(id, 5000).value("ok", false));
    }

    ASSERT_TRUE(editor.waitForFinished(10000)) << "editor did not exit after clicking Exit";
    EXPECT_EQ(editor.exitStatus(), QProcess::NormalExit);
    EXPECT_EQ(editor.exitCode(), 0);
}

// Exercises the core level-editing workflow: create a new path, add a collision line, add a
// map object. "New path" prompts via two sequential QInputDialog::getInt/getItem static
// dialogs, which (unlike AboutDialog) have no objectName at all, so they're addressed via the
// "@active_modal" target instead and accepted with their defaults. Success is verified through
// the undo history (QUndoView "undoView"), since the added collision line and object live in
// the tab's QGraphicsScene, which isn't part of the QWidget/QAction tree get_state can see.
TEST(EditorAutomation, NewPathAddCollisionAddObject)
{
    QProcess editor;
    AutomationClient client;
    QString socketName;
    ASSERT_TRUE(LaunchEditorAndConnect(editor, client, socketName));

    const int newPathClickId = client.SendCommand({{"cmd", "click"}, {"target", "actionNew_path"}});

    // Accept the "path id" dialog (default 0), then the "game" dialog (default AO) - both
    // still nested inside the still-outstanding click above.
    {
        const int id = client.SendCommand({{"cmd", "send_key"}, {"target", "@active_modal"}, {"key", "Return"}});
        ASSERT_TRUE(client.WaitForResponse(id, 5000).value("ok", false)) << "no active modal for path id dialog";
    }
    {
        const int id = client.SendCommand({{"cmd", "send_key"}, {"target", "@active_modal"}, {"key", "Return"}});
        ASSERT_TRUE(client.WaitForResponse(id, 5000).value("ok", false)) << "no active modal for game dialog";
    }
    EXPECT_TRUE(client.WaitForResponse(newPathClickId, 5000).value("ok", false));

    {
        const int id = client.SendCommand({{"cmd", "get_state"}, {"target", "tabWidget"}});
        const auto resp = client.WaitForResponse(id, 5000);
        ASSERT_TRUE(resp.value("ok", false));
        EXPECT_TRUE(ContainsClassName(resp.at("result"), "EditorTab")) << "new path tab not found under tabWidget";
    }

    // Add collision: no dialog, pushes straight onto the undo stack.
    {
        const int id = client.SendCommand({{"cmd", "click"}, {"target", "actionAdd_collision"}});
        EXPECT_TRUE(client.WaitForResponse(id, 5000).value("ok", false));
    }
    {
        const int id = client.SendCommand({{"cmd", "get_state"}, {"target", "undoView"}});
        const auto resp = client.WaitForResponse(id, 5000);
        ASSERT_TRUE(resp.value("ok", false));
        const auto items = resp.at("result").value("items", nlohmann::json::array());
        EXPECT_NE(std::find(items.begin(), items.end(), "Add collision line"), items.end())
            << "undo history does not show the added collision line: " << resp.at("result").dump();
    }

    // Add object: opens a modal dialog (AddObjectDialog) with a search box and a list of
    // object types - only added to the model if an item is actually selected before accepting.
    const int addObjectClickId = client.SendCommand({{"cmd", "click"}, {"target", "actionAdd_object"}});
    {
        const int id = client.SendCommand({{"cmd", "set_value"}, {"target", "lstObjects"}, {"value", 0}});
        ASSERT_TRUE(client.WaitForResponse(id, 5000).value("ok", false)) << "failed to select an object type";
    }
    {
        const int id = client.SendCommand({{"cmd", "send_key"}, {"target", "@active_modal"}, {"key", "Return"}});
        ASSERT_TRUE(client.WaitForResponse(id, 5000).value("ok", false)) << "no active modal for AddObjectDialog";
    }
    EXPECT_TRUE(client.WaitForResponse(addObjectClickId, 5000).value("ok", false));

    {
        const int id = client.SendCommand({{"cmd", "get_state"}, {"target", "undoView"}});
        const auto resp = client.WaitForResponse(id, 5000);
        ASSERT_TRUE(resp.value("ok", false));

        bool foundAddObject = false;
        for (const auto& item : resp.at("result").value("items", nlohmann::json::array()))
        {
            if (item.get<std::string>().rfind("Add new object ", 0) == 0)
            {
                foundAddObject = true;
                break;
            }
        }
        EXPECT_TRUE(foundAddObject) << "undo history does not show the added object: " << resp.at("result").dump();
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
    {
        const int id = client.SendCommand({{"cmd", "click"}, {"target", "@active_modal_button:Discard"}});
        ASSERT_TRUE(client.WaitForResponse(id, 5000).value("ok", false)) << "no unsaved-changes prompt to dismiss";
    }
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

    const int newPathClickId = client.SendCommand({{"cmd", "click"}, {"target", "actionNew_path"}});
    {
        const int id = client.SendCommand({{"cmd", "send_key"}, {"target", "@active_modal"}, {"key", "Return"}});
        ASSERT_TRUE(client.WaitForResponse(id, 5000).value("ok", false));
    }
    {
        const int id = client.SendCommand({{"cmd", "send_key"}, {"target", "@active_modal"}, {"key", "Return"}});
        ASSERT_TRUE(client.WaitForResponse(id, 5000).value("ok", false));
    }
    EXPECT_TRUE(client.WaitForResponse(newPathClickId, 5000).value("ok", false));

    {
        const int id = client.SendCommand({{"cmd", "click"}, {"target", "actionAdd_collision"}});
        EXPECT_TRUE(client.WaitForResponse(id, 5000).value("ok", false));
    }

    auto getCollisionLine = [&]() -> nlohmann::json
    {
        const int id = client.SendCommand({{"cmd", "get_scene_items"}});
        const auto resp = client.WaitForResponse(id, 5000);
        EXPECT_TRUE(resp.value("ok", false));
        for (const auto& item : resp.at("result").value("items", nlohmann::json::array()))
        {
            if (item.value("kind", std::string()) == "collision_line")
            {
                return item;
            }
        }
        ADD_FAILURE() << "no collision_line in get_scene_items result: " << resp.dump();
        return nlohmann::json::object();
    };

    auto sceneToView = [&](double x, double y) -> QPoint
    {
        const int id = client.SendCommand({{"cmd", "scene_to_view"}, {"x", x}, {"y", y}});
        const auto resp = client.WaitForResponse(id, 5000);
        EXPECT_TRUE(resp.value("ok", false));
        return QPoint(resp.at("result").value("x", 0), resp.at("result").value("y", 0));
    };

    auto dragView = [&](QPoint from, QPoint to)
    {
        const int id = client.SendCommand({
            {"cmd", "drag"}, {"target", "graphicsView"},
            {"from", {{"x", from.x()}, {"y", from.y()}}},
            {"to", {{"x", to.x()}, {"y", to.y()}}},
        });
        EXPECT_TRUE(client.WaitForResponse(id, 5000).value("ok", false));
    };

    const nlohmann::json baseline = getCollisionLine();
    ASSERT_TRUE(baseline.contains("x1"));
    const int baseX1 = baseline.at("x1").get<int>();
    const int baseY1 = baseline.at("y1").get<int>();
    const int baseX2 = baseline.at("x2").get<int>();
    const int baseY2 = baseline.at("y2").get<int>();
    EXPECT_NE(baseX1, baseX2) << "freshly created collision line is zero-length";

    // 1. Endpoint drag, in bounds: P2 should move to exactly the target; P1 stays put.
    {
        const QPoint from = sceneToView(baseX2, baseY2);
        const int targetX = baseX2 + 50;
        const int targetY = baseY2 + 30;
        dragView(from, sceneToView(targetX, targetY));

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
        const QPoint from = sceneToView(before.at("x2").get<int>(), before.at("y2").get<int>());
        dragView(from, sceneToView(1'000'000, 1'000'000));

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
        dragView(sceneToView(midX, midY), sceneToView(targetMidX, targetMidY));

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
        dragView(sceneToView(midX, midY), sceneToView(-1'000'000, -1'000'000));

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
        const int id1 = client.SendCommand({{"cmd", "click"}, {"target", "action_snap_collision_items_on_x"}});
        EXPECT_TRUE(client.WaitForResponse(id1, 5000).value("ok", false));
        const int id2 = client.SendCommand({{"cmd", "click"}, {"target", "action_snap_collision_objects_on_y"}});
        EXPECT_TRUE(client.WaitForResponse(id2, 5000).value("ok", false));

        const nlohmann::json before = getCollisionLine();
        const QPoint from = sceneToView(before.at("x2").get<int>(), before.at("y2").get<int>());
        // Deliberately not a multiple of 20 (or of the AO grid).
        dragView(from, sceneToView(before.at("x2").get<int>() + 47, before.at("y2").get<int>() + 47));

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

    const int newPathClickId = client.SendCommand({{"cmd", "click"}, {"target", "actionNew_path"}});
    {
        const int id = client.SendCommand({{"cmd", "send_key"}, {"target", "@active_modal"}, {"key", "Return"}});
        ASSERT_TRUE(client.WaitForResponse(id, 5000).value("ok", false));
    }
    {
        const int id = client.SendCommand({{"cmd", "send_key"}, {"target", "@active_modal"}, {"key", "Return"}});
        ASSERT_TRUE(client.WaitForResponse(id, 5000).value("ok", false));
    }
    EXPECT_TRUE(client.WaitForResponse(newPathClickId, 5000).value("ok", false));

    // Add a map object: opens AddObjectDialog, select the first entry in the (unfiltered) list
    // and accept - same sequence as NewPathAddCollisionAddObject.
    const int addObjectClickId = client.SendCommand({{"cmd", "click"}, {"target", "actionAdd_object"}});
    {
        const int id = client.SendCommand({{"cmd", "set_value"}, {"target", "lstObjects"}, {"value", 0}});
        ASSERT_TRUE(client.WaitForResponse(id, 5000).value("ok", false));
    }
    {
        const int id = client.SendCommand({{"cmd", "send_key"}, {"target", "@active_modal"}, {"key", "Return"}});
        ASSERT_TRUE(client.WaitForResponse(id, 5000).value("ok", false));
    }
    EXPECT_TRUE(client.WaitForResponse(addObjectClickId, 5000).value("ok", false));

    auto getMapObject = [&]() -> nlohmann::json
    {
        const int id = client.SendCommand({{"cmd", "get_scene_items"}});
        const auto resp = client.WaitForResponse(id, 5000);
        EXPECT_TRUE(resp.value("ok", false));
        for (const auto& item : resp.at("result").value("items", nlohmann::json::array()))
        {
            if (item.value("kind", std::string()) == "map_object")
            {
                return item;
            }
        }
        ADD_FAILURE() << "no map_object in get_scene_items result: " << resp.dump();
        return nlohmann::json::object();
    };

    auto sceneToView = [&](double x, double y) -> QPoint
    {
        const int id = client.SendCommand({{"cmd", "scene_to_view"}, {"x", x}, {"y", y}});
        const auto resp = client.WaitForResponse(id, 5000);
        EXPECT_TRUE(resp.value("ok", false));
        return QPoint(resp.at("result").value("x", 0), resp.at("result").value("y", 0));
    };

    auto dragView = [&](QPoint from, QPoint to)
    {
        const int id = client.SendCommand({
            {"cmd", "drag"}, {"target", "graphicsView"},
            {"from", {{"x", from.x()}, {"y", from.y()}}},
            {"to", {{"x", to.x()}, {"y", to.y()}}},
        });
        EXPECT_TRUE(client.WaitForResponse(id, 5000).value("ok", false));
    };

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
        dragView(sceneToView(midX, midY), sceneToView(midX + 30, midY + 20));

        const nlohmann::json rect = getMapObject();
        EXPECT_DOUBLE_EQ(rect.value("xpos", -1.0), baseX + 30) << "body drag did not move the box to the right place";
        EXPECT_DOUBLE_EQ(rect.value("ypos", -1.0), baseY + 20);
        EXPECT_DOUBLE_EQ(rect.value("width", -1.0), baseW) << "body drag changed the box size";
        EXPECT_DOUBLE_EQ(rect.value("height", -1.0), baseH);
    }

    // 2. Snapping: enable map-object grid snap and move by a deliberately non-grid-aligned
    // delta; same EditorTab::SnapY formula as the collision-line test, checked the same way.
    {
        const int id1 = client.SendCommand({{"cmd", "click"}, {"target", "action_snap_map_objects_x"}});
        EXPECT_TRUE(client.WaitForResponse(id1, 5000).value("ok", false));
        const int id2 = client.SendCommand({{"cmd", "click"}, {"target", "action_snap_map_objects_y"}});
        EXPECT_TRUE(client.WaitForResponse(id2, 5000).value("ok", false));

        const nlohmann::json before = getMapObject();
        const double x = before.at("xpos").get<double>();
        const double y = before.at("ypos").get<double>();
        const double w = before.at("width").get<double>();
        const double h = before.at("height").get<double>();
        const double midX = x + w / 2;
        const double midY = y + h / 2;
        // Deliberately not a multiple of 20.
        dragView(sceneToView(midX, midY), sceneToView(midX + 47, midY + 47));

        const nlohmann::json rect = getMapObject();
        const int ypos = static_cast<int>(rect.value("ypos", -1.0));
        EXPECT_EQ(ypos % 20, 0) << "box did not snap to the Y grid: ypos=" << ypos;

        const int id3 = client.SendCommand({{"cmd", "click"}, {"target", "action_snap_map_objects_x"}});
        EXPECT_TRUE(client.WaitForResponse(id3, 5000).value("ok", false));
        const int id4 = client.SendCommand({{"cmd", "click"}, {"target", "action_snap_map_objects_y"}});
        EXPECT_TRUE(client.WaitForResponse(id4, 5000).value("ok", false));
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
        dragView(sceneToView(x + w, y + h), sceneToView(x + w + 40, y + h + 25));

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
        dragView(sceneToView(x + w, y + h), sceneToView(1'000'000, 1'000'000));

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
        dragView(sceneToView(x + w, y + h), sceneToView(x - 500, y - 500));

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
        dragView(sceneToView(before.at("xpos").get<double>(), before.at("ypos").get<double>()), sceneToView(-1'000'000, -1'000'000));

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
        dragView(sceneToView(x + w / 2, y + h / 2), sceneToView(1'000'000, 1'000'000));

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

    const int newPathClickId = client.SendCommand({{"cmd", "click"}, {"target", "actionNew_path"}});
    {
        const int id = client.SendCommand({{"cmd", "send_key"}, {"target", "@active_modal"}, {"key", "Return"}});
        ASSERT_TRUE(client.WaitForResponse(id, 5000).value("ok", false));
    }
    {
        const int id = client.SendCommand({{"cmd", "send_key"}, {"target", "@active_modal"}, {"key", "Return"}});
        ASSERT_TRUE(client.WaitForResponse(id, 5000).value("ok", false));
    }
    EXPECT_TRUE(client.WaitForResponse(newPathClickId, 5000).value("ok", false));

    {
        const int id = client.SendCommand({{"cmd", "click"}, {"target", "actionAdd_collision"}});
        EXPECT_TRUE(client.WaitForResponse(id, 5000).value("ok", false));
    }

    const int addObjectClickId = client.SendCommand({{"cmd", "click"}, {"target", "actionAdd_object"}});
    {
        const int id = client.SendCommand({{"cmd", "set_value"}, {"target", "lstObjects"}, {"value", 0}});
        ASSERT_TRUE(client.WaitForResponse(id, 5000).value("ok", false));
    }
    {
        const int id = client.SendCommand({{"cmd", "send_key"}, {"target", "@active_modal"}, {"key", "Return"}});
        ASSERT_TRUE(client.WaitForResponse(id, 5000).value("ok", false));
    }
    EXPECT_TRUE(client.WaitForResponse(addObjectClickId, 5000).value("ok", false));

    auto getSceneItems = [&]() -> nlohmann::json
    {
        const int id = client.SendCommand({{"cmd", "get_scene_items"}});
        const auto resp = client.WaitForResponse(id, 5000);
        EXPECT_TRUE(resp.value("ok", false));
        return resp.at("result").value("items", nlohmann::json::array());
    };

    auto findKind = [](const nlohmann::json& items, const std::string& kind) -> nlohmann::json
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
    };

    auto sceneToView = [&](double x, double y) -> QPoint
    {
        const int id = client.SendCommand({{"cmd", "scene_to_view"}, {"x", x}, {"y", y}});
        const auto resp = client.WaitForResponse(id, 5000);
        EXPECT_TRUE(resp.value("ok", false));
        return QPoint(resp.at("result").value("x", 0), resp.at("result").value("y", 0));
    };

    auto dragView = [&](QPoint from, QPoint to)
    {
        const int id = client.SendCommand({
            {"cmd", "drag"}, {"target", "graphicsView"},
            {"from", {{"x", from.x()}, {"y", from.y()}}},
            {"to", {{"x", to.x()}, {"y", to.y()}}},
        });
        EXPECT_TRUE(client.WaitForResponse(id, 5000).value("ok", false));
    };

    // Move both items to non-default positions, so this actually verifies arbitrary state
    // round-trips through JSON rather than just the fixed creation position.
    {
        const nlohmann::json line = findKind(getSceneItems(), "collision_line");
        dragView(sceneToView(line.at("x2").get<int>(), line.at("y2").get<int>()),
                 sceneToView(line.at("x2").get<int>() + 60, line.at("y2").get<int>() + 35));
    }
    {
        const nlohmann::json rect = findKind(getSceneItems(), "map_object");
        const double midX = rect.at("xpos").get<double>() + rect.at("width").get<double>() / 2;
        const double midY = rect.at("ypos").get<double>() + rect.at("height").get<double>() / 2;
        dragView(sceneToView(midX, midY), sceneToView(midX + 45, midY - 25));
    }

    const nlohmann::json itemsBeforeSave = getSceneItems();
    const nlohmann::json lineBeforeSave = findKind(itemsBeforeSave, "collision_line");
    const nlohmann::json rectBeforeSave = findKind(itemsBeforeSave, "map_object");

    {
        const int id = client.SendCommand({{"cmd", "save_path_as"}, {"path", savePath.toStdString()}});
        const auto resp = client.WaitForResponse(id, 5000);
        ASSERT_TRUE(resp.value("ok", false));
        ASSERT_TRUE(resp.at("result").value("saved", false)) << "save_path_as reported failure";
    }

    ASSERT_TRUE(QFile::exists(savePath)) << "save_path_as did not create a file at " << savePath.toStdString();
    EXPECT_GT(QFileInfo(savePath).size(), 0) << "saved file is empty";

    {
        const int id = client.SendCommand({{"cmd", "open_path"}, {"path", savePath.toStdString()}});
        const auto resp = client.WaitForResponse(id, 5000);
        ASSERT_TRUE(resp.value("ok", false));
        ASSERT_TRUE(resp.at("result").value("opened", false)) << "open_path reported failure";
    }

    // open_path makes the reopened file the active tab, so get_scene_items now reads it back.
    const nlohmann::json itemsAfterReopen = getSceneItems();
    const nlohmann::json lineAfterReopen = findKind(itemsAfterReopen, "collision_line");
    const nlohmann::json rectAfterReopen = findKind(itemsAfterReopen, "map_object");

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

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
