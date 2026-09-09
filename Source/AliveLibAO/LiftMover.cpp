#include "stdafx_ao.h"
#include "LiftMover.hpp"
#include "LiftPoint.hpp"
#include "../relive_lib/Events.hpp"
#include "../relive_lib/SwitchStates.hpp"
#include "../AliveLibAE/stdlib.hpp"
#include "Factory.hpp"
#include "Path.hpp"
#include "../relive_lib/ObjectIds.hpp"
#include "../relive_lib/SerializedObjectData.hpp"
#include "Map.hpp"

namespace AO {

LiftMover::LiftMover(relive::Path_LiftMover* pTlv, const Guid& tlvId, ResourceManagerWrapper& resMan, BaseMap& map)
    : BaseGameObject(true, 0, resMan, map)
{
    mTlvId = tlvId;
    SetType(ReliveTypes::eLiftMover);

    mLiftMoverSwitchId = pTlv->mLiftMoverSwitchId;
    mTargetLiftPointId = pTlv->mTargetLiftPointId;

    if (pTlv->mMoveDirection == relive::Path_LiftMover::YDirection::eUp)
    {
        mLiftSpeed = FP_FromInteger(-4);
    }
    else
    {
        mLiftSpeed = FP_FromInteger(4);
    }

    mState = LiftMoverStates::eInactive_0;
}

LiftMover::~LiftMover()
{
    if (mTargetLift.IsValid())
    {
        mTargetLift = Guid{};
    }
    mMap.TLV_Reset(mTlvId);
}

void LiftMover::VUpdate()
{
    auto pTargetLift = static_cast<LiftPoint*>(sObjectIds.Find_Impl(mTargetLift));

    if (!pTargetLift && mState != LiftMoverStates::eInactive_0)
    {
        // mTargetLift is a runtime object id restored (or left default)
        // from a save/load, so it isn't meaningful across sessions - if
        // we're mid-operation but don't have a live target, re-resolve it
        // by the TLV-derived mTargetLiftPointId the same way eInactive_0
        // below already does, instead of the active states further down
        // dereferencing a null pTargetLift (previously unguarded here).
        pTargetLift = FindLiftPointWithId(mTargetLiftPointId);
        if (pTargetLift)
        {
            mTargetLift = pTargetLift->mBaseGameObjectId;
        }
    }

    if (pTargetLift && pTargetLift->GetDead())
    {
        mTargetLift = Guid{};
        SetDead(true);
        return;
    }

    switch (mState)
    {
        case LiftMoverStates::eInactive_0:
            if (SwitchStates_Get(mLiftMoverSwitchId))
            {
                if (pTargetLift)
                {
                    mState = LiftMoverStates::eStartMovingDown_1;
                }
                else
                {
                    // Find the lift point
                    pTargetLift = FindLiftPointWithId(mTargetLiftPointId);

                    if (!pTargetLift)
                    {
                        // Load lift point objects (I guess in case for some reason it got unloaded ??)
                        // AE doesn't do this.
                        for (s16 y = 0; y < mMap.mCamsOnY; y++)
                        {
                            for (s16 x = 0; x < mMap.mCamsOnX; x++)
                            {
                                mMap.GetPath().Loader(x, y, relive::Factory::LoadMode::ConstructObject_0, ReliveTypes::eLiftPoint);
                            }
                        }

                        // And have another look now that we might have just loaded it in
                        pTargetLift = FindLiftPointWithId(mTargetLiftPointId);
                    }

                    if (pTargetLift)
                    {
                        mTargetLift = pTargetLift->mBaseGameObjectId;
                        mState = LiftMoverStates::eStartMovingDown_1;
                    }
                }
            }
            break;

        case LiftMoverStates::eStartMovingDown_1:
            if (!pTargetLift)
            {
                return;
            }

            if (!pTargetLift->OnAnyFloor())
            {
                pTargetLift->mKeepOnMiddleFloor = true;
                mState = LiftMoverStates::eMovingDown_2;
            }
            else
            {
                pTargetLift->Move(FP_FromInteger(0), mLiftSpeed);

                if ((mLiftSpeed > FP_FromInteger(0) && pTargetLift->OnBottomFloor()) || (mLiftSpeed < FP_FromInteger(0) && pTargetLift->OnTopFloor()))
                {
                    mState = LiftMoverStates::eMovingDown_2;
                }
            }
            break;

        case LiftMoverStates::eMovingDown_2:
            if (!pTargetLift)
            {
                return;
            }

            if (!pTargetLift->OnAFloorLiftMoverCanUse())
            {
                pTargetLift->Move(FP_FromInteger(0), mLiftSpeed);
            }
            else
            {
                pTargetLift->Move(FP_FromInteger(0), FP_FromInteger(0));
                mState = LiftMoverStates::eMovingDone_5;
            }
            break;

        case LiftMoverStates::eStartMovingUp_3:
            if (!pTargetLift)
            {
                return;
            }

            if (pTargetLift->OnAFloorLiftMoverCanUse())
            {
                pTargetLift->Move(FP_FromInteger(0), mLiftSpeed);

                if ((mLiftSpeed > FP_FromInteger(0) && pTargetLift->OnBottomFloor()) || (mLiftSpeed < FP_FromInteger(0) && pTargetLift->OnTopFloor()))
                {
                    mState = LiftMoverStates::eMovingDown_2;
                }
            }
            else
            {
                pTargetLift->mKeepOnMiddleFloor = true;
                mState = LiftMoverStates::eMovingUp_4;
            }
            break;

        case LiftMoverStates::eMovingUp_4:
            if (!pTargetLift)
            {
                return;
            }

            if (pTargetLift->OnAFloorLiftMoverCanUse())
            {
                pTargetLift->Move(FP_FromInteger(0), FP_FromInteger(0));
                mState = LiftMoverStates::eInactive_0;
                mLiftSpeed = -mLiftSpeed;
            }
            else
            {
                pTargetLift->Move(FP_FromInteger(0), mLiftSpeed);
            }
            break;

        case LiftMoverStates::eMovingDone_5:
            if (!SwitchStates_Get(mLiftMoverSwitchId))
            {
                mState = LiftMoverStates::eStartMovingUp_3;
                mLiftSpeed = -mLiftSpeed;
            }
            break;

        default:
            break;
    }

    if (EventGet(Event::kEventDeathReset))
    {
        SetDead(true);
    }
}

LiftPoint* LiftMover::FindLiftPointWithId(s16 id)
{
    for (s32 i = 0; i < gBaseAliveGameObjects->Size(); i++)
    {
        auto pObj = gBaseAliveGameObjects->ItemAt(i);
        if (!pObj)
        {
            break;
        }

        if (pObj->Type() == ReliveTypes::eLiftPoint)
        {
            auto pLift = static_cast<LiftPoint*>(pObj);
            if (pLift->mLiftPointId == id)
            {
                return pLift;
            }
        }
    }
    return nullptr;
}

void LiftMover::VGetSaveState(SerializedObjectData& pSaveBuffer)
{
    LiftMoverSaveState data = {};

    data.mTlvId = mTlvId;
    data.mState = mState;

    pSaveBuffer.Write(data);
}

void LiftMover::CreateFromSaveState(SerializedObjectData& pData, ResourceManagerWrapper& resMan, BaseMap& map)
{
    const auto pState = pData.ReadTmpPtr<LiftMoverSaveState>();

    auto tlvIterator = map.TLV_From_Offset_Lvl_Cam(pState->mTlvId);
    auto pTlv = tlvIterator.GetTlvChecked<relive::Path_LiftMover>(ReliveTypes::eLiftMover);

    auto pLiftMover = relive_new LiftMover(pTlv, pState->mTlvId, resMan, map);
    if (pLiftMover)
    {
        // mTargetLift is deliberately left at its freshly-constructed
        // default (invalid) value - it's a runtime object id, not
        // meaningful across a restore. VUpdate() already re-resolves it
        // from mTargetLiftPointId on its next tick if mState says we
        // should be mid-operation (see the guard added at the top of
        // VUpdate above).
        pLiftMover->mState = pState->mState;
    }
}

} // namespace AO
