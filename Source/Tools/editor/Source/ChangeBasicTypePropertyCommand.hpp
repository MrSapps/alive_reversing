#pragma once

#include <QUndoCommand>
#include <QtGlobal>
#include "BasicTypePropertyChangeData.hpp"
#include "LinkedBasicTypeProperty.hpp"

class ChangeBasicTypePropertyCommand final : public QUndoCommand
{
public:
    ChangeBasicTypePropertyCommand(LinkedBasicTypeProperty linkedProperty, BasicTypePropertyChangeData propertyData);

    void undo() override;

    void redo() override;

    int id() const override
    {
        return 1;
    }

    bool mergeWith(const QUndoCommand* command) override;

private:
    void UpdateText();
    LinkedBasicTypeProperty mLinkedProperty;
    BasicTypePropertyChangeData mPropertyData;
    qint64 mTimeStamp = 0;
};
