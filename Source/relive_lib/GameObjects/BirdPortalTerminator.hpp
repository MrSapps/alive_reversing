#pragma once

#include "BaseAnimatedWithPhysicsGameObject.hpp"
#include "../SaveStateBase.hpp"

struct BirdPortalTerminatorSaveState final : public SaveStateBase
{
    BirdPortalTerminatorSaveState()
        : SaveStateBase(ReliveTypes::eBirdPortalTerminator, sizeof(*this))
    { }
};

class BirdPortalTerminator final : public BaseAnimatedWithPhysicsGameObject
{
public:
    // For a save: what the terminator looks like is in the save
    struct RestoreTag final
    { };
    BirdPortalTerminator(RestoreTag, ResourceManager& resMan, BaseMap& map);

    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pData, ResourceManager& resMan, BaseMap& map);

    BirdPortalTerminator(FP xpos, FP ypos, FP scale, relive::Path_BirdPortal::PortalType portalType, ResourceManager& resMan, BaseMap& map);

    void LoadAnimations();

    virtual void VScreenChanged() override;

    void Fadeout();
};
