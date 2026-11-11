#pragma once

#include "../relive_lib/GameObjects/PlatformBase.hpp"
#include "../relive_lib/data_conversion/relive_tlvs.hpp"
#include "../relive_lib/SaveStateBase.hpp"

struct LiftPointSaveState final : public SaveStateBase
{
    LiftPointSaveState()
        : SaveStateBase(ReliveTypes::eLiftPoint, sizeof(*this))
    { }
    FP mXPos;
    FP mYPos;
    Guid mPlatformId;
    Guid mTlvId;
    FP mFloorLevelY;
    relive::Path_LiftPoint::LiftPointStopType mLiftPointStopType;
    bool mMoving;
    bool mTopFloor;
    bool mMiddleFloor;
    bool mBottomFloor;
    bool mMoveToFloorLevel;
    bool mKeepOnMiddleFloor;
    AnimationSaveState mAnim;
    AnimationSaveState mLiftWheelAnim;
    AnimationSaveState mPulleyAnim;
    // False for a save from the original game's, which has none of the platform's variables
    bool mPlatformBaseSaved;
    s16 mPlatformBaseXOffset;
    s16 mPlatformBaseWidthOffset;
    s16 mPlatformBaseYOffset;
    s16 mPlatformBaseHeightOffset;
    s32 mPlatformBaseCount;
};

class LiftPoint final : public PlatformBase
{
public:
    LiftPoint(relive::Path_LiftPoint* pTlv, const Guid& tlvId, ResourceManager& resMan, BaseMap& map);
    ~LiftPoint();
    
    void LoadAnimations();

    virtual void VRender(OrderingTable& ot) override;
    virtual void VUpdate() override;
    virtual void VScreenChanged() override;
    virtual std::vector<NotStateMemory> VNotStateMemory() override;
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;

    static void CreateFromSaveState(SerializedObjectData& pData, ResourceManager& resMan, BaseMap& map);

    void KeepOnMiddleFloor();
    // Puts the ropes where the lift is: its update does this every frame
    void SyncRopes();
    bool MovingToFloorLevel() const;
    bool OnTopFloor() const;
    bool OnBottomFloor() const;
    bool OnMiddleFloor() const;
    bool OnAnyFloor() const;
    bool OnAFloorLiftMoverCanUse() const;

    void Move(FP xSpeed, FP ySpeed);

private:
    void MoveObjectsOnLift(FP xVelocity);
    static void ClearTlvFlags(relive::Path_TLV* pTlv);
    void StayOnFloor(bool bFloor, relive::Path_LiftPoint* pLiftTlv);

    void CreatePulleyIfExists();
    void RenderPulley(OrderingTable& ot);

public:
    s16 mLiftPointId = 0;
    bool mMoving = false;

private:
    relive::Path_LiftPoint::LiftPointStopType mLiftPointStopType = relive::Path_LiftPoint::LiftPointStopType::eTopFloor;
    Guid mRopeId2;
    Guid mRopeId1;
    Animation mLiftWheelAnim = {};
    Animation mPulleyAnim = {};
    s16 mPulleyXPos = 0;
    s16 mPulleyYPos = 0;
    FP mFloorLevelY = {};
    Guid mTlvId;
    bool mTopFloor = false;
    bool mMiddleFloor = false;
    bool mBottomFloor = false;
    bool mHasPulley = false;
    bool mMoveToFloorLevel = false;
    bool mKeepOnMiddleFloor = false;
    bool mIgnoreLiftMover = false;
};
