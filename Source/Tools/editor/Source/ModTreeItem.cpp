#include "ModTreeItem.hpp"
#include "EditorCamera.hpp"
#include "CollisionObject.hpp"
#include "ReflectedEnumProperties.hpp"
#include "../../relive_api/TlvsRelive.hpp"

namespace
{
    // Same "<name> @ x,y" / "x,y @ empty" label CameraManager.cpp's CameraListItem::SetLabel
    // already uses, kept identical here for consistency between the two camera-listing UIs.
    QString CameraLabel(const EditorCamera* pCamera)
    {
        const QString pos = QString::number(pCamera->mX) + "," + QString::number(pCamera->mY);
        if (!pCamera->mName.empty())
        {
            return QString::fromStdString(pCamera->mName) + " @ " + pos;
        }
        return pos + " @ empty";
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
