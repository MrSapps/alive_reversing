#include "ModTreeWidget.hpp"
#include "ModTreeItem.hpp"
#include "EditorMod.hpp"
#include "EditorMainWindow.hpp"
#include "EditorTab.hpp"
#include "Model.hpp"
#include "CameraManager.hpp"
#include "EditorGraphicsScene.hpp"
#include "ResizeableRectItem.hpp"
#include "ResizeableArrowItem.hpp"
#include "SetSelectionCommand.hpp"
#include <QTreeWidgetItemIterator>
#include <QMenu>
#include <QAction>
#include <QInputDialog>
#include <QMessageBox>
#include <QLineEdit>
#include <QSet>

namespace
{
    // Walks up from pItem to its containing PathTreeItem - e.g. a MapObjectTreeItem's parent
    // chain is Path -> "Cameras" group -> Camera -> MapObject - shared by every handler that
    // needs "which open path/tab does this row belong to" (the context menu's "Edit camera", and
    // tree<->scene selection sync below).
    PathTreeItem* FindAncestorPathItem(QTreeWidgetItem* pItem)
    {
        for (QTreeWidgetItem* ancestor = pItem; ancestor; ancestor = ancestor->parent())
        {
            if (auto* pPathItem = dynamic_cast<PathTreeItem*>(ancestor))
            {
                return pPathItem;
            }
        }
        return nullptr;
    }

    // Expands every ancestor of pItem up to (but not including) its PathTreeItem, so a row that
    // just became selected is actually visible rather than sitting collapsed under a "Cameras"/
    // camera/"Collisions" row nobody had a reason to open yet.
    void ExpandAncestors(QTreeWidgetItem* pItem)
    {
        for (QTreeWidgetItem* ancestor = pItem->parent(); ancestor && !dynamic_cast<PathTreeItem*>(ancestor); ancestor = ancestor->parent())
        {
            ancestor->setExpanded(true);
        }
    }
}

ModTreeWidget::ModTreeWidget(QWidget* pParent, EditorMainWindow* pMainWindow)
    : QTreeWidget(pParent), mMainWindow(pMainWindow)
{
    setObjectName("modTreeWidget");
    setHeaderHidden(true);
    setContextMenuPolicy(Qt::CustomContextMenu);
    // QTreeView's own built-in double-click handling (on by default) also toggles expansion,
    // fighting with onItemDoubleClicked's own explicit toggle below - both run off the same
    // event, so a double-click ends up net-toggling twice (i.e. doing nothing) instead of once.
    // This widget owns double-click semantics itself (expand a level, or open a path) instead.
    setExpandsOnDoubleClick(false);
    // A mixed selection (a collision line *and* a map object together) needs multi-select -
    // ctrl/shift-click across rows, same as the scene view already supports via rubber-band drag.
    setSelectionMode(QAbstractItemView::ExtendedSelection);

    connect(this, &QTreeWidget::itemDoubleClicked, this, &ModTreeWidget::onItemDoubleClicked);
    connect(this, &QTreeWidget::itemExpanded, this, &ModTreeWidget::onItemExpanded);
    connect(this, &QTreeWidget::customContextMenuRequested, this, &ModTreeWidget::onCustomContextMenuRequested);
    connect(this, &QTreeWidget::itemSelectionChanged, this, &ModTreeWidget::onItemSelectionChanged);
}

void ModTreeWidget::SetMod(EditorMod* pMod)
{
    // Rebuilding destroys every PathTreeItem, including ones tracking a currently-open tab -
    // remember which tabs were tracked so they can be re-linked below (SetMod can be called
    // while paths are still open, e.g. after creating a new level while other paths in this
    // same mod are open in tabs). NotifyTabOpened is a no-op for a tab that doesn't belong to
    // the mod the tree ends up showing (e.g. genuinely switching to a different mod, where every
    // other tab should already have been closed by then anyway), so this is safe either way.
    const QList<EditorTab*> stillOpenTabs = mUndoStackConnections.keys();

    for (auto it = mUndoStackConnections.begin(); it != mUndoStackConnections.end(); ++it)
    {
        disconnect(it.value());
    }
    mUndoStackConnections.clear();

    clear();
    mMod = pMod;

    if (!mMod)
    {
        return;
    }

    auto* pRoot = new GroupTreeItem(ModTreeItem::eType::eModRoot, mMod->mName);
    addTopLevelItem(pRoot);
    for (const QString& levelDir : mMod->DiscoverLevels())
    {
        auto* pLevelItem = new LevelTreeItem(levelDir);
        pRoot->addChild(pLevelItem);
        for (int pathId : mMod->DiscoverPathIds(levelDir))
        {
            pLevelItem->addChild(new PathTreeItem(levelDir, pathId));
        }
    }
    pRoot->setExpanded(true);

    for (EditorTab* pTab : stillOpenTabs)
    {
        NotifyTabOpened(pTab, pTab->GetJsonFileName());
    }
}

PathTreeItem* ModTreeWidget::FindPathItem(const QString& jsonFileName) const
{
    if (!mMod)
    {
        return nullptr;
    }
    QTreeWidgetItemIterator it(const_cast<ModTreeWidget*>(this));
    while (*it)
    {
        if (auto* pPathItem = dynamic_cast<PathTreeItem*>(*it))
        {
            if (mMod->PathJsonFile(pPathItem->LevelDir(), pPathItem->PathId()).compare(jsonFileName, Qt::CaseInsensitive) == 0)
            {
                return pPathItem;
            }
        }
        ++it;
    }
    return nullptr;
}

PathTreeItem* ModTreeWidget::FindPathItemByTab(EditorTab* pTab) const
{
    QTreeWidgetItemIterator it(const_cast<ModTreeWidget*>(this));
    while (*it)
    {
        if (auto* pPathItem = dynamic_cast<PathTreeItem*>(*it))
        {
            if (pPathItem->Tab() == pTab)
            {
                return pPathItem;
            }
        }
        ++it;
    }
    return nullptr;
}

void ModTreeWidget::NotifyTabOpened(EditorTab* pTab, const QString& jsonFileName)
{
    PathTreeItem* pPathItem = FindPathItem(jsonFileName);
    if (!pPathItem)
    {
        // Opened outside the current mod (classic File > Open Path, or no mod open at all) -
        // nothing in the tree to sync.
        return;
    }

    pPathItem->SetTab(pTab);
    RefreshPathSubtree(pPathItem);
    pPathItem->setExpanded(true);

    mUndoStackConnections[pTab] = connect(&pTab->GetUndoStack(), &QUndoStack::indexChanged, this, [this, pTab](int newIndex)
    {
        PathTreeItem* pStillOpen = FindPathItemByTab(pTab);
        if (!pStillOpen)
        {
            return;
        }

        // EditorTab pushes a SetSelectionCommand for every plain scene selection change (see
        // its SelectionChanged connection), same as any other undoable edit - so a full
        // RefreshPathSubtree used to run on every single click in the scene, tearing down and
        // rebuilding the whole subtree (losing manually-expanded nodes, scroll position, ...)
        // just to reflect a selection that changed, nothing structural. Recognise that case (the
        // command that was just applied going forward - command(newIndex-1) - or, if this
        // transition was an undo, the one about to be redone - command(newIndex) - is a
        // SetSelectionCommand either way) and only re-sync the tree's own highlighted rows
        // instead of rebuilding.
        QUndoStack& stack = pTab->GetUndoStack();
        const bool selectionOnly = pStillOpen->SubtreePopulated() &&
            ((newIndex > 0 && dynamic_cast<const SetSelectionCommand*>(stack.command(newIndex - 1))) ||
             (newIndex < stack.count() && dynamic_cast<const SetSelectionCommand*>(stack.command(newIndex))));

        if (selectionOnly)
        {
            SyncTreeSelectionFromScene(pStillOpen);
        }
        else
        {
            RefreshPathSubtree(pStillOpen);
        }
    });
}

void ModTreeWidget::NotifyTabClosed(EditorTab* pTab)
{
    auto it = mUndoStackConnections.find(pTab);
    if (it != mUndoStackConnections.end())
    {
        disconnect(it.value());
        mUndoStackConnections.erase(it);
    }

    if (PathTreeItem* pPathItem = FindPathItemByTab(pTab))
    {
        pPathItem->SetTab(nullptr);
        while (pPathItem->childCount() > 0)
        {
            delete pPathItem->takeChild(0);
        }
        pPathItem->SetSubtreePopulated(false);
    }
}

void ModTreeWidget::RefreshPathSubtree(PathTreeItem* pPathItem)
{
    EditorTab* pTab = pPathItem->Tab();
    if (!pTab)
    {
        return;
    }

    // Every row is about to be torn down and recreated from scratch (a brand new QTreeWidgetItem
    // always starts collapsed) - remember which ones the user had actually opened so they can be
    // put back, or something as unremarkable as adding one map object collapses the whole tree
    // shut around whatever the user was just looking at. Keyed by the underlying model pointer
    // (stable across this rebuild - only the QTreeWidgetItems representing it are being replaced)
    // rather than position, so this still lines up correctly even if cameras/objects were
    // reordered by whatever change triggered this refresh.
    bool wasCamerasExpanded = false;
    bool wasCollisionsExpanded = false;
    QSet<EditorCamera*> expandedCameras;
    for (int i = 0; i < pPathItem->childCount(); ++i)
    {
        auto* pGroupItem = dynamic_cast<GroupTreeItem*>(pPathItem->child(i));
        if (!pGroupItem)
        {
            continue;
        }
        if (pGroupItem->Type() == ModTreeItem::eType::eCamerasGroup)
        {
            wasCamerasExpanded = pGroupItem->isExpanded();
            for (int j = 0; j < pGroupItem->childCount(); ++j)
            {
                if (auto* pCameraItem = dynamic_cast<CameraTreeItem*>(pGroupItem->child(j)))
                {
                    if (pCameraItem->isExpanded())
                    {
                        expandedCameras.insert(pCameraItem->Camera());
                    }
                }
            }
        }
        else if (pGroupItem->Type() == ModTreeItem::eType::eCollisionsGroup)
        {
            wasCollisionsExpanded = pGroupItem->isExpanded();
        }
    }

    while (pPathItem->childCount() > 0)
    {
        delete pPathItem->takeChild(0);
    }

    auto* pCamerasGroup = new GroupTreeItem(ModTreeItem::eType::eCamerasGroup, tr("Cameras"));
    pPathItem->addChild(pCamerasGroup);
    pCamerasGroup->setExpanded(wasCamerasExpanded);
    for (auto& pCamera : pTab->GetModel().GetCameras())
    {
        // Model::CreateEmptyCameras fills every cell in the grid whether or not it's ever been
        // assigned an image/name yet - list every one of them here too (labelling an unnamed one
        // "x,y @ empty", same as CameraManager's own list - see CameraTreeItem/CameraLabel), so
        // an as-yet-imageless camera can still be found and right-clicked to open CameraManager
        // and give it one, the same way a real camera in the scene view can.
        auto* pCameraItem = new CameraTreeItem(pCamera.get());
        pCamerasGroup->addChild(pCameraItem);
        pCameraItem->setExpanded(expandedCameras.contains(pCamera.get()));
        for (auto& pMapObject : pCamera->mMapObjects)
        {
            pCameraItem->addChild(new MapObjectTreeItem(pMapObject.get()));
        }
    }

    auto* pCollisionsGroup = new GroupTreeItem(ModTreeItem::eType::eCollisionsGroup, tr("Collisions"));
    pPathItem->addChild(pCollisionsGroup);
    pCollisionsGroup->setExpanded(wasCollisionsExpanded);
    for (auto& pLine : pTab->GetModel().CollisionItems())
    {
        pCollisionsGroup->addChild(new CollisionLineTreeItem(pLine.get()));
    }

    pPathItem->SetSubtreePopulated(true);

    // Every row just got torn down and recreated from scratch, so any selection highlighting the
    // tree had is gone - restore it from the scene's own (still-intact) selection rather than
    // leaving the tree looking like nothing is selected.
    SyncTreeSelectionFromScene(pPathItem);
}

void ModTreeWidget::SyncTreeSelectionFromScene(PathTreeItem* pPathItem)
{
    EditorTab* pTab = pPathItem->Tab();
    if (!pTab)
    {
        return;
    }

    QSet<MapObjectBase*> selectedMapObjects;
    QSet<CollisionObject*> selectedLines;
    for (QGraphicsItem* pItem : pTab->GetScene().selectedItems())
    {
        if (auto* pRect = qgraphicsitem_cast<ResizeableRectItem*>(pItem))
        {
            selectedMapObjects.insert(pRect->GetMapObject());
        }
        else if (auto* pArrow = qgraphicsitem_cast<ResizeableArrowItem*>(pItem))
        {
            selectedLines.insert(pArrow->GetCollisionItem());
        }
    }

    // blockSignals rather than a reentrancy flag: setSelected() below would otherwise fire
    // itemSelectionChanged for every row touched, re-entering onItemSelectionChanged and pushing
    // the (already up to date) selection right back onto the scene for no reason.
    const bool wasBlocked = blockSignals(true);
    for (QTreeWidgetItemIterator it(pPathItem); *it; ++it)
    {
        // QTreeWidgetItemIterator walks the whole tree from pPathItem onward, including
        // everything after this path's own subtree - stop once we leave it.
        if (*it != pPathItem && FindAncestorPathItem(*it) != pPathItem)
        {
            break;
        }
        if (auto* pMapObjectItem = dynamic_cast<MapObjectTreeItem*>(*it))
        {
            const bool isSelected = selectedMapObjects.contains(pMapObjectItem->MapObject());
            pMapObjectItem->setSelected(isSelected);
            if (isSelected)
            {
                // A row being selected is exactly the case worth surfacing - e.g. the object
                // AddNewObjectCommand just created and selected inside a camera that had nothing
                // in it before, which otherwise stayed collapsed (a still-empty camera/group row
                // has no reason to auto-expand on its own - RefreshPathSubtree only preserves
                // expansion that was already there, see its own comment - so a brand new object's
                // very first appearance needs this extra push or there'd be nothing to preserve).
                ExpandAncestors(pMapObjectItem);
            }
        }
        else if (auto* pLineItem = dynamic_cast<CollisionLineTreeItem*>(*it))
        {
            const bool isSelected = selectedLines.contains(pLineItem->Line());
            pLineItem->setSelected(isSelected);
            if (isSelected)
            {
                ExpandAncestors(pLineItem);
            }
        }
    }
    blockSignals(wasBlocked);
}

void ModTreeWidget::onItemSelectionChanged()
{
    const QList<QTreeWidgetItem*> selected = selectedItems();
    if (selected.isEmpty())
    {
        return;
    }

    // Only ever drive the scene from a selection made up entirely of map-object/collision-line
    // rows (a "mixed" selection of the two is fine - that's exactly the case a rubber-band drag
    // in the scene already produces). Selecting a camera/group/path/level row alongside or
    // instead doesn't correspond to anything selectable in the scene (CameraGraphicsItem isn't
    // selectable at all) - leave the scene's current selection alone rather than silently
    // clearing it just because the user clicked some unrelated row.
    QSet<MapObjectBase*> wantedMapObjects;
    QSet<CollisionObject*> wantedLines;
    for (QTreeWidgetItem* pItem : selected)
    {
        if (auto* pMapObjectItem = dynamic_cast<MapObjectTreeItem*>(pItem))
        {
            wantedMapObjects.insert(pMapObjectItem->MapObject());
        }
        else if (auto* pLineItem = dynamic_cast<CollisionLineTreeItem*>(pItem))
        {
            wantedLines.insert(pLineItem->Line());
        }
        else
        {
            return;
        }
    }

    // A mixed selection only ever makes sense within one open path at a time - use whichever
    // path the first selected row belongs to, same as the scene itself (there's only ever one
    // scene selection to update).
    PathTreeItem* pPathItem = FindAncestorPathItem(selected.first());
    EditorTab* pTab = pPathItem ? pPathItem->Tab() : nullptr;
    if (!pTab)
    {
        return;
    }

    // Apply the new selection directly first, then record it on the undo stack exactly the way
    // EditorGraphicsScene's own mouse handlers do (see EditorTab's SelectionChanged connection -
    // that's also a "mutate first, push the command after" flow, not push-then-apply -
    // SetSelectionCommand's own first redo() is a no-op by design, assuming the caller already
    // applied it). Earlier this bypassed the undo stack entirely, which "worked" for the tree's
    // own display but left the undo stack's idea of the current selection stale - undoing some
    // unrelated later command could then restore a selection from *before* a tree click that, as
    // far as the undo stack was concerned, never happened. Going through SetSelectionCommand like
    // every other selection change keeps that one source of truth. It also does not reintroduce a
    // full tree rebuild: NotifyTabOpened's indexChanged handler already special-cases any
    // SetSelectionCommand (regardless of what triggered it) to just re-sync highlighting instead.
    EditorGraphicsScene& scene = pTab->GetScene();
    QList<QGraphicsItem*> oldSelection = scene.selectedItems();

    scene.clearSelection();
    for (QGraphicsItem* pItem : scene.items())
    {
        if (auto* pRect = qgraphicsitem_cast<ResizeableRectItem*>(pItem))
        {
            if (wantedMapObjects.contains(pRect->GetMapObject()))
            {
                pRect->setSelected(true);
            }
        }
        else if (auto* pArrow = qgraphicsitem_cast<ResizeableArrowItem*>(pItem))
        {
            if (wantedLines.contains(pArrow->GetCollisionItem()))
            {
                pArrow->setSelected(true);
            }
        }
    }

    QList<QGraphicsItem*> newSelection = scene.selectedItems();
    if (newSelection != oldSelection)
    {
        pTab->AddCommand(new SetSelectionCommand(pTab, &scene, oldSelection, newSelection));
    }
}

void ModTreeWidget::onItemDoubleClicked(QTreeWidgetItem* pItem, int /*column*/)
{
    if (auto* pLevelItem = dynamic_cast<LevelTreeItem*>(pItem))
    {
        pLevelItem->setExpanded(!pLevelItem->isExpanded());
        return;
    }

    if (auto* pPathItem = dynamic_cast<PathTreeItem*>(pItem))
    {
        if (mMod)
        {
            // OpenPath (EditorMainWindow::onOpenPath) already dedups against already-open tabs
            // by filename, so this is safe to call unconditionally whether or not the path is
            // already open.
            mMainWindow->OpenPath(mMod->PathJsonFile(pPathItem->LevelDir(), pPathItem->PathId()));
        }
        return;
    }

    if (auto* pCameraItem = dynamic_cast<CameraTreeItem*>(pItem))
    {
        if (PathTreeItem* pPathItem = FindAncestorPathItem(pCameraItem))
        {
            if (EditorTab* pTab = pPathItem->Tab())
            {
                const Model& model = pTab->GetModel();
                const EditorCamera* pCamera = pCameraItem->Camera();
                const QPointF center(
                    pCamera->mX * model.CameraGridWidth() + model.CameraGridWidth() / 2.0,
                    pCamera->mY * model.CameraGridHeight() + model.CameraGridHeight() / 2.0);
                pTab->CenterViewOn(center);
            }
        }
    }
}

void ModTreeWidget::onItemExpanded(QTreeWidgetItem* pItem)
{
    if (auto* pPathItem = dynamic_cast<PathTreeItem*>(pItem))
    {
        if (!pPathItem->SubtreePopulated() && pPathItem->Tab())
        {
            RefreshPathSubtree(pPathItem);
        }
    }
}

void ModTreeWidget::onCustomContextMenuRequested(const QPoint& pos)
{
    if (!mMod)
    {
        return;
    }

    QTreeWidgetItem* pItem = itemAt(pos);

    // Named so AutomationServer/tests can drive it the same way EditorGraphicsView's own
    // context menu already is (see its "graphicsViewContextMenu" object name).
    QMenu menu(this);
    menu.setObjectName("modTreeContextMenu");

    const auto* pModRoot = dynamic_cast<ModTreeItem*>(pItem);
    const bool isModRoot = pModRoot && pModRoot->Type() == ModTreeItem::eType::eModRoot;

    if (!pItem || isModRoot)
    {
        auto* pNewLevelAction = new QAction(tr("New Level..."), &menu);
        pNewLevelAction->setObjectName("actionNewLevel");
        connect(pNewLevelAction, &QAction::triggered, this, &ModTreeWidget::PromptNewLevel);
        menu.addAction(pNewLevelAction);
    }
    else if (auto* pLevelItem = dynamic_cast<LevelTreeItem*>(pItem))
    {
        auto* pNewPathAction = new QAction(tr("New Path..."), &menu);
        pNewPathAction->setObjectName("actionNewPath");
        connect(pNewPathAction, &QAction::triggered, this, [this, pLevelItem]() { PromptNewPath(pLevelItem); });
        menu.addAction(pNewPathAction);
    }
    else if (auto* pCameraItem = dynamic_cast<CameraTreeItem*>(pItem))
    {
        // A CameraTreeItem only ever exists under an already-populated (so already-open) path's
        // subtree - see RefreshPathSubtree - so its containing PathTreeItem is guaranteed to have
        // a live Tab().
        if (PathTreeItem* pPathItem = FindAncestorPathItem(pCameraItem))
        {
            if (EditorTab* pTab = pPathItem->Tab())
            {
                // Same action (name included) as EditorGraphicsView::contextMenuEvent's own
                // "Edit camera" - right-clicking a camera here should behave identically to
                // right-clicking it in the scene view, including for a camera that has no image
                // yet (now listed in the tree too, see RefreshPathSubtree above).
                EditorCamera* pCamera = pCameraItem->Camera();
                auto* pEditCameraAction = new QAction(tr("Edit camera"), &menu);
                pEditCameraAction->setObjectName("actionEditCamera");
                connect(pEditCameraAction, &QAction::triggered, this, [this, pTab, pCamera]()
                {
                    const QPoint scenePos(pCamera->mX * pTab->GetModel().CameraGridWidth(), pCamera->mY * pTab->GetModel().CameraGridHeight());
                    CameraManager cameraManager(this, pTab, &scenePos);
                    pTab->SetCameraManagerDialog(&cameraManager);
                    cameraManager.exec();
                    pTab->SetCameraManagerDialog(nullptr);
                });
                menu.addAction(pEditCameraAction);
            }
        }
    }

    if (menu.actions().count() > 0)
    {
        menu.exec(viewport()->mapToGlobal(pos));
    }
}

void ModTreeWidget::PromptNewLevel()
{
    bool ok = false;
    const QString levelDir = QInputDialog::getText(this, tr("New Level"), tr("Level folder name (e.g. a 2-letter abbreviation)"), QLineEdit::Normal, QString(), &ok).trimmed();
    if (!ok || levelDir.isEmpty())
    {
        return;
    }
    if (mMod->DiscoverLevels().contains(levelDir))
    {
        QMessageBox::warning(this, tr("New Level"), tr("That level already exists."));
        return;
    }

    mMod->CreateLevel(levelDir);
    SetMod(mMod);
}

void ModTreeWidget::PromptNewPath(LevelTreeItem* pLevelItem)
{
    const QVector<int> existingIds = mMod->DiscoverPathIds(pLevelItem->LevelDir());
    int suggestedId = 0;
    while (existingIds.contains(suggestedId))
    {
        suggestedId++;
    }

    bool ok = false;
    const int pathId = QInputDialog::getInt(this, tr("New Path"), tr("Path Id"), suggestedId, 0, 99, 1, &ok);
    if (!ok)
    {
        return;
    }
    if (existingIds.contains(pathId))
    {
        QMessageBox::warning(this, tr("New Path"), tr("That path id already exists in this level."));
        return;
    }

    // Add the tree node *before* creating/opening the path, so AddModelTab's NotifyTabOpened
    // (called internally by CreateAndOpenNewPath) can find and wire it up as soon as the tab
    // exists, rather than the tab existing with no matching tree node to link to yet.
    auto* pPathItem = new PathTreeItem(pLevelItem->LevelDir(), pathId);
    pLevelItem->addChild(pPathItem);
    // Reveal the newly created path right away rather than leaving it hidden under a still-
    // collapsed level.
    pLevelItem->setExpanded(true);

    const QString jsonFileName = mMod->PathJsonFile(pLevelItem->LevelDir(), pathId);
    if (!mMainWindow->CreateAndOpenNewPath(jsonFileName, pathId, mMod->mTargetGame))
    {
        delete pLevelItem->takeChild(pLevelItem->indexOfChild(pPathItem));
        QMessageBox::warning(this, tr("New Path"), tr("Failed to create the new path."));
        return;
    }

    mMod->RecordPathId(pLevelItem->LevelDir(), pathId);
}
