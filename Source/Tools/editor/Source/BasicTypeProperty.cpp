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
                mUndoStack.push(new ChangeBasicTypePropertyCommand(
                    LinkedBasicTypeProperty(this->mPropertyName, this->mIntType, this->mIntegerPtr, pParent, this->mGraphicsItem),
                    BasicTypePropertyChangeData(this->mIntType, this->mIntegerPtr, this->mOldValue, newValue)));
            }

            mOldValue = newValue;

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
    setText(1, QString::number(ReadInt(mIntType, mIntegerPtr)));

    if (mSpinBox)
    {
        mSpinBox->setValue(ReadInt(mIntType, mIntegerPtr), false);
    }
}
