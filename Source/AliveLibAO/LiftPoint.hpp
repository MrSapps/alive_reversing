#pragma once

#include "../relive_lib/Animation.hpp"
#include "../relive_lib/GameObjects/PlatformBase.hpp"
#include "Map.hpp"
#include "../relive_lib/data_conversion/relive_tlvs.hpp"
#include "../relive_lib/SaveStateBase.hpp"

class Rope;
class SerializedObjectData;

namespace AO {

struct LiftPointSaveState final : public SaveStateBase
{
    LiftPointSaveState()
        : SaveStateBase(ReliveTypes::eLiftPoint, sizeof(*this))
    { }
    FP mXPos = {};
    FP mYPos = {};
    Guid mPlatformId;
    Guid mTlvId;
    FP mFloorLevelY = {};
    relive::Path_LiftPoint::LiftPointStopType mLiftPointStopType = relive::Path_LiftPoint::LiftPointStopType::eStartPointOnly;
    bool mMoving = false;
    bool mTopFloor = false;
    bool mMiddleFloor = false;
    bool mBottomFloor = false;
    bool mMoveToFloorLevel = false;
    bool mKeepOnMiddleFloor = false;
};

class LiftPoint final : public ::PlatformBase
{
public:
    LiftPoint(relive::Path_LiftPoint* pTlv, const Guid& tlvId, ResourceManagerWrapper& resMan, BaseMap& map);
    ~LiftPoint();

    void LoadAnimations();

    virtual void VRender(OrderingTable& ot) override;
    virtual void VUpdate() override;
    virtual void VScreenChanged() override;
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pData, ResourceManagerWrapper& resMan, BaseMap& map);
    bool OnTopFloor() const;
    bool OnBottomFloor() const;
    bool OnMiddleFloor() const;
    bool OnAnyFloor() const;
    bool OnAFloorLiftMoverCanUse() const;

    void Move(FP xSpeed, FP ySpeed);

private:

    void MoveObjectsOnLift(FP xVelocity);

    void ClearTlvFlags(relive::Path_TLV* pTlv);
    void StayOnFloor(bool bFloor, relive::Path_LiftPoint* pLiftTlv);

    void CreatePulleyIfExists(s16 camX, s16 camY);
    void RenderPulley(OrderingTable& ot);

public:
    s16 mLiftPointId = 0;
    bool mMoving = false;
    bool mKeepOnMiddleFloor = false;
    bool mIgnoreLiftMover = true;
private:

    relive::Path_LiftPoint::LiftPointStopType mLiftPointStopType = relive::Path_LiftPoint::LiftPointStopType::eStartPointOnly;
    Guid mRopeId2 = {};
    Guid mRopeId1 = {};
    Animation mLiftWheelAnim;
    Animation mPulleyAnim;
    s16 mPulleyXPos = 0;
    s16 mPulleyYPos = 0;
    FP mFloorLevelY = {};
    Guid mTlvId;
    bool mTopFloor = false;
    bool mMiddleFloor = false;
    bool mBottomFloor = false;
    bool mHasPulley = false;
    bool mMoveToFloorLevel = false;
};


} // namespace AO
