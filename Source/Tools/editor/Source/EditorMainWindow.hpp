#pragma once

#include <QMainWindow>
#include <QSettings>
#include <memory>
#include "EditorTab.hpp"
#include "ClipBoard.hpp"
#include "GridSnapSettings.hpp"
#include "../../relive_lib/GameType.hpp"

namespace Ui
{
    class EditorMainWindow;
}

class AutomationServer;
class EditorMod;
class ModTreeWidget;

class EditorMainWindow final : public QMainWindow
{
    Q_OBJECT
    
public:
    explicit EditorMainWindow( QWidget* aParent = 0 );
    ~EditorMainWindow();

    // Currently active tab, or nullptr if none is open. Used by AutomationSceneCommands to
    // reach the graphics scene without exposing m_ui itself.
    EditorTab* CurrentTab() const;

    // Switches focus to an already-open tab - e.g. double-clicking a camera row in ModTreeWidget
    // should bring that camera's path to the front the same way double-clicking the path row
    // itself does (OpenPath's dedup-by-filename already does this for that case; this is the
    // same operation for a caller that already has the EditorTab*, not just its filename).
    void MakeTabCurrent(EditorTab* pTab);

    // Public wrapper for onOpenPath() - used by AutomationSceneCommands so tests can open a
    // known path directly instead of driving the real QFileDialog-based Open flow.
    bool OpenPath(QString fullFileName)
    {
        return onOpenPath(fullFileName);
    }

    // Creates a new path at an explicit on-disk location and opens+saves it immediately (no
    // Save-As dialog) - used by ModTreeWidget's "New Path" context menu action, which already
    // knows exactly where the file should live (inside the mod's level folder). AddModelTab
    // (private) is the only thing that constructs an EditorTab, so this is the same kind of
    // public wrapper OpenPath() already is for onOpenPath().
    bool CreateAndOpenNewPath(QString jsonFileName, s32 pathId, GameType game, u32 xSize = 4, u32 ySize = 4);

private slots:
    void on_actionNewMod_triggered();
    void on_actionOpenMod_triggered();
    void on_actionDark_Fusion_theme_triggered();

    void on_actionDark_theme_triggered();

    void on_action_about_qt_triggered();

    void on_action_about_triggered();

    void onCloseTab(int index);

    void on_action_open_path_triggered();

    void on_action_zoom_reset_triggered();

    void on_action_zoom_in_triggered();

    void on_action_zoom_out_triggered();

    void on_action_undo_triggered();

    void on_action_redo_triggered();

    void on_action_delete_selected_items_triggered();

    void on_action_save_path_triggered();

    void on_actionSave_all_triggered();


    void on_tabWidget_currentChanged(int index);

    void on_actionEdit_HintFly_messages_triggered();

    void on_actionEdit_LCDScreen_messages_triggered();

    void on_actionEdit_path_data_triggered();

    void on_actionEdit_map_size_triggered();

    void on_actionNew_path_triggered();

    void on_actionSave_As_triggered();

    void on_actionAdd_object_triggered();

    void on_actionAdd_collision_triggered();

    void on_actionConnect_collisions_triggered();

    void on_action_close_path_triggered();

    void on_actionItem_transparency_triggered();

    void on_action_toggle_show_grid_triggered();

    void on_actionCut_triggered();

    void on_actionCopy_triggered();

    void on_actionPaste_triggered();

    void on_action_snap_collision_items_on_x_toggled(bool on);

    void on_action_snap_map_objects_x_toggled(bool on);

    void on_action_snap_collision_objects_on_y_toggled(bool on);

    void on_action_snap_map_objects_y_toggled(bool on);

private:
    void readSettings();
    void setMenuActionsEnabled(bool enable);
    bool onOpenPath(QString fileName);
    EditorTab* AddModelTab(std::unique_ptr<Model> model, QString fileName, bool isTempFile);
    void UpdateWindowTitle();
    void DisconnectTabSignals();
    void closeEvent(QCloseEvent* pEvent) override;

    // Closes every open tab (each honoring its own unsaved-changes prompt via onCloseTab).
    // Returns false (leaving whatever's still open alone) if the user cancels any of those
    // prompts, so a mod switch that triggers this can abort rather than leave the tree/tabs out
    // of sync with each other.
    bool CloseAllTabs();

    // Common path for New Mod/Open Mod: closes every open tab (see CloseAllTabs), swaps
    // mCurrentMod, repopulates the mod tree, updates the window title, and persists
    // "last_open_mod_dir" (same QSettings/relive-editor.ini idiom "last_open_dir"/"theme"/
    // "windowState" already use) so it can be auto-reopened on next launch. Returns false (mod
    // left unchanged) if CloseAllTabs was cancelled.
    bool SwitchToMod(std::unique_ptr<EditorMod> pMod);
private:
    Ui::EditorMainWindow* m_ui;
    QSettings m_Settings;
    ClipBoard mClipBoard;
    // Global to all tabs
    GridSnapSettings mSnapSettings;
    QString mUnthemedStyle;
    std::unique_ptr<AutomationServer> mAutomationServer;
    std::unique_ptr<EditorMod> mCurrentMod;
    ModTreeWidget* mModTree = nullptr;
};
