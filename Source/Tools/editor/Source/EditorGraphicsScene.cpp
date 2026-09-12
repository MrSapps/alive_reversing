#include "EditorGraphicsScene.hpp"
#include "ItemPositionData.hpp"
#include <QPainter>
#include <QDebug>
#include <QGraphicsSceneMouseEvent>
#include "ResizeableArrowItem.hpp"
#include "ResizeableRectItem.hpp"
#include "CameraGraphicsItem.hpp"
#include "EditorTab.hpp"
#include "Model.hpp"
#include <QUndoCommand>
#include "DeleteItemsCommand.hpp"
#include "AddCollisionCommand.hpp"
#include <QGraphicsLineItem>
#include <QGraphicsView>
#include <QPen>
#include <QKeyEvent>

EditorGraphicsScene::EditorGraphicsScene(EditorTab* pTab)
    : mTab(pTab)
{
    ToggleGrid();
}

QList<ResizeableRectItem*> EditorGraphicsScene::MapObjectsForCamera(CameraGraphicsItem* pCameraGraphicsItem)
{
    const auto& modelMapObjects = pCameraGraphicsItem->GetCamera()->mMapObjects;

    QList<ResizeableRectItem*> graphicsItemMapObjects;
    QList<QGraphicsItem*> allItems = items();
    for (QGraphicsItem* item : allItems)
    {
        ResizeableRectItem* pCastedGraphicsItemMapObject = qgraphicsitem_cast<ResizeableRectItem*>(item);
        if (pCastedGraphicsItemMapObject)
        {
            for (auto& mapModelObj : modelMapObjects)
            {
                if (mapModelObj.get() == pCastedGraphicsItemMapObject->GetMapObject())
                {
                    graphicsItemMapObjects.append(pCastedGraphicsItemMapObject);
                    break;
                }
            }
        }
    }
    return graphicsItemMapObjects;
}

void EditorGraphicsScene::UpdateSceneRect()
{
    const int kXMargin = 100;
    const int kYMargin = 100;
    const auto& model = mTab->GetModel();
    setSceneRect(-kXMargin, -kYMargin, (model.CameraGridWidth() * model.XSize()) + (kXMargin * 2), (model.CameraGridHeight() * model.YSize()) + (kYMargin * 2));
}

CameraGraphicsItem* EditorGraphicsScene::CameraAt(int x, int y)
{
    
    QList<QGraphicsItem*> allItems = items();
    for (QGraphicsItem* item : allItems)
    {
        CameraGraphicsItem* pCameraItem = qgraphicsitem_cast<CameraGraphicsItem*>(item);
        if (pCameraItem)
        {
            if (pCameraItem->GetCamera()->mX == x && pCameraItem->GetCamera()->mY == y)
            {
                return pCameraItem;
            }
        }
    }
    
    return nullptr;
}

TransparencySettings& EditorGraphicsScene::GetTransparencySettings()
{
    return mTransparencySettings;
}

void EditorGraphicsScene::SyncTransparencySettings()
{
    QList<QGraphicsItem*> objs = items();
    for (auto& obj : objs)
    {
        auto pCameraGraphicsItem = qgraphicsitem_cast<CameraGraphicsItem*>(obj);
        if (pCameraGraphicsItem)
        {
            IGraphicsItem::SetTransparency(pCameraGraphicsItem, mTransparencySettings.CameraTransparency());
        }
        else
        {
            auto pResizeableArrowItem = qgraphicsitem_cast<ResizeableArrowItem*>(obj);
            if (pResizeableArrowItem)
            {
                pResizeableArrowItem->SetTransparency(pResizeableArrowItem, mTransparencySettings.CollisionTransparency());
            }
            else
            {
                auto pResizeableRectItem = qgraphicsitem_cast<ResizeableRectItem*>(obj);
                if (pResizeableRectItem)
                {
                    pResizeableRectItem->SetTransparency(pResizeableRectItem, mTransparencySettings.MapObjectTransparency());
                }
            }
        }
    }
}

void EditorGraphicsScene::CreateBackgroundBrush()
{
    int gridSizeX = 25;
    int gridSizeY = 20;

    // Paint a single grid background onto a pixmap.
    QImage singleGrid(gridSizeX, gridSizeY, QImage::Format_RGB32);
    singleGrid.fill(QColor(50, 50, 50));
    {
        QPainter painter(&singleGrid);

        // Lighter background
        /*painter.setPen(QPen(QColor(140, 140, 140)));

        // Draw grid mid lines
        qreal midx = gridSizeX / 2;
        qreal midy = gridSizeY / 2;
        painter.drawLine(0, midy, gridSizeX, midy);
        painter.drawLine(midx, 0, midx, gridSizeY);*/

        // Darker foreground
        painter.setPen(QPen(QColor(80, 80, 80)));

        // Draw main grid lines
        painter.drawRect(0, 0, gridSizeX, gridSizeY);

        painter.end();
    }

    // Use this as the background brush which will be tiled and scale
    // properly when the scene is zoomed. And also prevents rendering issues compared to
    // the overriding drawBackground() method.
    QBrush brushBackground(singleGrid);
    setBackgroundBrush(brushBackground);
}

void EditorGraphicsScene::ToggleGrid()
{
    mGridEnabled = !mGridEnabled;
    if (mGridEnabled)
    {
        CreateBackgroundBrush();
    }
    else
    {
        QBrush b;
        setBackgroundBrush(b);
    }
}

void EditorGraphicsScene::mousePressEvent(QGraphicsSceneMouseEvent* pEvent)
{
    if (mPlacingCollisionLine)
    {
        if (pEvent->button() == Qt::LeftButton)
        {
            // Explicitly accept, not just "handled by returning": QGraphicsView starts its
            // RubberBandDrag tracking whenever the forwarded press comes back unaccepted, which
            // is this event's default state regardless of dragMode - and on the *completing*
            // click specifically, HandlePlaceCollisionLineClick below restores dragMode back to
            // RubberBandDrag as part of ending placement, before QGraphicsView's own check runs
            // (it happens immediately after this handler returns) - so without an explicit
            // accept() here, that one click's trailing drag (if the button is released even a
            // moment later than the press) could still start a rubber band regardless of the
            // NoDrag mode used for every other click during placement.
            pEvent->accept();
            HandlePlaceCollisionLineClick(pEvent->scenePos());
        }
        return;
    }

    if (pEvent->button() != Qt::LeftButton)
    {
        qDebug() << "Ignore non left click";
        pEvent->ignore();
        return;
    }

    if (pEvent->button() == Qt::LeftButton)
    {
        mLeftButtonDown = true;

        qDebug() << "left press";

        // Record what we had before a click
        mOldSelection = selectedItems();

        // Do the click
        QGraphicsScene::mousePressEvent(pEvent);

        if (mOldSelection != selectedItems())
        {
            qDebug() << "left press selection changed";

            // A single item just got selected by being clicked on
            emit SelectionChanged(mOldSelection, selectedItems());
            mOldSelection = selectedItems();
        }

        // Save the locations of what is selected after click
        mOldPositions.Save(mOldSelection, mTab->GetModel(), false);
    }
    else
    {
        qDebug() << "mouse press";
        QGraphicsScene::mousePressEvent(pEvent);
    }
}

void EditorGraphicsScene::mouseMoveEvent(QGraphicsSceneMouseEvent* pEvent)
{
    if (mPlacingCollisionLine)
    {
        if (mHaveCollisionLineFirstPoint && mCollisionLineGhost)
        {
            QLineF line = mCollisionLineGhost->line();
            line.setP2(pEvent->scenePos());
            mCollisionLineGhost->setLine(line);
        }

        // Re-assert the cross cursor on every move: Qt dispatches hover events to whatever
        // item is under the cursor (ResizeableArrowItem/ResizeableRectItem's hover handlers set
        // their own cursor, e.g. an open hand) as part of the same event that leads here, before
        // this override runs - so without this, hovering an existing item while placing would
        // silently steal the cursor back, which is what made it "sometimes" look wrong.
        if (!views().isEmpty())
        {
            views().first()->setCursor(Qt::CrossCursor);
        }
        return;
    }

    QGraphicsScene::mouseMoveEvent(pEvent);
}

void EditorGraphicsScene::mouseReleaseEvent(QGraphicsSceneMouseEvent* pEvent)
{
    if (mPlacingCollisionLine)
    {
        return;
    }

    // Handle the button up
    QGraphicsScene::mouseReleaseEvent(pEvent);

    if (pEvent->button() == Qt::LeftButton)
    {
        qDebug() << "left release";

        // Find out what is selected
        QList<QGraphicsItem*> currentSelection = selectedItems();

        if (mOldSelection != currentSelection)
        {
            // Between mouse up/down the selection changed
            qDebug() << "left release selection changed";
            emit SelectionChanged(mOldSelection, currentSelection);
        }

        if (mOldPositions.Count() > 0)
        {
            // Get the position of where the selected items are now
            ItemPositionData newPositions;
            newPositions.Save(currentSelection, mTab->GetModel(), true);

            // Keep the whole selection within the map bounds as a rigid group. Only the one
            // item actually grabbed during a multi-item drag gets clamped to *its own* bounds
            // live (ResizeableRectItem/ResizeableArrowItem::mouseMoveEvent) - Qt's default
            // QGraphicsItem::mouseMoveEvent moves every other selected item by the raw delta
            // with no clamping at all, so a multi-select drag could otherwise leave part of the
            // selection outside the map. This re-clamps the union of everything that moved.
            newPositions.ClampToMapBounds(*mTab);

            if (mOldPositions != newPositions)
            {
                qDebug() << "left release positions changed";

                // They've moved since mouse down
                emit ItemsMoved(mOldPositions, newPositions);
            }
        }

        mLeftButtonDown = false;
    }
}

void EditorGraphicsScene::keyPressEvent(QKeyEvent* keyEvent)
{
    if (mPlacingCollisionLine && keyEvent->key() == Qt::Key_Escape)
    {
        CancelPlaceCollisionLine();
        return;
    }

    if (keyEvent->key() == Qt::Key_Delete)
    {
        QList<QGraphicsItem*> selected = selectedItems();
        if (!selected.isEmpty())
        {
            mTab->AddCommand(new DeleteItemsCommand(mTab, false, selected));
        }
    }
    else
    {
        QGraphicsScene::keyPressEvent(keyEvent);
    }
}

void EditorGraphicsScene::BeginPlaceCollisionLine()
{
    CancelPlaceCollisionLine();
    mPlacingCollisionLine = true;
    mHaveCollisionLineFirstPoint = false;
    clearSelection();

    // The mouse *release* that follows the second (committing) click arrives after placement
    // has already ended (HandlePlaceCollisionLineClick resets it synchronously as part of
    // handling that press), so it falls through to mouseReleaseEvent's normal, non-placing
    // path - which compares against mOldSelection/mOldPositions to decide whether to push a
    // SetSelectionCommand/MoveItemsCommand. Left stale from whatever they were before placement
    // started, that comparison would spuriously "detect" the selection change AddCollisionCommand
    // just made (selecting the new line) and push an extra, wrong undo entry on top of it.
    // Resetting them now, and again right when a line is actually committed below, keeps that
    // comparison honest.
    mOldSelection = selectedItems();
    mOldPositions = ItemPositionData();

    // QGraphicsView::RubberBandDrag draws its rubber-band overlay and starts item selection
    // entirely inside the view's own mousePressEvent/mouseMoveEvent - overriding this scene's
    // virtual mousePressEvent (returning without calling the base implementation, so nothing
    // "accepts" the event) doesn't stop it, since the view decides whether to start a rubber
    // band before the scene's *virtual* handler even runs. Switching off drag mode entirely is
    // the only way to actually suppress it while placing.
    if (!views().isEmpty())
    {
        QGraphicsView* view = views().first();
        view->setDragMode(QGraphicsView::NoDrag);
        view->setCursor(Qt::CrossCursor);
    }

    emit PlacingCollisionLineChanged(true);
}

void EditorGraphicsScene::CancelPlaceCollisionLine()
{
    const bool wasPlacing = mPlacingCollisionLine;

    if (mCollisionLineGhost)
    {
        removeItem(mCollisionLineGhost);
        delete mCollisionLineGhost;
        mCollisionLineGhost = nullptr;
    }
    mPlacingCollisionLine = false;
    mHaveCollisionLineFirstPoint = false;

    if (wasPlacing)
    {
        if (!views().isEmpty())
        {
            QGraphicsView* view = views().first();
            view->setDragMode(QGraphicsView::RubberBandDrag);
            view->unsetCursor();
        }

        emit PlacingCollisionLineChanged(false);
    }
}

void EditorGraphicsScene::HandlePlaceCollisionLineClick(QPointF scenePos)
{
    if (!mHaveCollisionLineFirstPoint)
    {
        mCollisionLineFirstPoint = scenePos;
        mHaveCollisionLineFirstPoint = true;

        QPen ghostPen(QColor(255, 255, 0, 180));
        ghostPen.setStyle(Qt::DashLine);
        ghostPen.setWidth(2);
        ghostPen.setCosmetic(true);

        mCollisionLineGhost = new QGraphicsLineItem(QLineF(scenePos, scenePos));
        mCollisionLineGhost->setPen(ghostPen);
        mCollisionLineGhost->setZValue(10.0);
        addItem(mCollisionLineGhost);
    }
    else
    {
        const QPointF firstPoint = mCollisionLineFirstPoint;
        const QPointF secondPoint = scenePos;

        // Clears the ghost and resets placement state before pushing the command - the command's
        // own construction (MakeResizeableArrowItem etc.) shouldn't run while still "placing".
        CancelPlaceCollisionLine();

        mTab->AddCommand(new AddCollisionCommand(mTab, firstPoint.toPoint(), secondPoint.toPoint()));

        // AddCollisionCommand::redo() selects the new line - resync mOldSelection to match so
        // the mouseReleaseEvent still to come for this same click (now running the normal,
        // non-placing path - see BeginPlaceCollisionLine's comment) doesn't mistake that for a
        // user-driven selection change and push a spurious SetSelectionCommand on top.
        mOldSelection = selectedItems();
    }
}

