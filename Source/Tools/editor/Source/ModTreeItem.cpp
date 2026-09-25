#include "ModTreeItem.hpp"
#include "EditorCamera.hpp"
#include "CollisionObject.hpp"
#include "ReflectedEnumProperties.hpp"
#include "../../relive_lib/data_conversion/EditorMapObjects.hpp"

namespace
{
    // Same "<name> @ x,y" / "x,y @ empty" label CameraManager.cpp's CameraListItem::SetLabel
    // already uses, kept identical here for consistency between the two camera-listing UIs -
    // plus a "(no image)" suffix for a named camera with no main image. That's a real, reachable
    // state: CameraGraphicsItem::Load silently leaves mCameraImage null if "<name>.png" is
    // missing from disk when the path is (re)opened (deleted/moved outside the editor), and
    // mName alone can't tell that apart from a camera that loaded fine. Without this, giving a
    // camera whose image failed to load a *new* one had no visible effect on its tree row either:
    // the row genuinely gets torn down and rebuilt (RefreshPathSubtree runs off every undoable
    // command, including setting a camera's image), but its label depended on nothing that
    // changed, so the "refresh" was invisible.
    QString CameraLabel(const EditorCamera* pCamera)
    {
        const QString pos = QString::number(pCamera->mX) + "," + QString::number(pCamera->mY);
        if (pCamera->mName.empty())
        {
            return pos + " @ empty";
        }
        QString label = QString::fromStdString(pCamera->mName) + " @ " + pos;
        if (pCamera->mCameraImageandLayers.mCameraImage.isNull())
        {
            label += " (no image)";
        }
        return label;
    }

    QString MapObjectLabel(const MapObjectBase* pMapObject)
    {
        const char* typeName = EnumReflector<ReliveTypes>::to_str(EnumReflector<ReliveTypes>::to_index(pMapObject->mBaseTlv->mTlvType));
        return QString("%1 (%2, %3)").arg(typeName).arg(pMapObject->XPos()).arg(pMapObject->YPos());
    }

    QString CollisionLineLabel(const CollisionObject* pLine)
    {
        return QString("Line %1: (%2,%3)-(%4,%5)").arg(pLine->mId).arg(pLine->X1()).arg(pLine->Y1()).arg(pLine->X2()).arg(pLine->Y2());
    }
}

CameraTreeItem::CameraTreeItem(EditorCamera* pCamera)
    : ModTreeItem(eType::eCamera, CameraLabel(pCamera)), mCamera(pCamera)
{
}

MapObjectTreeItem::MapObjectTreeItem(MapObjectBase* pMapObject)
    : ModTreeItem(eType::eMapObject, MapObjectLabel(pMapObject)), mMapObject(pMapObject)
{
}

CollisionLineTreeItem::CollisionLineTreeItem(CollisionObject* pLine)
    : ModTreeItem(eType::eCollisionLine, CollisionLineLabel(pLine)), mLine(pLine)
{
}
