#pragma once

#include "../relive_lib/GameObjects/BaseAnimatedWithPhysicsGameObject.hpp"
#include "../relive_lib/SaveStateBase.hpp"

class SerializedObjectData;

namespace relive
{
    struct Path_LightEffect;
}

namespace AO {

struct DoorLightSaveState final : public SaveStateBase
{
    DoorLightSaveState()
        : SaveStateBase(ReliveTypes::eDoorLight, sizeof(*this))
    { }
    Guid mTlvId;
    s32 mSwitchState = 0;
    u16 mRed = 0;
    u16 mGreen = 0;
    u16 mBlue = 0;
};

class DoorLight final : public BaseAnimatedWithPhysicsGameObject
{
public:
    DoorLight(relive::Path_LightEffect* pTlv, const Guid& tlvId, ResourceManagerWrapper& resMan, BaseMap& map);
    ~DoorLight();

    virtual void VScreenChanged() override;
    virtual void VUpdate() override;
    virtual void VRender(OrderingTable& ot) override;
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pBuffer, ResourceManagerWrapper& resMan, BaseMap& map);

public:
    Guid mTlvId;
    s16 mWidth = 0;
    s16 mHeight = 0;
    bool mHasSwitchId = false;
    s32 mSwitchState = 0;
    s16 mSwitchId = 0;
};

} // namespace AO
