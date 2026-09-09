#include "ChangeBasicTypePropertyCommand.hpp"
#include "PropertyTreeWidget.hpp"
#include "PropertyTreeItemBase.hpp"
#include "IGraphicsItem.hpp"
#include <QDateTime>
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

ChangeBasicTypePropertyCommand::ChangeBasicTypePropertyCommand(LinkedBasicTypeProperty linkedProperty, BasicTypePropertyChangeData propertyData)
    : mLinkedProperty(linkedProperty), mPropertyData(propertyData)
{
    UpdateText();
    mTimeStamp = QDateTime::currentMSecsSinceEpoch();
}

void ChangeBasicTypePropertyCommand::undo()
{
    WriteInt(mLinkedProperty.mIntegerType, mLinkedProperty.mIntPtr, mPropertyData.mOldValue);
    mLinkedProperty.mTreeWidget->FindObjectPropertyByKey(mLinkedProperty.mIntPtr)->Refresh();
    mLinkedProperty.mGraphicsItem->SyncInternalObject();
}

void ChangeBasicTypePropertyCommand::redo()
{
    WriteInt(mLinkedProperty.mIntegerType, mLinkedProperty.mIntPtr, mPropertyData.mNewValue);
    mLinkedProperty.mTreeWidget->FindObjectPropertyByKey(mLinkedProperty.mIntPtr)->Refresh();
    mLinkedProperty.mGraphicsItem->SyncInternalObject();

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

void ChangeBasicTypePropertyCommand::UpdateText()
{
    setText(QString("Change property %1 from %2 to %3").arg(mLinkedProperty.mPropertyName, QString::number(mPropertyData.mOldValue), QString::number(mPropertyData.mNewValue)));
}
