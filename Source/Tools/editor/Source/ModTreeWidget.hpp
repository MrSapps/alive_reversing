#pragma once

#include <QTreeWidget>
#include <QMap>
#include <QMetaObject>

class EditorMod;
class EditorMainWindow;
class EditorTab;
class LevelTreeItem;
class PathTreeItem;
class ModTreeItem;

// Left-docked tree (see EditorMainWindow) showing the currently open mod's levels and paths,
// and (once a path is open) that path's cameras/collision lines/map objects. Right-click to
// create a new level/path, double-click a level to expand it or a path to open it. Kept in sync
// with add/delete/undo/redo inside an open path via QUndoStack::indexChanged (see
// NotifyTabOpened) rather than by hand-wiring every individual command, so any future command
// that mutates the model is covered automatically. A pure selection change (the common case -
// clicking around the scene) is special-cased to a cheap re-highlight (SyncTreeSelectionFromScene)
// instead of a full rebuild, so the tree doesn't lose its expansion state on every click - see
// NotifyTabOpened. Selection is synced both ways: the scene's selection drives the tree's own
// (see RefreshPathSubtree/SyncTreeSelectionFromScene), and selecting a camera/map-object/
// collision-line row in the tree selects the matching item(s) in the scene (onItemSelectionChanged).
class ModTreeWidget final : public QTreeWidget
{
    Q_OBJECT
public:
    ModTreeWidget(QWidget* pParent, EditorMainWindow* pMainWindow);

    // Full rebuild from pMod (nullptr clears the tree). Called whenever the open mod changes.
    void SetMod(EditorMod* pMod);

    // Keeps a PathTreeItem's "currently open in this tab" state accurate and wires up live
    // sync for its Cameras/Collisions subtree - called from EditorMainWindow::AddModelTab/
    // onCloseTab for every tab open/close (not just ones that originated from this tree). A
    // no-op if jsonFileName doesn't belong to the currently open mod.
    void NotifyTabOpened(EditorTab* pTab, const QString& jsonFileName);
    void NotifyTabClosed(EditorTab* pTab);

private slots:
    void onItemDoubleClicked(QTreeWidgetItem* pItem, int column);
    void onItemExpanded(QTreeWidgetItem* pItem);
    void onCustomContextMenuRequested(const QPoint& pos);
    void onItemSelectionChanged();

private:
    void RefreshPathSubtree(PathTreeItem* pPathItem);
    void SyncTreeSelectionFromScene(PathTreeItem* pPathItem);
    void PromptNewLevel();
    void PromptNewPath(LevelTreeItem* pLevelItem);
    PathTreeItem* FindPathItem(const QString& jsonFileName) const;
    PathTreeItem* FindPathItemByTab(EditorTab* pTab) const;

    EditorMainWindow* mMainWindow = nullptr;
    EditorMod* mMod = nullptr;
    QMap<EditorTab*, QMetaObject::Connection> mUndoStackConnections;
};
