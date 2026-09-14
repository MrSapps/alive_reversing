#include "ChangeBasicTypePropertyCommand.hpp"
#include "PropertyTreeWidget.hpp"
#include "PropertyTreeItemBase.hpp"
#include "IGraphicsItem.hpp"
#include <QDateTime>
#include <memory>
#include "../../relive_lib/Types.hpp"

static void WriteInt(IntegerType intType, void* intPtr, qint64 value)
{
    switch (intType)
    {
        case IntegerType::Int_S16:
        *reinterpret_cast<s16*>(intPtr) = static_cast<s16>(value);
        break;

        case IntegerType::Int_U16:
        *reinterpret_cast<u16*>(intPtr) = static_cast<u16>(value);
        break;

        case IntegerType::Int_S32:
        *reinterpret_cast<s32*>(intPtr) = static_cast<s32>(value);
        break;

        case IntegerType::Int_U32:
        *reinterpret_cast<u32*>(intPtr) = static_cast<u32>(value);
        break;
    }
}

static qint64 ReadInt(IntegerType intType, const void* intPtr)
{
    switch (intType)
    {
        case IntegerType::Int_S16:
        return *reinterpret_cast<const s16*>(intPtr);

        case IntegerType::Int_U16:
        return *reinterpret_cast<const u16*>(intPtr);

        case IntegerType::Int_S32:
        return *reinterpret_cast<const s32*>(intPtr);

        case IntegerType::Int_U32:
        return *reinterpret_cast<const u32*>(intPtr);
    }
    return 0;
}

ChangeBasicTypePropertyCommand::ChangeBasicTypePropertyCommand(LinkedBasicTypeProperty linkedProperty, BasicTypePropertyChangeData propertyData)
    : mLinkedProperty(linkedProperty), mPropertyData(propertyData)
{
    UpdateText();
    mTimeStamp = QDateTime::currentMSecsSinceEpoch();
}

void ChangeBasicTypePropertyCommand::undo()
{
    WriteInt(mLinkedProperty.mIntegerType, mLinkedProperty.mIntPtr, mPropertyData.mOldValue);
    // SyncInternalObject (e.g. ResizeableRectItem::SyncFromMapObject) can silently correct the
    // raw value just written - clamping it to the map bounds/minimum size - so Refresh() must
    // run after it, not before, or the property row (and a still-open spin box's own internal
    // value/displayed text) is left showing the pre-correction number rather than what's
    // actually stored. That divergence compounds with every further step of a spin box's
    // up/down arrows, since each one computes its next value from its own (increasingly stale)
    // displayed number rather than the model's real, corrected one.
    mLinkedProperty.mGraphicsItem->SyncInternalObject();

    // Record what's actually stored now, not the raw value that was requested - the same
    // correction above means they can differ (e.g. undoing back to a value that's since become
    // invalid because the map shrank). Without this, mOldValue drifts from reality exactly like
    // mSpinBox's own displayed value used to, and the undo entry's "from X to Y" text (and any
    // later mergeWith bookkeeping built on it) keeps describing an edit that never actually
    // happened.
    mPropertyData.mOldValue = ReadInt(mLinkedProperty.mIntegerType, mLinkedProperty.mIntPtr);
    mLinkedProperty.mTreeWidget->FindObjectPropertyByKey(mLinkedProperty.mIntPtr)->Refresh();
    UpdateText();
}

void ChangeBasicTypePropertyCommand::redo()
{
    WriteInt(mLinkedProperty.mIntegerType, mLinkedProperty.mIntPtr, mPropertyData.mNewValue);
    mLinkedProperty.mGraphicsItem->SyncInternalObject();
    mPropertyData.mNewValue = ReadInt(mLinkedProperty.mIntegerType, mLinkedProperty.mIntPtr);
    mLinkedProperty.mTreeWidget->FindObjectPropertyByKey(mLinkedProperty.mIntPtr)->Refresh();
    UpdateText();
}

bool ChangeBasicTypePropertyCommand::mergeWith(const QUndoCommand* command)
{
    if (command->id() == id())
    {
        auto pOther = static_cast<const ChangeBasicTypePropertyCommand*>(command);
        if (mLinkedProperty.mIntPtr == pOther->mLinkedProperty.mIntPtr)
        {
            // Compare time stamps and only merge if dt <= 1 second
            if (abs(mTimeStamp - pOther->mTimeStamp) <= 1000)
            {
                mPropertyData.mNewValue = pOther->mPropertyData.mNewValue;
                mTimeStamp = QDateTime::currentMSecsSinceEpoch();
                UpdateText();
                return true;
            }
        }
    }
    return false;
}

void ChangeBasicTypePropertyCommand::PushIfEffective(QUndoStack& stack, LinkedBasicTypeProperty linkedProperty, BasicTypePropertyChangeData propertyData)
{
    auto cmd = std::make_unique<ChangeBasicTypePropertyCommand>(linkedProperty, propertyData);

    // Run the edit now (exactly what QUndoStack::push() would do immediately anyway) so
    // mPropertyData.mNewValue reflects the actual, possibly-corrected result - only then do we
    // know whether this is worth an undo-stack entry at all.
    cmd->redo();

    if (cmd->mPropertyData.mNewValue == cmd->mPropertyData.mOldValue)
    {
        // Fully absorbed by a correction - the redo() above already reapplied it and refreshed
        // the display, so the property is exactly where it started and there's nothing to undo.
        return;
    }

    // push() calls redo() again - harmless, since it just re-applies the same already-current
    // value a second time - but is what actually registers the command with the stack/QUndoView.
    stack.push(cmd.release());
}

void ChangeBasicTypePropertyCommand::UpdateText()
{
    setText(QString("Change property %1 from %2 to %3").arg(mLinkedProperty.mPropertyName, QString::number(mPropertyData.mOldValue), QString::number(mPropertyData.mNewValue)));
}
