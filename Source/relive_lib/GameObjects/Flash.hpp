#pragma once

#include "EffectBase.hpp"
#include "../SaveStateBase.hpp"
#include "../Layer.hpp"

struct FlashSaveState final : public SaveStateBase
{
    FlashSaveState()
        : SaveStateBase(ReliveTypes::eFlash, sizeof(*this))
    { }
    EffectBaseSaveState mEffect;
    s32 mFlashTime;
};

class Flash final : public EffectBase
{
public:
    Flash(Layer layer, s32 r, s32 g, s32 b, ResourceManager& resMan, BaseMap& map, relive::TBlendModes abr = relive::TBlendModes::eBlend_3, s32 time = 1);

    virtual void VUpdate() override;
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pData, ResourceManager& resMan, BaseMap& map);
private:
    s32 mFlashTime = 0;
};
