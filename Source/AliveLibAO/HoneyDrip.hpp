#pragma once

#include "../relive_lib/GameObjects/BaseAnimatedWithPhysicsGameObject.hpp"
#include "../relive_lib/SaveStateBase.hpp"

namespace AO {

struct HoneyDripSaveState final : public SaveStateBase
{
    HoneyDripSaveState()
        : SaveStateBase(ReliveTypes::eHoneyDrip, sizeof(*this))
    { }
    bool mSoundPlaying;
};

class HoneyDrip final : public BaseAnimatedWithPhysicsGameObject
{
public:
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pData, ResourceManager& resMan, BaseMap& map);

    HoneyDrip(FP xpos, FP ypos, ResourceManager& resMan, BaseMap& map);

    virtual void VUpdate() override;

    bool mSoundPlaying = false;
};

} // namespace AO
