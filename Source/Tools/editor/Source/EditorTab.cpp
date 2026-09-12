#include "EditorTab.hpp"
#include "EditorGraphicsView.hpp"
#include "ui_EditorTab.h"
#include <QGraphicsPathItem>
#include <QPainterPath>
#include <QPushButton>
#include <QMessageBox>
#include <QDebug>
#include <QGraphicsSceneMouseEvent>
#include <QOpenGLWidget>
#include <QUndoCommand>
#include <QSpinBox>
#include <QMenu>
#include <QStatusBar>
#include <QFileDialog>
#include "ResizeableArrowItem.hpp"
#include "ResizeableRectItem.hpp"
#include "CameraGraphicsItem.hpp"
#include "EditorGraphicsScene.hpp"
#include <QLineEdit>
#include "StringProperty.hpp"
#include "PropertyTreeWidget.hpp"
#include <QFileInfo>
#include "BasicTypeProperty.hpp"
#include "EnumProperty.hpp"
#include "CameraManager.hpp"
#include "ChangeMapSizeDialog.hpp"
#include "MessageEditorDialog.hpp"
#include "PathDataEditorDialog.hpp"
#include "AddObjectDialog.hpp"
#include "GridPlacement.hpp"
#include "TransparencyDialog.hpp"
#include "ProgressDialog.hpp"
#include <QFutureWatcher>
#include <QFuture>
#include <QtConcurrent/QtConcurrent>
#include "DeleteItemsCommand.hpp"
#include "ClipBoard.hpp"
#include "PasteItemsCommand.hpp"
#include "SetSelectionCommand.hpp"
#include "MoveItemsCommand.hpp"
#include "AddCollisionCommand.hpp"
#include "../../../relive_lib/Grid.hpp"
#include "CollisionConnect.hpp"
#include "Model.hpp"

#include "../../../relive_lib/Ipc/Ipc.hpp"

// Zoom by 10% each time.
const float KZoomFactor = 0.10f;
const float KMaxZoomOutLevels = 6.0f;
const float KMaxZoomInLevels = 14.0f;

//INITIALIZE_EASYLOGGINGPP

EditorTab::EditorTab(QTabWidget* aParent, std::unique_ptr<Model> model, QString jsonFileName, bool isTempFile, QStatusBar* pStatusBar, GridSnapSettings& snapSettings)
    : QMainWindow(aParent),
    ui(new Ui::EditorTab),
    mModel(std::move(model)),
    mJsonFileName(jsonFileName),
    mParent(aParent),
    mIsTempFile(isTempFile),
    mStatusBar(pStatusBar),
    mSnapSettings(snapSettings),
    mPathDirectory(QFileInfo(jsonFileName).path())
{
    ui->setupUi(this);

    // TODO: Set as a promoted type
    delete ui->graphicsView;
    ui->graphicsView = new EditorGraphicsView(this);
    ui->graphicsView->setObjectName(QStringLiteral("graphicsView")); // preserve the name setupUi() gave the widget this replaces
    QGraphicsView* pView = ui->graphicsView;
    pView->setDragMode(QGraphicsView::RubberBandDrag);


    pView->setRenderHint(QPainter::SmoothPixmapTransform);
    pView->setRenderHint(QPainter::Antialiasing);
    pView->setRenderHint(QPainter::TextAntialiasing);

    // turn off for now because of performance issues when a lot of
    // objects are onscreen
    //pView->setViewport(new QOpenGLWidget()); // Becomes owned by the view

    mScene = std::make_unique<EditorGraphicsScene>(this);

    connect(mScene.get(), &EditorGraphicsScene::SelectionChanged, this, [&](QList<QGraphicsItem*> oldSelection, QList<QGraphicsItem*> newSelection)
        {
            mUndoStack.push(new SetSelectionCommand(this, mScene.get(), oldSelection, newSelection));
        });

    connect(mScene.get(), &EditorGraphicsScene::ItemsMoved, this, [&](ItemPositionData oldPositions, ItemPositionData newPositions)
        {
            mUndoStack.push(new MoveItemsCommand(mScene.get(), oldPositions, newPositions, *mModel));
        });

    connect(&mUndoStack, &QUndoStack::cleanChanged, this, &EditorTab::cleanChanged);

    iZoomLevel = 1.0f;
    for (int i = 0; i < 2; ++i)
    {
        //  ZoomIn();
    }

    setCentralWidget(ui->graphicsView);

    // TODO: Temp hack
    delete ui->treeWidget;
    ui->treeWidget = new PropertyTreeWidget(ui->dockWidgetContents_2);
    ui->treeWidget->setObjectName(QStringLiteral("treeWidget")); // preserve the name setupUi() gave the widget this replaces
    ui->verticalLayout_5->addWidget(ui->treeWidget);

    // Disable "already disabled" context menus on the QDockWidgets
    ui->propertyDockWidget->setContextMenuPolicy(Qt::PreventContextMenu);
    ui->undoHistoryDockWidget->setContextMenuPolicy(Qt::PreventContextMenu);

    QFileInfo fi(jsonFileName);

    for (u32 x = 0; x < mModel->XSize(); x++)
    {
        for (u32 y = 0; y < mModel->YSize(); y++)
        {
            EditorCamera* pCam = mModel->CameraAt(x, y);
            auto pCameraGraphicsItem = MakeCameraGraphicsItem(pCam, mModel->CameraGridWidth() * x, y * mModel->CameraGridHeight(), mModel->CameraGridWidth(), mModel->CameraGridHeight());
            pCameraGraphicsItem->Load(fi.dir().absolutePath());
            mScene->addItem(pCameraGraphicsItem);

            if (pCam)
            {
                for (auto& mapObj : pCam->mMapObjects)
                {
                    auto pMapObject = MakeResizeableRectItem(mapObj.get());
                    mScene->addItem(pMapObject);
                }
            }
        }
    }


    for (auto& collision : mModel->CollisionItems())
    {
        auto pLine = MakeResizeableArrowItem(collision.get());
        mScene->addItem(pLine);
    }

    mScene->UpdateSceneRect();

    ui->graphicsView->setScene(mScene.get());

    mUndoStack.setUndoLimit(100);
    ui->undoView->setStack(&mUndoStack);

    static_cast<PropertyTreeWidget*>(ui->treeWidget)->Init();

    addDockWidget(Qt::RightDockWidgetArea, ui->propertyDockWidget);
    addDockWidget(Qt::RightDockWidgetArea, ui->undoHistoryDockWidget);

    ui->propertyDockWidget->setMinimumWidth(310);

    setContextMenuPolicy(Qt::PreventContextMenu);

    connect(&mUndoStack , &QUndoStack::cleanChanged, this, &EditorTab::UpdateTabTitle);
}

ResizeableRectItem* EditorTab::MakeResizeableRectItem(MapObjectBase* pMapObject)
{
    return new ResizeableRectItem(ui->graphicsView, pMapObject, *static_cast<PropertyTreeWidget*>(ui->treeWidget), mScene->GetTransparencySettings().MapObjectTransparency(), mSnapSettings, *this);
}

ResizeableArrowItem* EditorTab::MakeResizeableArrowItem(CollisionObject* pCollisionObject)
{
    return new ResizeableArrowItem(ui->graphicsView, pCollisionObject, *static_cast<PropertyTreeWidget*>(ui->treeWidget), mScene->GetTransparencySettings().CollisionTransparency(), mSnapSettings, *this);
}

CameraGraphicsItem* EditorTab::MakeCameraGraphicsItem(EditorCamera* pCamera, int x, int y, int w, int h)
{
    return new CameraGraphicsItem(pCamera, x, y, w, h, mScene->GetTransparencySettings().CameraTransparency());
}

void EditorTab::SyncPropertyEditor()
{
    auto selected = mScene->selectedItems();
    if (selected.count() == 1)
    {
        PopulatePropertyEditor(selected[0]);
    }
    else
    {
        ClearPropertyEditor();
    }
}

void EditorTab::EditTransparency()
{
    auto transparencyDialog = new TransparencyDialog(this, this);
    transparencyDialog->exec();
    delete transparencyDialog;
}

void EditorTab::Cut(ClipBoard& clipBoard)
{
    if (!mScene->selectedItems().isEmpty())
    {
        clipBoard.Set(mScene->selectedItems(), *mModel);
        mUndoStack.push(new DeleteItemsCommand(this, true, mScene->selectedItems()));
        mStatusBar->showMessage(tr("Selection cut"), 2000);
    }
}

void EditorTab::Copy(ClipBoard& clipBoard)
{
    if (!mScene->selectedItems().isEmpty())
    {
        clipBoard.Set(mScene->selectedItems(), *mModel);
        mStatusBar->showMessage(tr("Selection copied"), 2000);
    }
}

void EditorTab::Paste(ClipBoard& clipBoard)
{
    if (!clipBoard.IsEmpty())
    {
        mUndoStack.push(new PasteItemsCommand(this, clipBoard));
        mStatusBar->showMessage(tr("Items pasted"), 2000);
    }
}

void EditorTab::cleanChanged(bool /*clean*/)
{
    UpdateCleanState();
}

void EditorTab::UpdateTabTitle(bool clean)
{
    QFileInfo fileInfo(mJsonFileName);
    QString title = fileInfo.fileName();
    if (!clean || mIsTempFile)
    {
        title += "*";
    }

    for (int i = 0; i < mParent->count(); i++)
    {
        if (mParent->widget(i) == this)
        {
            mParent->setTabText(i, title);
            break;
        }
    }
}

void EditorTab::wheelEvent(QWheelEvent* pEvent)
{
    if (pEvent->modifiers() == Qt::Modifier::CTRL)
    {
        if (pEvent->angleDelta().y() > 0)
        {
            ZoomIn();
        }
        else
        {
            ZoomOut();
        }
    }
    QWidget::wheelEvent(pEvent);
}

// TODO: zoom doesn't work properly without ui->graphicsView->resetTransform().
// figure out why and if there's a better way.
void EditorTab::ZoomIn()
{
    if (iZoomLevel < 1.0f + (KZoomFactor*KMaxZoomInLevels))
    {
        iZoomLevel += KZoomFactor;
        ui->graphicsView->resetTransform();
        ui->graphicsView->setTransform(QTransform::fromScale(iZoomLevel, iZoomLevel), true);
    }
}

void EditorTab::ZoomOut()
{
    if (iZoomLevel > 1.0f - (KZoomFactor*KMaxZoomOutLevels))
    {
        iZoomLevel -= KZoomFactor;
        ui->graphicsView->resetTransform();
        ui->graphicsView->setTransform(QTransform::fromScale(iZoomLevel, iZoomLevel), true);
    }
}

void EditorTab::ResetZoom()
{
    iZoomLevel = 1.0f;
    ui->graphicsView->resetTransform();
    ui->graphicsView->setTransform(QTransform::fromScale(iZoomLevel, iZoomLevel), true);
}

EditorTab::~EditorTab()
{
    disconnect(&mUndoStack, &QUndoStack::cleanChanged, this, &EditorTab::UpdateTabTitle);
    delete ui;
}

void EditorTab::ClearPropertyEditor()
{
    auto pTree = static_cast<PropertyTreeWidget*>(ui->treeWidget);
    pTree->DePopulate();
}

void EditorTab::PopulatePropertyEditor(QGraphicsItem* pItem)
{
    ClearPropertyEditor();

    auto pTree = static_cast<PropertyTreeWidget*>(ui->treeWidget);
    pTree->Populate(mUndoStack, pItem);
}

void EditorTab::Undo()
{
    mUndoStack.undo();
}

void EditorTab::Redo()
{
    mUndoStack.redo();
}

bool EditorTab::Save()
{
    if (mIsTempFile)
    {
        return SaveAs();
    }
    else
    {
        return DoSave(mJsonFileName);
    }
}

bool EditorTab::SaveAs()
{
    QString jsonSaveFileName = QFileDialog::getSaveFileName(this, tr("Save " + mJsonFileName.toLocal8Bit() + " as json"), "", tr("Json Files (*.json);;All Files (*)"));
    if (jsonSaveFileName.isEmpty())
    {
        // They didn't want to save it
        return false;
    }

    return SaveAsPath(jsonSaveFileName);
}

bool EditorTab::SaveAsPath(QString jsonSaveFileName)
{
    // Append .json file ext if not specified
    if (!jsonSaveFileName.endsWith(".json", Qt::CaseInsensitive))
    {
        jsonSaveFileName += ".json";
    }

    if (!DoSave(jsonSaveFileName))
    {
        return false;
    }

    // Saved OK, update the file name and tab title
    mJsonFileName = jsonSaveFileName;

    // Camera images/layers save next to whatever mPathDirectory currently points at (see
    // CameraManager::SaveCameraImage) - without updating it here it stays stuck at wherever
    // this tab was first constructed from (e.g. "." for a brand new, not-yet-saved path), so
    // "Save As" to a real location wouldn't actually redirect where camera images go, and a
    // freshly added camera's files would end up somewhere other than next to the saved json.
    mPathDirectory = QFileInfo(jsonSaveFileName).path();

    // No longer a temp file so don't force SaveAs next time
    if (mIsTempFile)
    {
        mIsTempFile = false;
        UpdateCleanState();
        UpdateTabTitle(true);
    }

    return true;
}

template<class ResultType>
static ResultType ExecASync(QString dialogTitle, std::function<ResultType()> fnDoWork)
{
    auto dlg = new ProgressDialog();
    dlg->setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::CustomizeWindowHint);
    dlg->setWindowTitle(dialogTitle);

    QFutureWatcher<ResultType>* watcher = new QFutureWatcher<ResultType>();
    QObject::connect(watcher, &QFutureWatcher<ResultType>::finished, [&]() { dlg->close(); });
    QFuture<ResultType> f = QtConcurrent::run(fnDoWork);

    watcher->setFuture(f);

    dlg->exec();

    return f.result();
}

bool EditorTab::DoSave(QString fileName)
{
    if (ExecASync<bool>("Saving... " + fileName, [&]()
        {
            std::string json = mModel->ToJson();
            QFile f(fileName);
            if (f.open(QIODevice::WriteOnly))
            {
                QTextStream stream(&f);
                stream << json.c_str();
                return true;
            }
            return false;
        }))
    {

        auto reliveIpc = relive::MakeIpcInterface();
        reliveIpc->SendLevelChanged(QDir::toNativeSeparators(fileName).toStdString());

        mUndoStack.setClean();
        mStatusBar->showMessage(tr("Saved"), 2000);
        return true;
    }
    else
    {
        QMessageBox::critical(this, "Error", "Failed to save " + fileName);
        mStatusBar->showMessage(tr("Save failed"));
        return false;
    }
}

void EditorTab::EditHintFlyMessages()
{
    auto pDlg = new MessageEditorDialog(this, this, *mModel, false);
    pDlg->exec();
    delete pDlg;
}

void EditorTab::EditLCDScreenMessages()
{
    auto pDlg = new MessageEditorDialog(this, this, *mModel, true);
    pDlg->exec();
    delete pDlg;
}

void EditorTab::EditPathData()
{
    auto pDlg = new PathDataEditorDialog(this, this);
    pDlg->exec();
    delete pDlg;
}

void EditorTab::EditMapSize()
{
    auto pDlg = new ChangeMapSizeDialog(this, this);
    pDlg->exec();
    delete pDlg;
}

void EditorTab::UpdateCleanState()
{
    emit CleanChanged();
}

void EditorTab::AddObject()
{
    auto pDlg = new AddObjectDialog(this, this);
    pDlg->exec();
    delete pDlg;
}

void EditorTab::AddCollision()
{
    mUndoStack.push(new AddCollisionCommand(this));
}

void EditorTab::ConnectCollisions()
{
    std::vector<ResizeableArrowItem*> collisions;
    for (auto& selectedItem : mScene->selectedItems())
    {
        auto asResizableArrowItem = qgraphicsitem_cast<ResizeableArrowItem*>(selectedItem);
        if (asResizableArrowItem != nullptr)
        {
            collisions.push_back(asResizableArrowItem);
        }
    }

    if (collisions.size() != 2)
    {
        QMessageBox::critical(this, tr("Error"), tr("Select exactly two collision lines to connect."));
        return;
    }

    mUndoStack.push(new CollisionConnectCommand(collisions[0], collisions[1]));
    mStatusBar->showMessage(tr("Connected collisions"));
}

int EditorTab::SnapX(bool enabled, int x)
{
    if (enabled)
    {
        if (mModel->Game() == GameType::eAo)
        {
            x = SnapToXGrid_AO(FP_FromInteger(1), x) + 13;
        }
        else
        {
            x = SnapToXGrid_AE(FP_FromInteger(1), x) + 13;
        }
    }
    return x;
}

int EditorTab::SnapY(bool enabled, int y)
{
    if (enabled)
    {
        const auto yGridSize = 20; // 260/13
        y = (y / yGridSize) * yGridSize;
    }
    return y;
}

int EditorTab::ClampX(int x)
{
    return GridPlacement::ClampPixelToMapBounds(x, mModel->XSize() * mModel->CameraGridWidth());
}

int EditorTab::ClampY(int y)
{
    return GridPlacement::ClampPixelToMapBounds(y, mModel->YSize() * mModel->CameraGridHeight());
}

int EditorTab::ClampRangeStartX(int start, int length)
{
    return GridPlacement::ClampRangeStartToMapBounds(start, length, mModel->XSize() * mModel->CameraGridWidth());
}

int EditorTab::ClampRangeStartY(int start, int length)
{
    return GridPlacement::ClampRangeStartToMapBounds(start, length, mModel->YSize() * mModel->CameraGridHeight());
}
