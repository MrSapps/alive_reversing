#pragma once

#include "BaseAnimatedWithPhysicsGameObject.hpp"

class BirdPortalTerminator final : public BaseAnimatedWithPhysicsGameObject
{
public:
    BirdPortalTerminator(FP xpos, FP ypos, FP scale, relive::Path_BirdPortal::PortalType portalType, ResourceManagerWrapper& resMan);

    void LoadAnimations();

    virtual void VScreenChanged() override;

    void Fadeout();
};
