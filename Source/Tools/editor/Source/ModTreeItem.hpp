#pragma once

#include <QTreeWidgetItem>
#include <QString>

class EditorTab;
struct EditorCamera;
class MapObjectBase;
class CollisionObject;

// Per-node-type QTreeWidgetItem subclasses for ModTreeWidget, mirroring the pattern
// PropertyTreeItemBase/its subclasses already establish for the property panel: a small
// abstract base carrying a Type() tag (so a plain QTreeWidgetItem* from a click/double-click
// handler can be safely downcast), one concrete subclass per kind of node in the tree.
class ModTreeItem : public QTreeWidgetItem
{
public:
    enum class eType
    {
        eModRoot,
        eLevel,
        ePath,
        eCamerasGroup,
        eCamera,
        eMapObject,
        eCollisionsGroup,
        eCollisionLine,
    };

    ModTreeItem(eType type, const QString& text)
        : QTreeWidgetItem(QStringList{text}), mType(type)
    {
    }

    eType Type() const
    {
        return mType;
    }

private:
    eType mType;
};

// A level (a directory under the mod containing a paths/ folder).
class LevelTreeItem final : public ModTreeItem
{
public:
    LevelTreeItem(const QString& levelDir)
        : ModTreeItem(eType::eLevel, levelDir), mLevelDir(levelDir)
    {
    }

    const QString& LevelDir() const
    {
        return mLevelDir;
    }

private:
    QString mLevelDir;
};

// A single path (one path.json) within a level. Tracks the EditorTab it's currently open in (if
// any) so double-click/lookup can focus an already-open tab instead of reloading it, and so the
// tree knows which path node to refresh when that tab's undo stack changes (see
// ModTreeWidget::NotifyTabOpened/NotifyTabClosed).
class PathTreeItem final : public ModTreeItem
{
public:
    PathTreeItem(const QString& levelDir, int pathId)
        : ModTreeItem(eType::ePath, "Path " + QString::number(pathId)), mLevelDir(levelDir), mPathId(pathId)
    {
    }

    const QString& LevelDir() const
    {
        return mLevelDir;
    }

    int PathId() const
    {
        return mPathId;
    }

    EditorTab* Tab() const
    {
        return mTab;
    }

    void SetTab(EditorTab* pTab)
    {
        mTab = pTab;
        // Bold the row while its path is open, so it's obvious at a glance which paths in the
        // tree correspond to currently-open tabs.
        QFont f = font(0);
        f.setBold(pTab != nullptr);
        setFont(0, f);
    }

    // Whether this path's Cameras/Collisions subtree has been populated yet (lazy - only done
    // once the node is actually expanded, see ModTreeWidget::itemExpanded).
    bool SubtreePopulated() const
    {
        return mSubtreePopulated;
    }

    void SetSubtreePopulated(bool populated)
    {
        mSubtreePopulated = populated;
    }

private:
    QString mLevelDir;
    int mPathId = 0;
    EditorTab* mTab = nullptr;
    bool mSubtreePopulated = false;
};

// Group headers ("Cameras", "Collisions") under an open path - exist only so double-click/expand
// behaves sensibly and so the two kinds of children aren't interleaved; carry no data of their
// own.
class GroupTreeItem final : public ModTreeItem
{
public:
    GroupTreeItem(ModTreeItem::eType type, const QString& text)
        : ModTreeItem(type, text)
    {
    }
};

class CameraTreeItem final : public ModTreeItem
{
public:
    CameraTreeItem(EditorCamera* pCamera);

    EditorCamera* Camera() const
    {
        return mCamera;
    }

private:
    EditorCamera* mCamera = nullptr;
};

class MapObjectTreeItem final : public ModTreeItem
{
public:
    MapObjectTreeItem(MapObjectBase* pMapObject);

    MapObjectBase* MapObject() const
    {
        return mMapObject;
    }

private:
    MapObjectBase* mMapObject = nullptr;
};

class CollisionLineTreeItem final : public ModTreeItem
{
public:
    CollisionLineTreeItem(CollisionObject* pLine);

    CollisionObject* Line() const
    {
        return mLine;
    }

private:
    CollisionObject* mLine = nullptr;
};
