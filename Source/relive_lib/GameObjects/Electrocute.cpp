#include "stdafx.h"
#include "Electrocute.hpp"
#include "BaseAliveGameObject.hpp"
#include "../ObjectIds.hpp"
#include "../Function.hpp"
#include "MapWrapper.hpp"
#include "PaletteOverwriter.hpp"
#include "../GameType.hpp"
#include "BaseMap.hpp"
#include "../SerializedObjectData.hpp"
#include <cstring>

Electrocute::Electrocute(BaseAliveGameObject* pTargetObj, bool bExtraOverwriter, bool bKillTarget, ResourceManager& resMan, BaseMap& map)
    : BaseGameObject(true, 0, resMan, map),
    mTargetObjId(pTargetObj->mBaseGameObjectId),
    mExtraOverwriter(bExtraOverwriter),
    mKillTarget(bKillTarget),
    mPalOverwritersCount(bExtraOverwriter ? 3 : 2)
{
    SetType(ReliveTypes::eElectrocute);

    // The target has its sprite's colours put back afterwards (the original saved a copy of the
    // palette for these)
    if (pTargetObj->Type() == ReliveTypes::eFlyingSlig ||
        pTargetObj->Type() == ReliveTypes::eGlukkon ||
        pTargetObj->Type() == ReliveTypes::eAbe ||
        (pTargetObj->Type() == ReliveTypes::eSlig && GetGameType() == GameType::eAe))
    {
        mRestoreSprite = true;
    }
}

Electrocute::Electrocute(RestoreTag, ResourceManager& resMan, BaseMap& map)
    : BaseGameObject(true, 0, resMan, map)
{
    SetType(ReliveTypes::eElectrocute);
}

Electrocute::~Electrocute()
{
    KillPalOverwriters();
}

void Electrocute::VScreenChanged()
{
    BaseAliveGameObject* pTargetObj = static_cast<BaseAliveGameObject*>(sObjectIds.Find_Impl(mTargetObjId));
    // If the map has changed or target we are tracking has died then..
    if (mMap.LevelChanged() || mMap.PathChanged() || (pTargetObj && pTargetObj->GetDead()))
    {
        Stop();
    }
}

// The colours of the overwriters, as the PSX made them from 8 bit colours (5 bits each)
static constexpr u8 To5Bit(u8 v)
{
    return static_cast<u8>((v >> 3) << 3);
}

// How a SpriteFx::Sweep's colour is drawn: the semi-transparency bit is set on white and blue
// (it blends if the sprite is semi-transparent), and black is the transparent colour
static constexpr u32 kSweepOpaque = 0;
static constexpr u32 kSweepSemiTrans = 1;
static constexpr u32 kSweepTransparent = 2;

void Electrocute::VUpdate()
{
    BaseAliveGameObject* pTargetObj = static_cast<BaseAliveGameObject*>(sObjectIds.Find_Impl(mTargetObjId));
    if (!pTargetObj || pTargetObj->GetDead())
    {
        Stop();
        return;
    }

    switch (mState)
    {
        case States::eSetNewColour:
            if (pTargetObj->Type() == ReliveTypes::eAbe && GetGameType() == GameType::eAe)
            {
                mTargetRed = 127;
                mTargetGreen = 127;
                mTargetBlue = 127;
            }
            else
            {
                mTargetRed = pTargetObj->mRGB.r;
                mTargetGreen = pTargetObj->mRGB.g;
                mTargetBlue = pTargetObj->mRGB.b;
            }

            pTargetObj->SetElectrocuting(true);

            pTargetObj->mRGB.SetRGB(255, 255, 255);
            mState = States::eAlphaFadeout;
            break;

        case States::eAlphaFadeout:
        {
            // In AO the overwriters are updated by the game loop, so their update delays work
            // and the white, blue and black bands trail one another. In AE they were updated by
            // this object (see below), the delays did nothing, and all go at once: only the last
            // colour is seen.
            const bool overwritersUpdated = GetGameType() == GameType::eAo;

            mPalOverwriters[0] = relive_new PalleteOverwriter(
                mTargetObjId, 0, kSweepSemiTrans,
                To5Bit(255), To5Bit(255), To5Bit(255), overwritersUpdated,
                mResMan, mMap);

            mPalOverwriters[1] = relive_new PalleteOverwriter(
                mTargetObjId, 1, kSweepSemiTrans,
                To5Bit(64), To5Bit(64), To5Bit(255), overwritersUpdated,
                mResMan, mMap);
            if (mPalOverwriters[1])
            {
                mPalOverwriters[1]->SetUpdateDelay(4);
            }

            if (mExtraOverwriter)
            {
                mPalOverwriters[2] = relive_new PalleteOverwriter(
                    mTargetObjId, 2, kSweepTransparent,
                    0, 0, 0, overwritersUpdated,
                    mResMan, mMap);
                if (mPalOverwriters[2])
                {
                    mPalOverwriters[2]->SetUpdateDelay(8);
                }
            }

            mState = States::eHandleDamage;
            break;
        }

        case States::eHandleDamage:
        {
            PalleteOverwriter* pPalOverwriter = mPalOverwriters[mPalOverwritersCount - 1];
            if (pPalOverwriter && pPalOverwriter->mDone)
            {
                if (GetGameType() == GameType::eAe)
                {
                    // AE put the palette back first, then hurt or hid the target. AO hurt it
                    // first.
                    RestoreSprite(*pTargetObj);
                }

                if (mKillTarget)
                {
                    pTargetObj->VTakeDamage(this);
                }
                else
                {
                    pTargetObj->GetAnimation().SetRender(false);
                }

                if (DeElectrocuteTarget(false))
                {
                    mState = States::eKillElectrocute;
                }
            }
        }
        break;

        case States::eKillElectrocute:
            SetDead(true);
            break;

        default:
            break;
    }

    if (GetGameType() == GameType::eAe)
    {
        for (auto& pPalOverwriter : mPalOverwriters)
        {
            if (pPalOverwriter)
            {
                pPalOverwriter->VUpdate();
            }
        }
    }
}

void Electrocute::Stop()
{
    KillPalOverwriters();

    SetDead(true);

    DeElectrocuteTarget(true);
}

void Electrocute::KillPalOverwriters()
{
    for (auto& pPalOverwriter : mPalOverwriters)
    {
        if (pPalOverwriter)
        {
            if (GetGameType() == GameType::eAo)
            {
                // In AO the game loop updates them, so it is the one to get rid of them. It may have done so already
                // when all the objects are destroyed (they are made after this one)
                bool exists = false;
                for (s32 i = 0; i < gBaseGameObjects->Size() && !exists; i++)
                {
                    exists = gBaseGameObjects->ItemAt(i) == pPalOverwriter;
                }
                if (exists)
                {
                    pPalOverwriter->SetDead(true);
                }
            }
            else
            {
                relive_delete pPalOverwriter;
            }
        }
        pPalOverwriter = nullptr;
    }
}

bool Electrocute::DeElectrocuteTarget(bool dealDamage)
{
    auto pTarget = static_cast<BaseAliveGameObject*>(sObjectIds.Find_Impl(mTargetObjId));
    if (pTarget)
    {
        if ((GetGameType() == GameType::eAo && pTarget->Type() == ReliveTypes::eAbe) ||
            GetGameType() == GameType::eAe)
        {
            RestoreSprite(*pTarget);

            pTarget->mRGB.SetRGB(mTargetRed, mTargetGreen, mTargetBlue);
            pTarget->SetElectrocuting(false);
        }

        if (dealDamage)
        {
            pTarget->VTakeDamage(this);
        }

        mTargetObjId = Guid{};
        return true;
    }
    return false;
}

void Electrocute::RestoreSprite(BaseAliveGameObject& target)
{
    // Put the sprite's colours back
    if (mRestoreSprite)
    {
        target.GetAnimation().Fx().ClearSweeps();
    }
}

bool Electrocute::IsLastOverwriterFinished()
{
    for (s32 i = mPalOverwritersCount - 1; i >= 0; i--)
    {
        if (mPalOverwriters[i] && mPalOverwriters[i]->mDone)
        {
            return true;
        }
    }
    return false;
}

void Electrocute::VGetSaveState(SerializedObjectData& pSaveBuffer)
{
    ElectrocuteSaveState data = {};
    data.mTargetObjId = mTargetObjId;
    data.mTargetRed = mTargetRed;
    data.mTargetGreen = mTargetGreen;
    data.mTargetBlue = mTargetBlue;
    data.mPalOverwritersCount = mPalOverwritersCount;
    data.mState = static_cast<s32>(mState);
    data.mExtraOverwriter = mExtraOverwriter;
    data.mKillTarget = mKillTarget;
    data.mRestoreSprite = mRestoreSprite;
    for (s32 i = 0; i < 3; i++)
    {
        data.mHasOverwriter[i] = mPalOverwriters[i] != nullptr;
        if (mPalOverwriters[i])
        {
            data.mOverwriterIds[i] = mPalOverwriters[i]->mBaseGameObjectId;
            data.mOverwriters[i] = mPalOverwriters[i]->GetData();
        }
    }
    pSaveBuffer.Write(data);
}

void Electrocute::CreateFromSaveState(SerializedObjectData& pData, ResourceManager& resMan, BaseMap& map)
{
    const ElectrocuteSaveState& state = *pData.ReadTmpPtr<ElectrocuteSaveState>();
    Electrocute* p = relive_new Electrocute(RestoreTag{}, resMan, map);
    if (!p)
    {
        return;
    }
    p->mTargetObjId = state.mTargetObjId;
    p->mTargetRed = state.mTargetRed;
    p->mTargetGreen = state.mTargetGreen;
    p->mTargetBlue = state.mTargetBlue;
    p->mPalOverwritersCount = state.mPalOverwritersCount;
    p->mState = static_cast<States>(state.mState);
    p->mExtraOverwriter = state.mExtraOverwriter;
    p->mKillTarget = state.mKillTarget;
    p->mRestoreSprite = state.mRestoreSprite;
    for (s32 i = 0; i < 3; i++)
    {
        if (!state.mHasOverwriter[i])
        {
            continue;
        }
        if (GetGameType() == GameType::eAe)
        {
            // Not in the list of objects that update, so not saved on their own: made here, with the id they had (they
            // are drawn, and the drawing goes through them in the order of their ids)
            Guid::CreationScope idScope(state.mOverwriterIds[i]);
            p->mPalOverwriters[i] = PalleteOverwriter::CreateFromData(state.mOverwriters[i], false, resMan, map);
        }
        else
        {
            // Saved on their own, made by now or after this
            p->mRestoredOverwriterIds[i] = state.mOverwriterIds[i];
        }
    }
}

void Electrocute::VPostRestore()
{
    for (s32 i = 0; i < 3; i++)
    {
        if (mRestoredOverwriterIds[i].IsValid())
        {
            mPalOverwriters[i] = static_cast<PalleteOverwriter*>(sObjectIds.Find_Impl(mRestoredOverwriterIds[i]));
            mRestoredOverwriterIds[i] = Guid{};
        }
    }
}
