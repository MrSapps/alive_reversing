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


signals:
    void SelectionChanged(QList<QGraphicsItem*> oldItems, QList<QGraphicsItem*> newItems);
    void ItemsMoved(ItemPositionData oldPositions, ItemPositionData newPositions);
private:
    void mousePressEvent(QGraphicsSceneMouseEvent* pEvent) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* pEvent) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* pEvent) override;

    void keyPressEvent(QKeyEvent* keyEvent) override;

    void CreateBackgroundBrush();

private:
    EditorTab* mTab = nullptr;
    QList<QGraphicsItem*> mOldSelection;
    ItemPositionData mOldPositions;
    bool mLeftButtonDown = false;
    TransparencySettings mTransparencySettings;
    bool mGridEnabled = false;
};
