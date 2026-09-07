#pragma once

#include "../relive_lib/GameObjects/BaseGameObject.hpp"
#include "../relive_lib/SaveStateBase.hpp"

struct MinesAlarmSaveState final : public SaveStateBase
{
    MinesAlarmSaveState()
        : SaveStateBase(ReliveTypes::eMinesAlarm, sizeof(*this))
    { }
    s32 mExplosionTimer;
};

class MinesAlarm final : public BaseGameObject
{
public:
    static void Create(s32 timer, ResourceManagerWrapper& resMan);
    static void CreateFromSaveState(SerializedObjectData& pBuffer, ResourceManagerWrapper& resMan);
    MinesAlarm(s32 timer, ResourceManagerWrapper& resMan);
    ~MinesAlarm();

    virtual void VScreenChanged() override;
    virtual void VUpdate() override;
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
};

extern s32 gExplosionTimer;
