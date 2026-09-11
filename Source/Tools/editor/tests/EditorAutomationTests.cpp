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
#include <QLocalSocket>
#include <QPoint>
#include <QProcess>
#include <QProcessEnvironment>
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

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
