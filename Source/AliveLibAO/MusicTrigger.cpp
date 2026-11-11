#include "stdafx_ao.h"
#include <algorithm>
#include "../relive_lib/ObjectIds.hpp"
#include "../relive_lib/SerializedObjectData.hpp"
#include "MusicTrigger.hpp"
#include "../relive_lib/SwitchStates.hpp"
#include "../AliveLibAE/stdlib.hpp"
#include "../relive_lib/Events.hpp"
#include "MusicController.hpp"
#include "Engine.hpp"
#include "Path.hpp"
#include "Map.hpp"

namespace AO {

MusicTrigger::MusicTrigger(relive::Path_MusicTrigger::MusicTriggerMusicType type, relive::Path_MusicTrigger::TriggeredBy triggeredBy, s32 switchId, s32 delay, ResourceManager& resMan, BaseMap& map)
    : BaseGameObject(true, 0, resMan, map)
{
    Init(type, triggeredBy, static_cast<s16>(switchId), static_cast<s16>(delay));
    mTlvId = {};
}

MusicTrigger::MusicTrigger(relive::Path_MusicTrigger* pTlv, const Guid& tlvId, ResourceManager& resMan, BaseMap& map)
    : BaseGameObject(true, 0, resMan, map)
{
    Init(pTlv->mMusicType, pTlv->mTriggeredBy, pTlv->mSwitchId, pTlv->mMusicDelay);
    mTlvId = tlvId;

    // A restore makes it again, because the flags of what is made by a camera are cleared in a save: only the ones the save has were alive
    if (mMap.mLoadingFromSave)
    {
        const std::vector<Guid>& saved = mMap.mRestoreObjectOrder;
        SetDead(std::find(saved.begin(), saved.end(), Guid::FromTlvId(tlvId)) == saved.end());
    }
}

MusicTrigger::MusicTrigger(RestoreTag, ResourceManager& resMan, BaseMap& map)
    : BaseGameObject(true, 0, resMan, map)
{
    SetType(ReliveTypes::eNone);
}

void MusicTrigger::Init(relive::Path_MusicTrigger::MusicTriggerMusicType type, relive::Path_MusicTrigger::TriggeredBy triggeredBy, u16 switchId, s16 delay)
{
    mSetMusicToNoneOnDtor = false;
    mUnknown = false;
    mHasSwitchId = false;

    SetType(ReliveTypes::eNone);

    switch (type)
    {
        case relive::Path_MusicTrigger::MusicTriggerMusicType::eDrumAmbience:
            mMusicType = MusicController::MusicTypes::eDrumAmbience_3;
            mCounter = 400;
            break;

        case relive::Path_MusicTrigger::MusicTriggerMusicType::eDeathDrumShort:
            mMusicType = MusicController::MusicTypes::eDeathDrumShort_13;
            mCounter = 30;
            break;

        case relive::Path_MusicTrigger::MusicTriggerMusicType::eSecretAreaLong:
            mMusicType = MusicController::MusicTypes::eSecretAreaLong_16;
            mCounter = 30;
            break;

        case relive::Path_MusicTrigger::MusicTriggerMusicType::eSoftChase:
            mMusicType = MusicController::MusicTypes::eSlogChase_5;
            mSetMusicToNoneOnDtor = true;
            break;

        case relive::Path_MusicTrigger::MusicTriggerMusicType::eIntenseChase:
            mMusicType = MusicController::MusicTypes::eIntenseChase_8;
            mSetMusicToNoneOnDtor = true;
            break;

        case relive::Path_MusicTrigger::MusicTriggerMusicType::eChime:
            mMusicType = MusicController::MusicTypes::eChime_2;
            mCounter = delay;
            break;

        case relive::Path_MusicTrigger::MusicTriggerMusicType::eSecretAreaShort:
            mMusicType = MusicController::MusicTypes::eSecretAreaShort_15;
            mCounter = 30;
            break;

        default:
            break;
    }

    switch (triggeredBy)
    {
        case relive::Path_MusicTrigger::TriggeredBy::eTimer:
            SetUpdateDelay(0);
            break;

        case relive::Path_MusicTrigger::TriggeredBy::eTouching:
            SetUpdateDelay(delay);
            break;

        case relive::Path_MusicTrigger::TriggeredBy::eSwitchId: // removed in AE
            mHasSwitchId = true;;
            mSwitchId = switchId;
            SetUpdateDelay(0);
            mCounter = delay;
            if (switchId > 1)
            {
                if (SwitchStates_Get(switchId))
                {
                    SetDead(true);
                }
            }
            break;

        case relive::Path_MusicTrigger::TriggeredBy::eUnknown: // removed in AE
            mHasSwitchId = true;
            mSwitchId = switchId;
            SetUpdateDelay(0);
            mCounter = -1;
            break;

        default:
            return;
    }
}

MusicTrigger::~MusicTrigger()
{
    if (mSetMusicToNoneOnDtor)
    {
        MusicController::static_PlayMusic(MusicController::MusicTypes::eType0, this, 0, 0);
    }
    MusicController::ClearObject(this);
}

void MusicTrigger::VScreenChanged()
{
    if (mMap.LevelChanged())
    {
        SetDead(true);
    }
}

void MusicTrigger::VUpdate()
{
    if (EventGet(Event::kEventHeroDying))
    {
        SetDead(true);
        if (mTlvId.IsValid())
        {
            mMap.TLV_Reset(mTlvId);
        }
    }

    if (mHasSwitchId)
    {
        if (SwitchStates_Get(mSwitchId))
        {
            mHasSwitchId = false;

            MusicController::static_PlayMusic(mMusicType, this, mSetMusicToNoneOnDtor, 1);
            mUnknown = true;

            if (mCounter >= 0)
            {
                mCounter += sGnFrame;
            }
        }
    }
    else
    {
        if (!mUnknown)
        {
            MusicController::static_PlayMusic(mMusicType, this, mSetMusicToNoneOnDtor, 1);
            mUnknown = true;
            mCounter += sGnFrame;
            return;
        }

        if (mCounter < 0)
        {
            if (!SwitchStates_Get(mSwitchId))
            {
                SetDead(true);
                return;
            }

            if (mCounter < 0)
            {
                MusicController::static_PlayMusic(mMusicType, this, mSetMusicToNoneOnDtor, 0);
                return;
            }
        }

        if (static_cast<s32>(sGnFrame) < mCounter)
        {
            MusicController::static_PlayMusic(mMusicType, this, mSetMusicToNoneOnDtor, 0);
        }
        else
        {
            SetDead(true);
        }
    }
}

void MusicTrigger::VGetSaveState(SerializedObjectData& pSaveBuffer)
{
    MusicTriggerSaveState data = {};
    data.mTlvId = mTlvId;
    data.mHasSwitchId = mHasSwitchId;
    data.mSetMusicToNoneOnDtor = mSetMusicToNoneOnDtor;
    data.mUnknown = mUnknown;
    data.mCounter = mCounter;
    data.mMusicType = static_cast<s32>(mMusicType);
    data.mSwitchId = mSwitchId;
    pSaveBuffer.Write(data);
}

void MusicTrigger::RestoreFromSaveState(const MusicTriggerSaveState& state)
{
    BaseGameObject* pObj = sObjectIds.Find_Impl(state.mObjectId);
    if (!pObj)
    {
        return;
    }
    MusicTrigger* p = static_cast<MusicTrigger*>(pObj);
    p->mTlvId = state.mTlvId;
    p->mHasSwitchId = state.mHasSwitchId;
    p->mSetMusicToNoneOnDtor = state.mSetMusicToNoneOnDtor;
    p->mUnknown = state.mUnknown;
    p->mCounter = state.mCounter;
    p->mMusicType = static_cast<MusicController::MusicTypes>(state.mMusicType);
    p->mSwitchId = state.mSwitchId;
}

void MusicTrigger::CreateFromSaveState(SerializedObjectData& pData, ResourceManager& resMan, BaseMap& map)
{
    const MusicTriggerSaveState& state = *pData.ReadTmpPtr<MusicTriggerSaveState>();
    MusicTrigger* p = relive_new MusicTrigger(MusicTrigger::RestoreTag{}, resMan, map);
    if (!p)
    {
        return;
    }
    p->mTlvId = state.mTlvId;
    p->mHasSwitchId = state.mHasSwitchId;
    p->mSetMusicToNoneOnDtor = state.mSetMusicToNoneOnDtor;
    p->mUnknown = state.mUnknown;
    p->mCounter = state.mCounter;
    p->mMusicType = static_cast<MusicController::MusicTypes>(state.mMusicType);
    p->mSwitchId = state.mSwitchId;
}

} // namespace AO
