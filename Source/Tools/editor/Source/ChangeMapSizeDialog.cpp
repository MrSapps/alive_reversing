#include "ChangeMapSizeDialog.hpp"
#include "ui_ChangeMapSizeDialog.h"
#include "EditorTab.hpp"
#include <QUndoCommand>
#include "EditorGraphicsScene.hpp"
#include "SelectionSaver.hpp"
#include "ResizeableRectItem.hpp"
#include "ResizeableArrowItem.hpp"
#include "CameraGraphicsItem.hpp"
#include "ItemPositionData.hpp"
#include "GridPlacement.hpp"
#include "Model.hpp"

struct RemovedCamera final
{
    // Item we've removed from the model
     std::unique_ptr<EditorCamera> mCameraModel;

    // The graphics item for this camera
    CameraGraphicsItem* mCameraGraphicsItem = nullptr;

    // All of the graphics items for this cameras map object
    QList<ResizeableRectItem*> mGraphicsItemMapObjects;

    RemovedCamera(CameraGraphicsItem* pCameraGraphicsItem, EditorGraphicsScene* pScene)
        : mCameraGraphicsItem(pCameraGraphicsItem)
    {
        // Collect the map object graphics items for this camera
        mGraphicsItemMapObjects = pScene->MapObjectsForCamera(mCameraGraphicsItem);
    }

    ~RemovedCamera()
    {
        if (mCameraModel)
        {
            delete mCameraGraphicsItem;
            for (auto& mapObj : mGraphicsItemMapObjects)
            {
                delete mapObj;
            }
        }
    }

    void redo(Model& model, EditorGraphicsScene* pScene)
    {
        // Remove it from the model now we have the model item
        mCameraModel = model.RemoveCamera(mCameraGraphicsItem->GetCamera());

        // Remove graphics items from the scene
        pScene->removeItem(mCameraGraphicsItem);
        for (auto& mapObject : mGraphicsItemMapObjects)
        {
            pScene->removeItem(mapObject);
        }
    }

    void undo(Model& model, EditorGraphicsScene* pScene)
    {
        // Add camera back to the model
        model.AddCamera(std::move(mCameraModel));

        // Add graphics items to the scene
        pScene->addItem(mCameraGraphicsItem);
        for (auto& mapObject : mGraphicsItemMapObjects)
        {
            pScene->addItem(mapObject);
        }
    }
};

struct AddedCamera final
{
    // New camera
    CameraGraphicsItem* mCameraGraphicsItem = nullptr;

    // New model item
    std::unique_ptr<EditorCamera> mCameraModel;

    AddedCamera(EditorTab* pTab, int x, int y)
    {
        mCameraModel = std::make_unique<EditorCamera>();
        mCameraModel->mX = x;
        mCameraModel->mY = y;
        const auto& model = pTab->GetModel();
        mCameraGraphicsItem = pTab->MakeCameraGraphicsItem(mCameraModel.get(), model.CameraGridWidth() * x, y * model.CameraGridHeight(), model.CameraGridWidth(), model.CameraGridHeight());
    }

    ~AddedCamera()
    {
        if (mCameraModel)
        {
            delete mCameraGraphicsItem;
        }
    }

    void undo(Model& model, EditorGraphicsScene* pScene)
    {
        mCameraModel = model.RemoveCamera(mCameraGraphicsItem->GetCamera());
        pScene->removeItem(mCameraGraphicsItem);
    }

    void redo(Model& model, EditorGraphicsScene* pScene)
    {
        model.AddCamera(std::move(mCameraModel));
        pScene->addItem(mCameraGraphicsItem);
    }
};

// Forces every surviving map object and collision line to fit within the map's current
// (already-resized) pixel bounds: shrinks an object/line's own size first if it's now bigger
// than the whole map (e.g. shrinking the grid down to a single camera), then repositions it -
// same idea as a drag/resize/paste clamp, just applied to everything in the scene at once
// rather than one item. Cameras themselves need no such handling: CalcMapChanges only ever
// removes/adds whole grid cells at the edge, it never repositions a surviving one.
static void ForceItemsInsideMapBounds(EditorTab* pTab)
{
    const Model& model = pTab->GetModel();
    const unsigned int mapWidth = model.CameraGridWidth() * model.XSize();
    const unsigned int mapHeight = model.CameraGridHeight() * model.YSize();

    for (QGraphicsItem* item : pTab->GetScene().items())
    {
        if (auto* pRect = qgraphicsitem_cast<ResizeableRectItem*>(item))
        {
            const QRectF rect = pRect->CurrentRect();
            const int width = GridPlacement::ClampLengthToMapBounds(static_cast<int>(rect.width()), mapWidth);
            const int height = GridPlacement::ClampLengthToMapBounds(static_cast<int>(rect.height()), mapHeight);
            const int x = GridPlacement::ClampRangeStartToMapBounds(static_cast<int>(rect.x()), width, mapWidth);
            const int y = GridPlacement::ClampRangeStartToMapBounds(static_cast<int>(rect.y()), height, mapHeight);
            if (x != rect.x() || y != rect.y() || width != rect.width() || height != rect.height())
            {
                pRect->SetRect(QRectF(x, y, width, height));
            }
        }
        else if (auto* pArrow = qgraphicsitem_cast<ResizeableArrowItem*>(item))
        {
            // Same shrink-then-reposition as the rect branch above, via the same GridPlacement
            // primitives (FitLineToMapBounds composes them for a line - see its own comment in
            // GridPlacement.hpp) - a line just has no independent width/height to write the
            // shrunk extent back to, only two endpoints. ItemPositionData::Save/Restore (used by
            // mBeforeResizeSnapshot below) captures a line's full local line() shape, not just
            // its position, so undo restores an oversized line's exact pre-shrink shape even
            // though this shrinks it here.
            const QLineF line = pArrow->SaveLine().translated(pArrow->x(), pArrow->y());
            const int x1 = static_cast<int>(line.x1());
            const int y1 = static_cast<int>(line.y1());
            const int x2 = static_cast<int>(line.x2());
            const int y2 = static_cast<int>(line.y2());

            const GridPlacement::ClampedLine fitted = GridPlacement::FitLineToMapBounds(x1, y1, x2, y2, mapWidth, mapHeight);
            if (fitted.x1 != x1 || fitted.y1 != y1 || fitted.x2 != x2 || fitted.y2 != y2)
            {
                pArrow->setPos(QPointF());
                pArrow->RestoreLine(QLineF(fitted.x1, fitted.y1, fitted.x2, fitted.y2));
            }
        }
    }
}

class ChangeMapSizeCommand final : public QUndoCommand
{
public:
    ChangeMapSizeCommand(EditorTab* pTab, int newXSize, int newYSize)
      : mTab(pTab),
        mSelectionSaver(pTab),
        mNewXSize(newXSize),
        mNewYSize(newYSize)
    {
        mOldXSize = mTab->GetModel().XSize();
        mOldYSize = mTab->GetModel().YSize();

        setText("Change map size from " +
            QString::number(mOldXSize) + "x" + QString::number(mOldYSize) + " to " +
            QString::number(mNewXSize) + "x" + QString::number(mNewYSize));

        auto edits = GridPlacement::CalcMapChanges(mOldXSize, mNewXSize, mOldYSize, mNewYSize);

        for (auto& edit : edits)
        {
            if (!edit.mAdd)
            {
                CameraGraphicsItem* pCam = mTab->GetScene().CameraAt(edit.x, edit.y);
                mRemovedCameras.emplace_back(std::make_unique<RemovedCamera>(pCam, &mTab->GetScene()));
            }
            else
            {
                mAddedCameras.emplace_back(std::make_unique<AddedCamera>(mTab, edit.x, edit.y));
            }
        }

        // Snapshot every surviving item's pre-resize position/size, so undo can put anything
        // ForceItemsInsideMapBounds touched in redo() back exactly where/how big it was -
        // restoring the old map size alone isn't enough once an oversized object has been
        // shrunk to fit. Capturing everything currently in the scene is simplest; restoring an
        // item that redo() never actually touched is just a harmless no-op.
        QList<QGraphicsItem*> allItems = mTab->GetScene().items();
        mBeforeResizeSnapshot.Save(allItems, mTab->GetModel(), false);
    }

    void undo() override
    {
        mTab->GetModel().SetXSize(mOldXSize);
        mTab->GetModel().SetYSize(mOldYSize);

        // Fix scene rect
        mTab->GetScene().UpdateSceneRect();

        // Add back removed cameras and objects
        for (auto& removed : mRemovedCameras)
        {
            removed->undo(mTab->GetModel(), &mTab->GetScene());
        }

        // Remove newly added cameras
        for (auto& added : mAddedCameras)
        {
            added->undo(mTab->GetModel(), &mTab->GetScene());
        }

        // Put back exactly where/how big everything was before redo()'s bounds enforcement.
        mBeforeResizeSnapshot.Restore(mTab->GetModel());

        mSelectionSaver.undo();
    }

    void redo() override
    {
        mSelectionSaver.redo();

        mTab->GetModel().SetXSize(mNewXSize);
        mTab->GetModel().SetYSize(mNewYSize);

        // Fix scene rect
        mTab->GetScene().UpdateSceneRect();

        // Remove cameras and objects
        for (auto& removed : mRemovedCameras)
        {
            removed->redo(mTab->GetModel(), &mTab->GetScene());
        }

        // Add new cameras
        for (auto& added : mAddedCameras)
        {
            added->redo(mTab->GetModel(), &mTab->GetScene());
        }

        // Shrink map got smaller than some object/line, or growing exposed an object that
        // was previously extending past the map edge and only got away with it because
        // there simply wasn't any code enforcing it - either way, force everything remaining
        // back within the new bounds.
        ForceItemsInsideMapBounds(mTab);
    }

private:
    EditorTab* mTab = nullptr;

    SelectionSaver mSelectionSaver;

    int mOldXSize = 0;
    int mOldYSize = 0;

    int mNewXSize = 0;
    int mNewYSize = 0;

    std::vector<std::unique_ptr<RemovedCamera>> mRemovedCameras;
    std::vector<std::unique_ptr<AddedCamera>> mAddedCameras;
    ItemPositionData mBeforeResizeSnapshot;
};

ChangeMapSizeDialog::ChangeMapSizeDialog(QWidget *parent, EditorTab* pTab) :
    QDialog(parent, Qt::WindowMaximizeButtonHint | Qt::WindowCloseButtonHint),
    ui(new Ui::ChangeMapSizeDialog),
    mTab(pTab)
{
    ui->setupUi(this);

    ui->spnXSize->setMinimum(1);
    ui->spnYSize->setMinimum(1);

    ui->spnXSize->setValue(mTab->GetModel().XSize());
    ui->spnYSize->setValue(mTab->GetModel().YSize());
}

ChangeMapSizeDialog::~ChangeMapSizeDialog()
{
    delete ui;
}

void ChangeMapSizeDialog::on_buttonBox_accepted()
{
    if (static_cast<int>(mTab->GetModel().XSize()) != ui->spnXSize->value() ||
        static_cast<int>(mTab->GetModel().YSize()) != ui->spnYSize->value())
    {
        mTab->AddCommand(new ChangeMapSizeCommand(mTab, ui->spnXSize->value(), ui->spnYSize->value()));
    }
}
