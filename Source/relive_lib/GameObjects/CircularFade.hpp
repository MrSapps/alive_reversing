#pragma once

#include "BaseAnimatedWithPhysicsGameObject.hpp"
#include "../SaveStateBase.hpp"

struct CircularFadeSaveState final : public SaveStateBase
{
    CircularFadeSaveState()
        : SaveStateBase(ReliveTypes::eCircularFade, sizeof(*this))
    { }
    bool mFadeIn;
    bool mDone;
    bool mDestroyOnDone;
    s16 mFadeColour;
    s16 mSpeed;
};

class CircularFade final : public BaseAnimatedWithPhysicsGameObject
{
public:
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pData, ResourceManager& resMan, BaseMap& map);

    CircularFade(FP xpos, FP ypos, FP scale, bool fadeIn, s8 destroyOnDone, ResourceManager& resMan, BaseMap& map);
    ~CircularFade();

    virtual void VScreenChanged() override;
    virtual void VUpdate() override;
    virtual void VRender(OrderingTable& ot) override;
    virtual std::vector<NotStateMemory> VNotStateMemory() override;

    // New virtuals
    virtual void VFadeIn(bool fadeIn, s8 destroyOnDone);
    virtual s32 VDone();

private:
    bool mFadeIn = false;
    bool mDone = false;
    bool mDestroyOnDone = false;
    Poly_G4 mTile1 = {};
    Poly_G4 mTile2 = {};
    Poly_G4 mTile3 = {};
    Poly_G4 mTile4 = {};
    s16 mFadeColour = 0;
    s16 mSpeed = 0;
};

CircularFade* Make_Circular_Fade(FP xpos, FP ypos, FP scale, bool fadeIn, s8 destroyOnDone, bool surviveDeathReset, ResourceManager& resMan, BaseMap& map);