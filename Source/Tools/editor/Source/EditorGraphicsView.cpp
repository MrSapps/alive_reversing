#include "EditorGraphicsView.hpp"
#include "EditorTab.hpp"
#include "EditorGraphicsScene.hpp"
#include "CameraManager.hpp"
#include "CameraGraphicsItem.hpp"
#include <QDebug>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>

EditorGraphicsView::EditorGraphicsView(EditorTab* editorTab)
    : mEditorTab(editorTab)
{
    setAcceptDrops(true);
}

void EditorGraphicsView::mousePressEvent(QMouseEvent* pEvent)
{
    if (pEvent->button() != Qt::LeftButton)
    {
        // prevent band dragging on other buttons
        qDebug() << "Ignore non left press";
        pEvent->ignore();
        return;
    }

    qDebug() << "view mouse press (left)";
    QGraphicsView::mousePressEvent(pEvent);
}

void EditorGraphicsView::mouseReleaseEvent(QMouseEvent* pEvent)
{
    if (pEvent->button() != Qt::LeftButton)
    {
        qDebug() << "Ignore non left release";
        pEvent->ignore();
        return;
    }

    qDebug() << "view mouse release (left)";
    QGraphicsView::mouseReleaseEvent(pEvent);
}

void EditorGraphicsView::wheelEvent(QWheelEvent* pEvent)
{
    if (pEvent->modifiers() == Qt::Modifier::CTRL)
    {
        pEvent->ignore();
        return;
    }
    QGraphicsView::wheelEvent(pEvent);
}


// TODO: implement proper ScrollHandDrag mode.
// you should be able to move around by pressing and holding the middle mouse button.
void EditorGraphicsView::keyPressEvent(QKeyEvent* pEvent)
{
    if (pEvent->key() == Qt::Key::Key_Shift)
    {
        setDragMode(DragMode::ScrollHandDrag);
        setInteractive(false);
        pEvent->ignore();
        return;
    }
    QGraphicsView::keyPressEvent(pEvent);
}

void EditorGraphicsView::keyReleaseEvent(QKeyEvent* pEvent)
{
    if (pEvent->key() == Qt::Key::Key_Shift)
    {
        setDragMode(DragMode::RubberBandDrag);
        setInteractive(true);
        pEvent->ignore();
        return;
    }
    QGraphicsView::keyPressEvent(pEvent);
}

void EditorGraphicsView::focusOutEvent(QFocusEvent* pEvent)
{
    if (pEvent->lostFocus())
    {
        // prevents ScrollHandDrag getting "stuck" when losing focus while holding shift
        setDragMode(DragMode::RubberBandDrag);
        setInteractive(true);

        // Cancel an in-progress "add collision" click-to-place (e.g. alt-tabbing away,
        // switching tabs, minimizing) - nothing is committed to the model/undo stack until the
        // second click, so there's nothing worth preserving through an interruption, unlike an
        // actual in-progress drag.
        mEditorTab->GetScene().CancelPlaceCollisionLine();
    }
    QGraphicsView::focusOutEvent(pEvent);
}

void EditorGraphicsView::leaveEvent(QEvent* pEvent)
{
    // Same reasoning as focusOutEvent: the mouse leaving the view entirely means there's no
    // sensible "second point" to keep waiting for.
    mEditorTab->GetScene().CancelPlaceCollisionLine();
    QGraphicsView::leaveEvent(pEvent);
}

void EditorGraphicsView::contextMenuEvent(QContextMenuEvent* pEvent)
{
    QMenu menu(this);

    const QPoint scenePos = mapToScene(pEvent->pos()).toPoint();
    QList<QGraphicsItem*> itemsAtMousePos = scene()->items(scenePos);
    qDebug() << "There are " << itemsAtMousePos.count() << " at the context menu";

    bool cameraAtMenu = false;
    for (int i = 0; i < itemsAtMousePos.count(); i++)
    {
        auto pCameraItem = qgraphicsitem_cast<CameraGraphicsItem*>(itemsAtMousePos[i]);
        if (pCameraItem)
        {
            cameraAtMenu = true;
            break;
        }
    }

    if (cameraAtMenu)
    {
        auto pEditCameraAction = new QAction(tr("Edit camera"), &menu);
        connect(pEditCameraAction, &QAction::triggered, this, [&]()
                {
                    CameraManager cameraManager(this, mEditorTab, &scenePos);
                    mEditorTab->SetCameraManagerDialog(&cameraManager);
                    cameraManager.exec();
                    mEditorTab->SetCameraManagerDialog(nullptr); });
        menu.addAction(pEditCameraAction);
    }

    if (scene()->selectedItems().count() > 0)
    {
        // TODO: Cut, Copy, Paste, Delete

        // TODO: Check if the selection has collision items
        // If nothing is selected its impossible to connect collision items
        auto pConnectCollisionsAction = new QAction(tr("Connect collisions"), &menu);
        connect(pConnectCollisionsAction, &QAction::triggered, this, [&]()
                { mEditorTab->ConnectCollisions(); });
        menu.addAction(pConnectCollisionsAction);
    }

    // TODO: Show paste if the global clipboard object owned by MainWindow isn't empty

    if (menu.actions().count() > 0)
    {
        menu.exec(pEvent->globalPos());
    }
}

void EditorGraphicsView::dragEnterEvent(QDragEnterEvent* pEvent)
{
    pEvent->acceptProposedAction();
}

void EditorGraphicsView::dragMoveEvent(QDragMoveEvent* pEvent)
{
    pEvent->acceptProposedAction();
}

void EditorGraphicsView::dropEvent(QDropEvent* pEvent)
{
    // Attempt to load the dropped image
    QUrl imgUrl = pEvent->mimeData()->urls().first();
    QPixmap img;

    if (!imgUrl.isLocalFile())
    {
        QMessageBox::critical(this, "Error", "Reading from remote file systems is unsupported.");
        return;
    }

    if (!img.load(imgUrl.toLocalFile()))
    {
        QMessageBox::critical(this, "Error", "The file dropped could not be understood as an image.");
        return;
    }

    // Image is valid, continue
    const QPoint scenePos = mapToScene(pEvent->pos()).toPoint();
    CameraManager cameraManager(this, mEditorTab, &scenePos);
    cameraManager.CreateCamera(true, img);
    pEvent->acceptProposedAction();
}
