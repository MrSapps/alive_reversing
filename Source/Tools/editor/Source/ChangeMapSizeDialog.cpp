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
#include <QDebug>
#include "Model.hpp"
#include <algorithm>
#include <cmath>

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

struct CamToEdit final
{
    int x = 0;
    int y = 0;
    bool mAdd = false;

    bool operator == (const CamToEdit& rhs) const
    {
        return x == rhs.x && y == rhs.y && mAdd == rhs.mAdd;
    }
};

static void Add(std::vector<CamToEdit>& edits, int x, int y, bool add)
{
    for (auto& edit : edits)
    {
        if (edit.x == x && edit.y == y)
        {
            edit.mAdd = add;
            return;
        }
    }
    edits.push_back(CamToEdit{ x, y, add });
}

static std::vector<CamToEdit> CalcMapChanges(int oldW, int newW, int oldH, int newH)
{
    // Calculate coords to remove or add cameras to
    const int maxX = std::max(oldW, newW);
    const int maxY = std::max(oldH, newH);

    //const int minX = std::min(oldW, newW);
    //const int minY = std::min(oldH, newH);

    std::vector<CamToEdit> edits;
    for (int x = 0; x < maxX; x++)
    {
        for (int y = 0; y < maxY; y++)
        {
            if (x >= oldW)
            {
                // add
                Add(edits, x, y, true);
            }
            else if (x >= newW)
            {
                // remove
                Add(edits, x, y, false);
            }

            if (y >= oldH)
            {
                // add
                Add(edits, x, y, true);
            }
            else if (y >= newH)
            {
                // remove
                Add(edits, x, y, false);
            }
        }
    }

    for (auto& edit : edits)
    {
        qDebug() << edit.x << ", " << edit.y << (edit.mAdd ? " remove" : " add");
    }

    return edits;
}

static bool Equal(const std::vector<CamToEdit>& lhs, const std::vector<CamToEdit>& rhs)
{
    if (lhs.size() != rhs.size())
    {
        return false;
    }

    for (const auto& lhsItem : lhs)
    {
        bool found = false;
        for (const auto& rhsItem : rhs)
        {
            if (rhsItem == lhsItem)
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            return false;
        }
    }

    return true;
}

void Test_RemoveCoulmn()
{
    auto edits = CalcMapChanges(2, 1, 2, 2);
    std::vector<CamToEdit> expected = { CamToEdit{1, 0, false}, CamToEdit{1, 1, false} };
    if (!Equal(edits, expected))
    {
        abort();
    }
}

void Test_AddCoulmn()
{
    auto edits = CalcMapChanges(2, 3, 2, 2);
    std::vector<CamToEdit> expected = { CamToEdit{2, 0, true}, CamToEdit{2, 1, true} };
    if (!Equal(edits, expected))
    {
        abort();
    }
}

void Test_RemoveRow()
{
    auto edits = CalcMapChanges(2, 2, 2, 1);
    std::vector<CamToEdit> expected = { CamToEdit{0, 1, false}, CamToEdit{1, 1, false} };
    if (!Equal(edits, expected))
    {
        abort();
    }
}

void Test_AddRow()
{
    auto edits = CalcMapChanges(2, 2, 2, 3);
    std::vector<CamToEdit> expected = { CamToEdit{0, 2, true}, CamToEdit{1, 2, true} };
    if (!Equal(edits, expected))
    {
        abort();
    }
}

void Test_RemoveCoulmn_AddRow()
{
    auto edits = CalcMapChanges(2, 1, 2, 3);
    std::vector<CamToEdit> expected = { 
        CamToEdit{1, 0, false}, CamToEdit{1, 1, false},
        CamToEdit{0, 2, true}, CamToEdit{1, 2, true} };

    if (!Equal(edits, expected))
    {
        abort();
    }
}

void Test_AddCoulmn_AddRow()
{
    auto edits = CalcMapChanges(2, 3, 2, 3);
    std::vector<CamToEdit> expected = { 
        CamToEdit{2, 0, true}, CamToEdit{2, 1, true},
        CamToEdit{0, 2, true}, CamToEdit{1, 2, true}, 
        CamToEdit{2, 2, true} };
    if (!Equal(edits, expected))
    {
        abort();
    }
}

void Test_RemoveCoulmn_RemoveRow()
{
    auto edits = CalcMapChanges(2, 1, 2, 1);
    std::vector<CamToEdit> expected = { 
        CamToEdit{1, 0, false}, CamToEdit{1, 1, false},
        CamToEdit{0, 1, false}, };
    if (!Equal(edits, expected))
    {
        abort();
    }
}
void DoMapSizeTests()
{
    Test_RemoveCoulmn();
    Test_AddCoulmn();
    Test_RemoveRow();
    Test_AddRow();
    Test_RemoveCoulmn_AddRow();
    Test_AddCoulmn_AddRow();
    Test_RemoveCoulmn_RemoveRow();
}

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
            const QLineF line = pArrow->SaveLine().translated(pArrow->x(), pArrow->y());
            const qreal left = std::min(line.x1(), line.x2());
            const qreal top = std::min(line.y1(), line.y2());
            const qreal width = std::abs(line.x2() - line.x1());
            const qreal height = std::abs(line.y2() - line.y1());
            const int clampedLeft = GridPlacement::ClampRangeStartToMapBounds(static_cast<int>(left), static_cast<int>(width), mapWidth);
            const int clampedTop = GridPlacement::ClampRangeStartToMapBounds(static_cast<int>(top), static_cast<int>(height), mapHeight);
            const int dx = clampedLeft - static_cast<int>(left);
            const int dy = clampedTop - static_cast<int>(top);
            if (dx != 0 || dy != 0)
            {
                pArrow->Translate(dx, dy);
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

        auto edits = CalcMapChanges(mOldXSize, mNewXSize, mOldYSize, mNewYSize);

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
