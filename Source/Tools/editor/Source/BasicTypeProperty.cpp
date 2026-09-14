#include "BasicTypeProperty.hpp"
#include "ChangeBasicTypePropertyCommand.hpp"
#include "IGraphicsItem.hpp"
#include <QDateTime>
#include "BigSpinBox.hpp"
#include "../../relive_lib/Types.hpp"

static qint64 ReadInt(IntegerType intType, void* intPtr)
{
    switch (intType)
    {
        case IntegerType::Int_S16:
        return *reinterpret_cast<s16*>(intPtr);

        case IntegerType::Int_U16:
        return *reinterpret_cast<u16*>(intPtr);

        case IntegerType::Int_S32:
        return *reinterpret_cast<s32*>(intPtr);

        case IntegerType::Int_U32:
        return *reinterpret_cast<u32*>(intPtr);
    }

    return 0; // rip
}

BasicTypeProperty::BasicTypeProperty(IntegerType intType, void* pInteger, const char* pPropertyName, QUndoStack& undoStack, IGraphicsItem* pGraphicsItem)
    : PropertyTreeItemBase()
    , mIntType(intType)
    , mIntegerPtr(pInteger)
    , mPropertyName(pPropertyName)
    , mUndoStack(undoStack)
    , mGraphicsItem(pGraphicsItem)
{
    setText(0, mPropertyName); // TODO: Add text indent
    this->Refresh();
}

QWidget* BasicTypeProperty::GetEditorWidget(PropertyTreeWidget* pParent)
{
    mSpinBox = new BigSpinBox(pParent);
    // Named so automation can reach it (see the "click_tree_item"/"set_value" automation
    // commands) - this widget only exists transiently once the user clicks into this property's
    // row, so it has no fixed identity otherwise.
    mSpinBox->setObjectName(QString("propertyEditor_%1").arg(mPropertyName));


    switch(mIntType)
    {
        case IntegerType::Int_S16:
            mSpinBox->setMax(std::numeric_limits<s16>::max());
            mSpinBox->setMin(std::numeric_limits<s16>::min());
            break;

        case IntegerType::Int_U16:
            mSpinBox->setMax(std::numeric_limits<u16>::max());
            mSpinBox->setMin(std::numeric_limits<u16>::min());
            break;

        case IntegerType::Int_S32:
            mSpinBox->setMax(std::numeric_limits<s32>::max());
            mSpinBox->setMin(std::numeric_limits<s32>::min());
            break;

        case IntegerType::Int_U32:
            mSpinBox->setMax(std::numeric_limits<u32>::max());
            mSpinBox->setMin(std::numeric_limits<u32>::min());
            break;
    }
    mSpinBox->setValue(ReadInt(mIntType, mIntegerPtr));

    mOldValue = ReadInt(mIntType, mIntegerPtr);

    connect(mSpinBox, &BigSpinBox::valueChanged, this, [pParent, this](qint64 newValue, bool closeEditor)
        {
            if (mOldValue != newValue)
            {
                // PushIfEffective's redo() (run whether or not it ends up pushing) refreshes
                // this property via Refresh(), which already resyncs mOldValue to whatever is
                // actually stored - do not also set it to the raw requested newValue afterward:
                // that would clobber the corrected value with the pre-correction one whenever
                // they differ (e.g. clamped), leaving mOldValue stale. The *next* edit would
                // then compare against that stale number instead of reality - wrongly treating
                // an actual no-op as a real change (spuriously pushing a "from stale to
                // corrected" undo entry) while also leaving BigSpinBox's own already-corrected
                // display with nothing to re-sync it if a later comparison instead wrongly
                // treats a real change as a no-op and skips the correction entirely.
                ChangeBasicTypePropertyCommand::PushIfEffective(mUndoStack,
                    LinkedBasicTypeProperty(this->mPropertyName, this->mIntType, this->mIntegerPtr, pParent, this->mGraphicsItem),
                    BasicTypePropertyChangeData(this->mIntType, this->mIntegerPtr, this->mOldValue, newValue));
            }

            if (closeEditor)
            {
                pParent->setItemWidget(this, 1, nullptr);
            }
        });

    connect(mSpinBox, &BigSpinBox::destroyed, this, [this](QObject*)
        {
            this->mSpinBox = nullptr;
        });

    return mSpinBox;
}

void BasicTypeProperty::Refresh()
{
    // Also resync mOldValue - not just the displayed text/spin box - to whatever the model
    // actually holds now. Refresh() runs after ChangeBasicTypePropertyCommand::redo()/undo(),
    // which may have corrected the raw value it just wrote (e.g. ResizeableRectItem::
    // SyncFromMapObject clamping an out-of-bounds size/position); mOldValue is what the spin
    // box's valueChanged lambda compares the *next* edit against to decide whether anything
    // changed, so leaving it at the pre-correction value would make one subsequent step/edit
    // that happens to land back on that stale number silently produce no command at all -
    // letting the displayed value drift out of sync with the model a little further with every
    // such step, same bug this whole reorder exists to fix.
    const qint64 currentValue = ReadInt(mIntType, mIntegerPtr);
    setText(1, QString::number(currentValue));

    if (mSpinBox)
    {
        mSpinBox->setValue(currentValue, false);
    }

    mOldValue = currentValue;
}
