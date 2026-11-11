#pragma once

#include "../relive_lib/GameObjects/BaseGameObject.hpp"
#include "../relive_lib/SaveStateBase.hpp"
#include "../relive_lib/WorldStatic.hpp"
#include "../relive_lib/Primitives.hpp"

namespace relive
{
    struct Path_ExplosionSet;
}

struct ExplosionSetSaveState final : public SaveStateBase
{
    ExplosionSetSaveState()
        : SaveStateBase(ReliveTypes::eExplosionSet, sizeof(*this))
    { }
    s16 mScreenShakeIdx;
    s16 mScreenShakeIdxModifier;
    s16 mStartDelay;
    s16 mSpacingMultiplicator;
    s16 mIncreasingGridSpacing;
    bool mSpawnAssets;
    bool mFlipX;
    bool mActive;
    // What its TLV gave it, false for a save without it
    bool mTlvDataSaved = false;
    s16 mTlvRectX = 0;
    s16 mTlvRectY = 0;
    s16 mTlvRectW = 0;
    s16 mTlvRectH = 0;
    FP mSpriteScale;
    s16 mSwitchId = 0;
    s16 mAssetInterval = 0;
    s16 mGridSpacing = 0;
};

class ExplosionSet final : public BaseGameObject
{
public:
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void RestoreFromSaveState(const ExplosionSetSaveState& state);

    ExplosionSet(ResourceManager& resMan, BaseMap& map);
    ~ExplosionSet();

    
    virtual void VScreenChanged() override;
    virtual void VRender(OrderingTable& ot) override;
    virtual void VUpdate() override;

    void Init(relive::Path_ExplosionSet* pTlv);
    void Start();

private:
    Prim_ScreenOffset mScreenOffset = {};
    s16 mScreenShakeIdx = 0;
    s16 mScreenShakeIdxModifier = 1;
    s16 mStartDelay = 0;
    s16 mSpacingMultiplicator = 0;
    PSX_RECT mTlvRect = {};
    FP mSpriteScale = FP_FromInteger(1);
    s16 mSwitchId = 0;
    s16 mAssetInterval = 0;
    s16 mGridSpacing = 0;
    s16 mIncreasingGridSpacing = 0;
    bool mSpawnAssets = false;
    bool mFlipX = false;
    bool mActive = false;
};

extern ExplosionSet* gExplosionSet;
extern WorldStatic<bool> gExplosionSetEnabled;
