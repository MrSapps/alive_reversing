#pragma once

#include "BaseAnimatedWithPhysicsGameObject.hpp"
#include "../SaveStateBase.hpp"

struct GroundExplosionSaveState final : public SaveStateBase
{
    GroundExplosionSaveState()
        : SaveStateBase(ReliveTypes::eGroundExplosion, sizeof(*this))
    { }
    FP mBombSpriteScale;
};

class GroundExplosion final : public ::BaseAnimatedWithPhysicsGameObject
{
public:
    // For a save: made with none of what an explosion does, that is in the save
    struct RestoreTag final
    { };
    GroundExplosion(RestoreTag, ResourceManager& resMan, BaseMap& map);

    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pData, ResourceManager& resMan, BaseMap& map);

    virtual void VUpdate() override;

    GroundExplosion(FP x, FP y, FP scale, ResourceManager& resMan, BaseMap& map);
private:
    void DealDamageRect(const PSX_RECT* pRect);

    FP mBombSpriteScale = {};
};
