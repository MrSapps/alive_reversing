#pragma once

#include <QGraphicsScene>
#include <QKeyEvent>
#include "ItemPositionData.hpp"
#include "TransparencySettings.hpp"

class ResizeableArrowItem;
class ResizeableRectItem;
class CameraGraphicsItem;
class EditorTab;
struct EditorCamera;
class Model;
class QGraphicsLineItem;

class EditorGraphicsScene final : public QGraphicsScene
{
    Q_OBJECT
public:
    explicit EditorGraphicsScene(EditorTab* pTab);

    QList<ResizeableRectItem*> MapObjectsForCamera(CameraGraphicsItem* pCameraGraphicsItem);

    void UpdateSceneRect();

    CameraGraphicsItem* CameraAt(int x, int y);

    TransparencySettings& GetTransparencySettings();

    void SyncTransparencySettings();
    void ToggleGrid();

    // Click-to-place tool for adding a collision line: click one endpoint, move the mouse (a
    // semi-transparent ghost line follows the cursor), click the second endpoint to commit.
    // Cancelled - with no command ever pushed - by Escape, the mouse leaving the view, or the
    // view losing focus (e.g. alt-tabbing away, switching tabs, minimizing): nothing is
    // committed to the model/undo stack until the second click, so there's no state worth
    // preserving across an interruption, unlike an in-progress drag or a modal dialog.
    void BeginPlaceCollisionLine();
    void CancelPlaceCollisionLine();
    bool IsPlacingCollisionLine() const
    {
        return mPlacingCollisionLine;
    }


signals:
    void SelectionChanged(QList<QGraphicsItem*> oldItems, QList<QGraphicsItem*> newItems);
    void ItemsMoved(ItemPositionData oldPositions, ItemPositionData newPositions);
    // So EditorMainWindow can keep actionAdd_collision's checked ("depressed") state in sync,
    // however placement starts or ends (the button, or a cancel via Escape/focus-loss/mouse-leave).
    void PlacingCollisionLineChanged(bool placing);
private:
    void mousePressEvent(QGraphicsSceneMouseEvent* pEvent) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* pEvent) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* pEvent) override;

    void keyPressEvent(QKeyEvent* keyEvent) override;

    void CreateBackgroundBrush();

    void HandlePlaceCollisionLineClick(QPointF scenePos);

private:
    EditorTab* mTab = nullptr;
    QList<QGraphicsItem*> mOldSelection;
    ItemPositionData mOldPositions;
    bool mLeftButtonDown = false;
    TransparencySettings mTransparencySettings;
    bool mGridEnabled = false;

    bool mPlacingCollisionLine = false;
    bool mHaveCollisionLineFirstPoint = false;
    QPointF mCollisionLineFirstPoint;
    QGraphicsLineItem* mCollisionLineGhost = nullptr;
};
