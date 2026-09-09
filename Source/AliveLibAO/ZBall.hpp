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
    ZBall(relive::Path_ZBall* pTlv, const Guid& tlvId, ResourceManagerWrapper& resMan, BaseMap& map);

    virtual void VUpdate() override;

    Guid mTlvInfo;
    s16 mFrameAbove12 = 0;
    s16 mSoundPitch = 0;

    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pBuffer, ResourceManagerWrapper& resMan, BaseMap& map);
};

// mSoundPitch is derived once from the TLV/level at construction and never
// changes; mFrameAbove12 is recomputed every VUpdate() from the current
// animation frame. The only thing worth persisting is the swing's current
// animation frame, so the pendulum doesn't visually snap back to its
// default start position after a reload.
struct ZBallSaveState final : public SaveStateBase
{
    ZBallSaveState()
        : SaveStateBase(ReliveTypes::eZBall, sizeof(*this))
    { }
    Guid mTlvInfo;
    s32 mFrame = 0;
};

} // namespace AO
