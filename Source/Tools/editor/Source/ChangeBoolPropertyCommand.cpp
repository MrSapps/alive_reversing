#include "ChangeBoolPropertyCommand.hpp"
#include "PropertyTreeWidget.hpp"
#include "PropertyTreeItemBase.hpp"
#include "IGraphicsItem.hpp"

void ChangeBoolPropertyCommand::undo()
{
    mLinkedProperty.mBoolValue = mPropertyData.mOldValue;
    mLinkedProperty.mTreeWidget->FindObjectPropertyByKey(&mLinkedProperty.mBoolValue)->Refresh();
    mLinkedProperty.mGraphicsItem->SyncInternalObject();
}

void ChangeBoolPropertyCommand::redo()
{
    mLinkedProperty.mBoolValue = !mPropertyData.mOldValue; // new value is inverse of the old one
    mLinkedProperty.mTreeWidget->FindObjectPropertyByKey(&mLinkedProperty.mBoolValue)->Refresh();
    mLinkedProperty.mGraphicsItem->SyncInternalObject();
}

void ChangeBoolPropertyCommand::UpdateText()
{
    setText(QString("Change property %1 from %2 to %3").arg(mLinkedProperty.mPropertyName, mPropertyData.mOldValue ? "true" : "false", !mPropertyData.mOldValue ? "true" : "false"));
}
