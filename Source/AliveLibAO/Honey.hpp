#pragma once

#include "../relive_lib/GameObjects/BaseAnimatedWithPhysicsGameObject.hpp"

namespace AO {

class Honey final : public BaseAnimatedWithPhysicsGameObject
{
public:
    Honey(FP xpos, FP ypos, ResourceManagerWrapper& resMan);

    virtual void VUpdate() override;
    virtual void VScreenChanged() override;

    Guid mTlvInfo;
};

} // namespace AO
