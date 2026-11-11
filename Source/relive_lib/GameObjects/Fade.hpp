#pragma once

#include "EffectBase.hpp"
#include "../SaveStateBase.hpp"
#include "../Layer.hpp"

enum class FadeOptions
{
    eFadeOut,
    eFadeIn
};

struct FadeSaveState final : public SaveStateBase
{
    FadeSaveState()
        : SaveStateBase(ReliveTypes::eFade, sizeof(*this))
    { }
    EffectBaseSaveState mEffect;
    s32 mFadeOption;
    bool mDestroyOnDone;
    s32 mSpeed;
    s32 mFadeColour;
    bool mDone;
};

class Fade final : public EffectBase
{
public:
    Fade(Layer layer, FadeOptions fade, bool destroyOnDone, s32 speed, relive::TBlendModes abr, ResourceManager& resMan, BaseMap& map);
    ~Fade();
    void Init(Layer layer, FadeOptions fade, bool destroyOnDone, s32 speed);
    virtual void VScreenChanged() override;
    virtual void VUpdate() override;
    virtual void VRender(OrderingTable& ot) override;
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pData, ResourceManager& resMan, BaseMap& map);

public:
    bool mDone = false;

private:
    s32 mFadeColour = 0;
    s32 mSpeed = 0;
    FadeOptions mFadeOption = FadeOptions::eFadeOut;
    bool mDestroyOnDone = false;
};
