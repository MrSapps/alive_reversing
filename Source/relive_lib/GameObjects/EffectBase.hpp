#pragma once

#include "BaseGameObject.hpp"
#include "../Primitives.hpp"
#include "../Layer.hpp"
#include "../MapWrapper.hpp"

namespace relive {
enum class TBlendModes : u32;
}

// What the effects (a flash, a fade...) keep in common in their saves
struct EffectBaseSaveState final
{
    u32 mBlendMode;
    s16 mLayer;
    s16 mPathId;
    s16 mLevelId;
    s32 mRed;
    s32 mGreen;
    s32 mBlue;
    bool mSemiTrans;
};

class EffectBase : public BaseGameObject
{
public:
    virtual void VRender(OrderingTable& ot) override;
    EffectBase(Layer layer, relive::TBlendModes blendMode, ResourceManager& resMan, BaseMap& map);
    ~EffectBase();

    virtual std::vector<NotStateMemory> VNotStateMemory() override;

protected:
    // The blend mode and layer are what the object is made with, the rest is put back on it
    EffectBaseSaveState GetEffectState() const;
    void SetEffectState(const EffectBaseSaveState& state);

    s16 mEffectBasePathId = 0;
    EReliveLevelIds mEffectBaseLevelId = EReliveLevelIds::eNone;
    Poly_G4 mEffectBaseTile = {};
    Layer mEffectBaseLayer = Layer::eLayer_0;
    s32 mEffectBaseRed = 0;
    s32 mEffectBaseGreen = 0;
    s32 mEffectBaseBlue = 0;
    bool mSemiTrans = false;
    const relive::TBlendModes mBlendMode = {};
};
