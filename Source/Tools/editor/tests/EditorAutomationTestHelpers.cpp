// End-to-end regression test for relive-editor's automation channel: launches the real
// editor binary with an explicit --automation-socket, drives it over that socket exactly
// the way an external tool would (AutomationCli, or me via Bash), and asserts it can open
// the About dialog, close it, and shut the app down cleanly.
//
// This intentionally talks the wire protocol directly via AutomationProtocol.hpp rather than
// shelling out to relive-editor-automation-cli, so a bug in the CLI can't mask a protocol bug.


#include "EditorAutomationTestHelpers.hpp"

namespace AutomationTest {

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
#ifdef _WIN32
        // Except on Windows: Qt 5.15's offscreen plugin has no native interface, and on Windows
        // QMessageBox::showEvent dereferences it (qt_getWindowsSystemMenu) and crashes the editor
        // with an access violation. Use the native platform there instead, clearing any offscreen
        // setting inherited from the environment.
        env.remove("QT_QPA_PLATFORM");
#else
        env.insert("QT_QPA_PLATFORM", "offscreen");
#endif
        // Qt on Windows sends a GUI app's log output to the debugger, not stderr, unless told to.
        env.insert("QT_FORCE_STDERR_LOGGING", "1");
        editor.setProcessEnvironment(env);
        // Show the editor's own output (including its relive.automation trace) in the test log.
        editor.setProcessChannelMode(QProcess::ForwardedChannels);

        editor.start(QString::fromUtf8(RELIVE_EDITOR_PATH), {QStringLiteral("--automation-socket=%1").arg(outSocketName)});
        if (!editor.waitForStarted(5000))
        {
            ADD_FAILURE() << "editor failed to start: " << editor.errorString().toStdString();
            return false;
        }

        client.SetEditorProcess(&editor);
        if (!client.ConnectWithRetry(outSocketName, 10000))
        {
            ADD_FAILURE() << "failed to connect to automation socket";
            return false;
        }

        return true;
    }

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

    int CountClassName(const nlohmann::json& node, const std::string& className)
    {
        int count = node.value("className", std::string()) == className ? 1 : 0;
        for (const auto& child : node.value("children", nlohmann::json::array()))
        {
            count += CountClassName(child, className);
        }
        return count;
    }

    bool CreateNewPath(AutomationClient& client, int xSize, int ySize)
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
        if (!client.Call({{"cmd", "set_value"}, {"target", "@active_modal_input"}, {"value", xSize}}).value("ok", false))
        {
            ADD_FAILURE() << "failed to set new path width";
            return false;
        }
        if (!client.Call({{"cmd", "send_key"}, {"target", "@active_modal"}, {"key", "Return"}}).value("ok", false))
        {
            ADD_FAILURE() << "no active modal for width dialog";
            return false;
        }
        if (!client.Call({{"cmd", "set_value"}, {"target", "@active_modal_input"}, {"value", ySize}}).value("ok", false))
        {
            ADD_FAILURE() << "failed to set new path height";
            return false;
        }
        if (!client.Call({{"cmd", "send_key"}, {"target", "@active_modal"}, {"key", "Return"}}).value("ok", false))
        {
            ADD_FAILURE() << "no active modal for height dialog";
            return false;
        }
        return client.WaitForResponse(newPathClickId, 5000).value("ok", false);
    }

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

    nlohmann::json GetSceneItems(AutomationClient& client)
    {
        const auto resp = client.Call({{"cmd", "get_scene_items"}});
        EXPECT_TRUE(resp.value("ok", false));
        return resp.at("result").value("items", nlohmann::json::array());
    }

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

    nlohmann::json FindCollisionById(const nlohmann::json& items, int id)
    {
        for (const auto& item : items)
        {
            if (item.value("kind", std::string()) == "collision_line" && item.value("id", -1) == id)
            {
                return item;
            }
        }
        ADD_FAILURE() << "no collision_line with id " << id << " in scene: " << items.dump();
        return nlohmann::json::object();
    }

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

    bool SendMouseEvent(AutomationClient& client, const char* phase, QPoint pos)
    {
        return client.Call({{"cmd", "mouse_event"}, {"target", "graphicsView"}, {"phase", phase}, {"x", pos.x()}, {"y", pos.y()}}).value("ok", false);
    }

    bool AddCollisionLine(AutomationClient& client)
    {
        if (!client.Call({{"cmd", "click"}, {"target", "actionAdd_collision"}}).value("ok", false))
        {
            return false;
        }

        const QPoint p1 = SceneToView(client, 100, 100);
        const QPoint p2 = SceneToView(client, 200, 100);
        if (!SendMouseEvent(client, "down", p1) || !SendMouseEvent(client, "up", p1))
        {
            return false;
        }
        return SendMouseEvent(client, "down", p2) && SendMouseEvent(client, "up", p2);
    }

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

    bool DeleteCameraViaContextMenu(AutomationClient& client, QPoint viewPos)
    {
        const int contextMenuId = client.SendCommand({{"cmd", "context_menu"}, {"target", "graphicsView"}, {"x", viewPos.x()}, {"y", viewPos.y()}});
        const int editCameraClickId = client.SendCommand({{"cmd", "click"}, {"target", "actionEditCamera"}});

        if (!client.Call({{"cmd", "get_state"}, {"target", "CameraManager"}}).value("ok", false))
        {
            ADD_FAILURE() << "CameraManager dialog did not open from the context menu";
            return false;
        }
        if (!client.Call({{"cmd", "click"}, {"target", "btnDeleteCamera"}}).value("ok", false))
        {
            ADD_FAILURE() << "failed to click btnDeleteCamera";
            return false;
        }
        if (!client.Call({{"cmd", "close"}, {"target", "CameraManager"}}).value("ok", false))
        {
            ADD_FAILURE() << "failed to close the CameraManager dialog";
            return false;
        }
        if (!client.WaitForResponse(editCameraClickId, 5000).value("ok", false))
        {
            ADD_FAILURE() << "click on actionEditCamera did not report success";
            return false;
        }

        // Showing CameraManager (an application-modal QDialog) already closes the still-open
        // QMenu as a side effect of it grabbing focus - so by this point the menu is normally
        // already gone. Dismiss it explicitly (Escape is the normal way to close a QMenu) only
        // if it's somehow still around.
        if (client.Call({{"cmd", "get_state"}, {"target", "graphicsViewContextMenu"}}).value("ok", false))
        {
            if (!client.Call({{"cmd", "send_key"}, {"target", "graphicsViewContextMenu"}, {"key", "Escape"}}).value("ok", false))
            {
                ADD_FAILURE() << "failed to dismiss the still-open context menu";
                return false;
            }
        }
        return client.WaitForResponse(contextMenuId, 5000).value("ok", false);
    }

    std::string SpinBoxDisplayedText(AutomationClient& client, const std::string& objectName)
    {
        const auto resp = client.Call({{"cmd", "get_state"}, {"target", objectName}});
        EXPECT_TRUE(resp.value("ok", false));
        for (const auto& child : resp.at("result").value("children", nlohmann::json::array()))
        {
            if (child.value("className", std::string()) == "QLineEdit")
            {
                return child.value("text", std::string());
            }
        }
        ADD_FAILURE() << "no QLineEdit child found under " << objectName;
        return {};
    }

    std::vector<nlohmann::json> SortedMapObjectsByX(AutomationClient& client)
    {
        std::vector<nlohmann::json> objs;
        for (const auto& item : GetSceneItems(client))
        {
            if (item.value("kind", std::string()) == "map_object")
            {
                objs.push_back(item);
            }
        }
        std::sort(objs.begin(), objs.end(), [](const nlohmann::json& a, const nlohmann::json& b)
                  { return a.at("xpos").get<double>() < b.at("xpos").get<double>(); });
        return objs;
    }

    std::vector<nlohmann::json> AddThreeAlignedMapObjects(AutomationClient& client, double rowY, double spacing)
    {
        for (int i = 0; i < 3; ++i)
        {
            if (!AddMapObject(client))
            {
                return {};
            }
            const nlohmann::json obj = FindSelectedKind(GetSceneItems(client), "map_object");
            if (obj.empty())
            {
                return {};
            }
            const double curX = obj.at("xpos").get<double>();
            const double curY = obj.at("ypos").get<double>();
            const double targetX = 100 + i * spacing;
            DragView(client, SceneToView(client, curX + 10, curY + 10), SceneToView(client, targetX + 10, rowY + 10));
        }
        return SortedMapObjectsByX(client);
    }

    bool CreateNewMod(AutomationClient& client, const QString& dir, const QString& name, const QString& author, int gameIndex)
    {
        const int clickId = client.SendCommand({{"cmd", "click"}, {"target", "actionNewMod"}});
        if (!client.Call({{"cmd", "set_value"}, {"target", "txtName"}, {"value", name.toStdString()}}).value("ok", false))
        {
            ADD_FAILURE() << "failed to set mod name";
            return false;
        }
        if (!client.Call({{"cmd", "set_value"}, {"target", "txtAuthor"}, {"value", author.toStdString()}}).value("ok", false))
        {
            ADD_FAILURE() << "failed to set mod author";
            return false;
        }
        if (gameIndex != 0 && !client.Call({{"cmd", "set_value"}, {"target", "cmbGame"}, {"value", gameIndex}}).value("ok", false))
        {
            ADD_FAILURE() << "failed to set mod target game";
            return false;
        }
        if (!client.Call({{"cmd", "set_value"}, {"target", "txtDirectory"}, {"value", dir.toStdString()}}).value("ok", false))
        {
            ADD_FAILURE() << "failed to set mod directory";
            return false;
        }
        if (!client.Call({{"cmd", "send_key"}, {"target", "@active_modal"}, {"key", "Return"}}).value("ok", false))
        {
            ADD_FAILURE() << "no active modal for NewModDialog";
            return false;
        }
        return client.WaitForResponse(clickId, 5000).value("ok", false);
    }

    bool DismissMenuIfStillOpen(AutomationClient& client, const char* menuObjectName)
    {
        if (client.Call({{"cmd", "get_state"}, {"target", menuObjectName}}).value("ok", false))
        {
            return client.Call({{"cmd", "send_key"}, {"target", menuObjectName}, {"key", "Escape"}}).value("ok", false);
        }
        return true;
    }

    bool CreateModWithLevelAndOpenPath(AutomationClient& client, const QString& modDir)
    {
        if (!CreateNewMod(client, modDir, "My Mod", "Tester"))
        {
            return false;
        }
        // A right-click opens ModTreeWidget's context menu via QMenu::exec() - a nested,
        // blocking event loop, same as EditorGraphicsView's own context menu (see
        // DeleteCameraViaContextMenu's comment above) - so it has to be fired via SendCommand
        // (not Call) and its response only collected via WaitForResponse once the action that
        // dismisses the menu (clicking one of its entries) has actually happened.
        const int rightClickModRootId = client.SendCommand({{"cmd", "click_tree_item"}, {"target", "modTreeWidget"}, {"column", 0}, {"row_text", "My Mod"}, {"right_click", true}});
        {
            const int clickId = client.SendCommand({{"cmd", "click"}, {"target", "actionNewLevel"}});
            if (!client.Call({{"cmd", "set_value"}, {"target", "@active_modal_input"}, {"value", "MI"}}).value("ok", false)
                || !client.Call({{"cmd", "send_key"}, {"target", "@active_modal"}, {"key", "Return"}}).value("ok", false)
                || !client.WaitForResponse(clickId, 5000).value("ok", false))
            {
                ADD_FAILURE() << "New Level flow failed";
                return false;
            }
        }
        if (!DismissMenuIfStillOpen(client, "modTreeContextMenu")
            || !client.WaitForResponse(rightClickModRootId, 5000).value("ok", false))
        {
            ADD_FAILURE() << "failed to right-click the mod root";
            return false;
        }

        const int rightClickLevelId = client.SendCommand({{"cmd", "click_tree_item"}, {"target", "modTreeWidget"}, {"column", 0}, {"row_text", "MI"}, {"right_click", true}});
        {
            const int clickId = client.SendCommand({{"cmd", "click"}, {"target", "actionNewPath"}});
            if (!client.Call({{"cmd", "send_key"}, {"target", "@active_modal"}, {"key", "Return"}}).value("ok", false)
                || !client.WaitForResponse(clickId, 5000).value("ok", false))
            {
                ADD_FAILURE() << "New Path flow failed";
                return false;
            }
        }
        if (!DismissMenuIfStillOpen(client, "modTreeContextMenu")
            || !client.WaitForResponse(rightClickLevelId, 5000).value("ok", false))
        {
            ADD_FAILURE() << "failed to right-click the new level";
            return false;
        }
        return true;
    }

} // namespace AutomationTest

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
