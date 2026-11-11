#pragma once

#include "../relive_lib/GameObjects/BaseAnimatedWithPhysicsGameObject.hpp"
#include "../relive_lib/SaveStateBase.hpp"

namespace relive
{
    struct Path_LightEffect;
}

namespace AO {

struct LightEffectSaveState final : public SaveStateBase
{
    LightEffectSaveState()
        : SaveStateBase(ReliveTypes::eLightEffect, sizeof(*this))
    { }
    s32 mRnd1;
    s32 mRnd2;
    s32 mRnd3;
    s32 mRnd4;
};

class LightEffect final : public BaseAnimatedWithPhysicsGameObject
{
public:
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void RestoreFromSaveState(const LightEffectSaveState& state);

    LightEffect(relive::Path_LightEffect* pTlv, const Guid& tlvId, ResourceManager& resMan, BaseMap& map);
    ~LightEffect();

    virtual void VRender(OrderingTable& ot) override;
    virtual void VScreenChanged() override;
    virtual void VUpdate() override;

    Guid mTlvId;
    s32 mRnd1 = 0;
    s32 mRnd2 = 0;
    s32 mRnd3 = 0;
    s32 mRnd4 = 0;
};

} // namespace AO
