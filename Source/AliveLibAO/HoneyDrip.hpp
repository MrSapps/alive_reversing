#pragma once

#include "../relive_lib/GameObjects/BaseAnimatedWithPhysicsGameObject.hpp"

namespace AO {

class HoneyDrip final : public BaseAnimatedWithPhysicsGameObject
{
public:
    HoneyDrip(FP xpos, FP ypos, ResourceManager& resMan, BaseMap& map);

    virtual void VUpdate() override;

    bool mSoundPlaying = false;
};

} // namespace AO
