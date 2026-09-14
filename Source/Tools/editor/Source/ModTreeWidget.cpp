#include "ModTreeWidget.hpp"
#include "ModTreeItem.hpp"
#include "EditorMod.hpp"
#include "EditorMainWindow.hpp"
#include "EditorTab.hpp"
#include "Model.hpp"
#include "CameraManager.hpp"
#include <QTreeWidgetItemIterator>
#include <QMenu>
#include <QAction>
#include <QInputDialog>
#include <QMessageBox>
#include <QLineEdit>

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

    connect(this, &QTreeWidget::itemDoubleClicked, this, &ModTreeWidget::onItemDoubleClicked);
    connect(this, &QTreeWidget::itemExpanded, this, &ModTreeWidget::onItemExpanded);
    connect(this, &QTreeWidget::customContextMenuRequested, this, &ModTreeWidget::onCustomContextMenuRequested);
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

    mUndoStackConnections[pTab] = connect(&pTab->GetUndoStack(), &QUndoStack::indexChanged, this, [this, pTab](int)
    {
        if (PathTreeItem* pStillOpen = FindPathItemByTab(pTab))
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

    while (pPathItem->childCount() > 0)
    {
        delete pPathItem->takeChild(0);
    }

    auto* pCamerasGroup = new GroupTreeItem(ModTreeItem::eType::eCamerasGroup, tr("Cameras"));
    pPathItem->addChild(pCamerasGroup);
    for (auto& pCamera : pTab->GetModel().GetCameras())
    {
        // Model::CreateEmptyCameras fills every cell in the grid whether or not it's ever been
        // assigned an image/name yet - list every one of them here too (labelling an unnamed one
        // "x,y @ empty", same as CameraManager's own list - see CameraTreeItem/CameraLabel), so
        // an as-yet-imageless camera can still be found and right-clicked to open CameraManager
        // and give it one, the same way a real camera in the scene view can.
        auto* pCameraItem = new CameraTreeItem(pCamera.get());
        pCamerasGroup->addChild(pCameraItem);
        for (auto& pMapObject : pCamera->mMapObjects)
        {
            pCameraItem->addChild(new MapObjectTreeItem(pMapObject.get()));
        }
    }

    auto* pCollisionsGroup = new GroupTreeItem(ModTreeItem::eType::eCollisionsGroup, tr("Collisions"));
    pPathItem->addChild(pCollisionsGroup);
    for (auto& pLine : pTab->GetModel().CollisionItems())
    {
        pCollisionsGroup->addChild(new CollisionLineTreeItem(pLine.get()));
    }

    pPathItem->SetSubtreePopulated(true);
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
        // subtree - see RefreshPathSubtree - so its great-grandparent PathTreeItem is guaranteed
        // to have a live Tab().
        if (auto* pPathItem = dynamic_cast<PathTreeItem*>(pCameraItem->parent() ? pCameraItem->parent()->parent() : nullptr))
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
