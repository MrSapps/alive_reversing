#pragma once

#include "../relive_lib/GameObjects/BaseAnimatedWithPhysicsGameObject.hpp"
#include "../SaveStateBase.hpp"

struct DoveSaveState final : public SaveStateBase
{
    DoveSaveState()
        : SaveStateBase(ReliveTypes::eDove, sizeof(*this))
    { }
    // The id the dove had when saved: the bird portal holds its doves' ids, see BaseGameObject::RefreshId
    Guid mBaseTlvId;
    // The TLV that made it (invalid for the ones made at a position), reset when it dies
    Guid mTlvInfo;
    FP mXPos;
    FP mYPos;
    FP mVelX;
    FP mVelY;
    FP mSpriteScale;
    FP mJoinX;
    FP mJoinY;
    s16 mCurrentPath;
    EReliveLevelIds mCurrentLevel;
    s32 mAnimId;
    AnimationSaveState mAnim;
    bool mKeepInGlobalArray;
    s16 mFlyAwayCounter;
    s16 mDoveState;
    s32 mJoinDeadTimer;
    s8 mAngle;
    // The width the doves around Abe's portal are circling at is shared by all of them (file statics in Dove.cpp)
    s32 mAbePortalTimer;
    s16 mAbePortalWidth;
    s16 mAbePortalDirection;
};

class Dove final : public ::BaseAnimatedWithPhysicsGameObject
{
public:
    Dove(AnimId animId, const Guid& tlvId, FP scale, ResourceManager& resMan, BaseMap& map);
    Dove(AnimId animId, FP xpos, FP ypos, FP scale, ResourceManager& resMan, BaseMap& map);
    ~Dove();

    void LoadAnimations();
    virtual void VUpdate() override;
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pData, ResourceManager& resMan, BaseMap& map);
    virtual void VRender(OrderingTable& ot) override
    {
        // TODO: redundant override ?
        BaseAnimatedWithPhysicsGameObject::VRender(ot);
    }


    void AsAlmostACircle(FP xpos, FP ypos, u8 angle);
    void AsACircle(FP xpos, FP ypos, u8 angle);
    void AsJoin(FP xpos, FP ypos);
    void FlyAway(bool spookedInstantly);

    static void All_FlyAway(bool spookedInstantly);

private:
    s16 mFlyAwayCounter = 0;
    enum class State : s16
    {
        eOnGround_0 = 0,
        eFlyAway_1 = 1,
        eJoin_2 = 2,
        eCircle_3 = 3,
        eAlmostACircle_4 = 4,
    };
    State mDoveState = State::eOnGround_0;
    bool mKeepInGlobalArray = false;
    Guid mTlvInfo;
    FP mJoinX = {};
    FP mJoinY = {};
    s32 mJoinDeadTimer = 0;
    s8 mAngle = 0;
};
