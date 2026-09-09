#include "BoolProperty.hpp"
#include "ChangeBoolPropertyCommand.hpp"
#include "IGraphicsItem.hpp"
#include <QDateTime>

BoolProperty::BoolProperty(PropertyTreeWidget* pParent, bool& boolProperty, const char* pPropertyName, QUndoStack& undoStack, IGraphicsItem* pGraphicsItem)
    : PropertyTreeItemBase()
    , mBoolValue(boolProperty)
    , mPropertyName(pPropertyName)
    , mUndoStack(undoStack)
    , mGraphicsItem(pGraphicsItem)
{
    // Save the value before the user changes it
    mOldValue = mBoolValue;

    mCheckBox = new QCheckBox(pParent);
    setText(0, mPropertyName);

    // Sync checkbox to correct state
    Refresh();

    connect(mCheckBox, &QCheckBox::toggled, this, [pParent, this](bool checked)
        {
            const bool newValue = checked;
            if (mOldValue != newValue)
            {
                mUndoStack.push(new ChangeBoolPropertyCommand(
                    LinkedBoolProperty(this->mPropertyName, this->mBoolValue, pParent, this->mGraphicsItem),
                    BoolPropertyChangeData(this->mBoolValue, mOldValue)));
            }

            mOldValue = newValue;
        });
}

QWidget* BoolProperty::GetEditorWidget(PropertyTreeWidget* /*pParent*/)
{
    return nullptr;
}

QWidget* BoolProperty::GetPersistentEditorWidget(PropertyTreeWidget* /*pParent*/)
{
    return mCheckBox;
}

void BoolProperty::Refresh()
{
    // Don't fire a signal when we set the value else it will trigger another undo action
    // which will call Refresh which will trigger another undo action which... you get the point.
    const QSignalBlocker blocker(mCheckBox);
    mCheckBox->setChecked(mBoolValue);
}
