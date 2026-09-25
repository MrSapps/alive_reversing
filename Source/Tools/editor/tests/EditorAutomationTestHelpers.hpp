#pragma once

// Shared helpers for the EditorAutomation*Tests.cpp files - defined in EditorAutomationTestHelpers.cpp.

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

namespace AutomationTest {

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
    bool LaunchEditorAndConnect(QProcess& editor, AutomationClient& client, QString& outSocketName);

    // Recursively searches a get_state tree (see AutomationCommands.cpp's DescribeObject) for
    // a node with the given className.
    bool ContainsClassName(const nlohmann::json& node, const std::string& className);

    // Same idea as ContainsClassName, but counts every match rather than stopping at the first -
    // e.g. counting how many EditorTab widgets currently exist under tabWidget, to check a
    // double-click didn't open a duplicate tab for an already-open path.
    int CountClassName(const nlohmann::json& node, const std::string& className);

    // Creates a new path via actionNew_path, accepting the "path id"/"game" QInputDialogs with
    // their defaults (0, AO) via the "@active_modal" target - they have no objectName of their
    // own, unlike AboutDialog - then setting its "width"/"height" (in cameras) QInputDialogs to
    // xSize/ySize via "@active_modal_input" (same idiom CreateNewMod's txtName/txtAuthor use,
    // one level further in since these dialogs have no named widget to target directly).
    // Defaults to 1x1 (Model::CreateAsNewPath's own default is 4x4 - see its header comment) so
    // every existing caller of this helper, written back when 1x1 was the *only* size a new path
    // could ever be, keeps testing exactly the single-camera map it already assumes without
    // having to pass a size explicitly; a test that actually wants a bigger grid passes one.
    bool CreateNewPath(AutomationClient& client, int xSize = 1, int ySize = 1);

    // Adds a map object via AddObjectDialog: select the first entry in the (unfiltered) type
    // list and accept.
    bool AddMapObject(AutomationClient& client);

    // Returns get_scene_items' "items" array (collision lines, map objects, cameras, ...).
    nlohmann::json GetSceneItems(AutomationClient& client);

    // Finds the first item of a given "kind" (e.g. "collision_line", "map_object") in a
    // get_scene_items result. Fails the test if none match.
    nlohmann::json FindKind(const nlohmann::json& items, const std::string& kind);

    // Finds a collision_line item by its model id (CollisionObject::mId) - for telling two
    // same-kind items apart once there's more than one in the scene.
    nlohmann::json FindCollisionById(const nlohmann::json& items, int id);

    // The undo stack's list of command description strings, in oldest-to-newest order, as
    // shown in the QUndoView ("undoView") - e.g. "Add collision line", "Move 3 item(s)". Works
    // because AutomationCommands.cpp special-cases any QAbstractItemView (QUndoView included)
    // and dumps column-0 display text per row into the "items" field.
    std::vector<std::string> GetUndoWidgetTextList(AutomationClient& client);

    bool UndoContains(const std::vector<std::string>& items, const std::string& text);

    // True if any entry starts with the given prefix - for descriptions that embed a
    // generated/variable suffix, e.g. "Add new object <TypeName>".
    bool UndoContainsPrefix(const std::vector<std::string>& items, const std::string& prefix);

    QPoint SceneToView(AutomationClient& client, double x, double y);

    void DragView(AutomationClient& client, QPoint from, QPoint to);

    // Sends a single press/move/release event to graphicsView - unlike DragView (which does all
    // three as one atomic "drag" call), these let a test inspect state *between* them (e.g. a
    // get_scene_items call after MouseMove but before MouseUp, to check something is clamped
    // live during a drag rather than only once released).
    bool SendMouseEvent(AutomationClient& client, const char* phase, QPoint pos);

    // Adds a collision line via the click-to-place tool (actionAdd_collision starts placement;
    // it no longer creates a line immediately - see AddCollisionClickToPlaceShowsGhostAndCreatesLine
    // for that flow in detail). Always clicks the same fixed pair of scene points - a 100px
    // horizontal line, matching the tool's own placement-independent shape - so tests that add
    // more than one line still get predictable (and, called twice in a row, coincident)
    // positions to work with, same as the old fixed-offset default did.
    bool AddCollisionLine(AutomationClient& client);

    // Opens ChangeMapSizeDialog (actionEdit_map_size), sets both spinboxes and accepts via
    // Return - same "@active_modal" idiom as CreateNewPath/AddMapObject's dialogs.
    bool SetMapSize(AutomationClient& client, int x, int y);

    int CountKind(const nlohmann::json& items, const std::string& kind);

    // Finds the selected item of a given kind - for telling two same-kind items apart (e.g. a
    // pasted copy, which PasteItemsCommand leaves selected, from the original it was copied
    // from). Fails the test if none match.
    nlohmann::json FindSelectedKind(const nlohmann::json& items, const std::string& kind);

    // Deletes the camera at a scene position by driving the real UI path - right-click (context
    // menu) -> "Edit camera" -> CameraManager's "Delete camera" button -> close the dialog -
    // rather than a domain-specific automation-only bypass. Both the context menu (QMenu::exec())
    // and the CameraManager dialog (QDialog::exec()) block in their own nested event loop until
    // dismissed, same as any other modal here, so each step is fired via SendCommand (not Call)
    // and its response only collected once the step that unblocks it has happened - same idiom
    // CreateNewPath uses for its two chained QInputDialogs, just one level deeper. Triggering
    // "actionEditCamera" directly (rather than through the menu's own click handling) doesn't
    // tell the menu itself to close, so that's dismissed explicitly with Escape at the end.
    bool DeleteCameraViaContextMenu(AutomationClient& client, QPoint viewPos);

    // Reads the text currently displayed by a BigSpinBox (a property panel row's editor
    // widget) via its internal QLineEdit child ("qt_spinbox_lineedit") - the only place that
    // text is exposed through get_state, since DescribeObject only reports a "text" field for
    // QLineEdit/QLabel/QAbstractButton, not QAbstractSpinBox itself.
    std::string SpinBoxDisplayedText(AutomationClient& client, const std::string& objectName);

    // All current map_object scene items, sorted left-to-right by xpos - for telling a row of
    // otherwise-identical objects apart after a drag that's expected to preserve their order.
    std::vector<nlohmann::json> SortedMapObjectsByX(AutomationClient& client);

    // Adds 3 identical-size map objects and repositions each into a horizontal row (same rowY,
    // spaced by spacing in X) via individual single-item body drags right after each is added -
    // AddNewObjectCommand always places a new object at the current viewport's center, so
    // without repositioning immediately (while it's still uniquely identifiable as "the selected
    // one", per AddNewObjectCommand::redo()) all 3 would land exactly on top of each other and
    // become indistinguishable. Drags a point 10px inside each object's body (never a resize
    // handle) so the resulting move is exactly (targetX - curX, rowY - curY) regardless of the
    // object's own width/height. Returns the 3 objects sorted by xpos, or empty on failure (with
    // a recorded test failure).
    std::vector<nlohmann::json> AddThreeAlignedMapObjects(AutomationClient& client, double rowY, double spacing);

    // Drives NewModDialog (File > Mod > New Mod...) end to end: fills name/author/directory
    // (bypassing the "Browse..." native folder picker - txtDirectory is read-only in the UI, but
    // "set_value" writes to it directly via QLineEdit::setText, same as it does for any other
    // read-only field elsewhere in these tests) and accepts. Leaves gameIndex (0=AO, 1=AE)
    // untouched if not overridden, matching cmbGame's own default.
    bool CreateNewMod(AutomationClient& client, const QString& dir, const QString& name, const QString& author, int gameIndex = 0);

    // Creates a mod, a "MI" level, and path 0 inside it via the tree's own right-click New
    // Level/New Path actions (not a shortcut around them) - shared setup for tests that need an
    // already-open path inside a mod without re-verifying the creation flow itself (see
    // NewLevelAndNewPathViaTreeContextMenuCreatesFilesAndOpensPath for that).
    // Triggering a context menu's action programmatically (via "click" on its QAction, rather
    // than a real mouse click landing inside the still-open QMenu) doesn't tell the menu itself
    // to close - same gotcha DeleteCameraViaContextMenu's own comment documents for
    // "graphicsViewContextMenu". Dismiss it explicitly with Escape (only if it's actually still
    // around - the action may have already closed it some other way) so the menu's own exec()
    // call unblocks and the original right-click command can finally return.
    bool DismissMenuIfStillOpen(AutomationClient& client, const char* menuObjectName);

    bool CreateModWithLevelAndOpenPath(AutomationClient& client, const QString& modDir);

} // namespace AutomationTest
