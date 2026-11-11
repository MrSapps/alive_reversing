#pragma once

#include "../relive_lib/GameObjects/BaseAnimatedWithPhysicsGameObject.hpp"
#include "../relive_lib/SaveStateBase.hpp"

class SerializedObjectData;

namespace relive
{
    struct Path_ZBall;
}

namespace AO {

class ZBall final : public BaseAnimatedWithPhysicsGameObject
{
public:
    ZBall(relive::Path_ZBall* pTlv, const Guid& tlvId, ResourceManager& resMan, BaseMap& map);

    virtual void VUpdate() override;

    Guid mTlvInfo;
    s16 mFrameAbove12 = 0;
    s16 mSoundPitch = 0;

    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pBuffer, ResourceManager& resMan, BaseMap& map);
};

// mSoundPitch is derived once from the TLV/level at construction and never changes
struct ZBallSaveState final : public SaveStateBase
{
    ZBallSaveState()
        : SaveStateBase(ReliveTypes::eZBall, sizeof(*this))
    { }
    Guid mTlvInfo;
    s32 mFrame = 0;
    s16 mFrameAbove12 = 0;
};

} // namespace AO
