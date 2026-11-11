#pragma once

#include "../relive_lib/GameObjects/BaseAnimatedWithPhysicsGameObject.hpp"

#include "../relive_lib/SaveStateBase.hpp"

namespace relive
{
    struct Path_StatusLight;
}

struct StatusLightSaveState final : public SaveStateBase
{
    StatusLightSaveState()
        : SaveStateBase(ReliveTypes::eStatusLight, sizeof(*this))
    { }
    FP mUntweakedXPos;
    FP mUntweakedYPos;
};

class StatusLight final : public ::BaseAnimatedWithPhysicsGameObject
{
public:
    StatusLight(relive::Path_StatusLight* pTlv, const Guid& tlvId, ResourceManager& resMan, BaseMap& map);
    ~StatusLight();

    void LoadAnimations();

    virtual void VUpdate() override;
    virtual void VScreenChanged() override;
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void RestoreFromSaveState(const StatusLightSaveState& state);

    bool isLinkedAndDisabled(s16 switchId);

private:
    Guid mTlvId;
    s16 mInputSwitchId = 0;
    s16 mLinkedStatusLightSwitchId1 = 0;
    s16 mLinkedStatusLightSwitchId2 = 0;
    s16 mLinkedStatusLightSwitchId3 = 0;
    s16 mLinkedStatusLightSwitchId4 = 0;
    s16 mLinkedStatusLightSwitchId5 = 0;
    bool mIgnoreGridSnapping = false;
    FP mUntweakedXPos = {};
    FP mUntweakedYPos = {};
};
