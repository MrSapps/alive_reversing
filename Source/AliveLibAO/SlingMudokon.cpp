#include "stdafx_ao.h"
#include "../relive_lib/Function.hpp"
#include "SlingMudokon.hpp"
#include "../relive_lib/SerializedObjectData.hpp"
#include "GameSpeak.hpp"
#include "../relive_lib/Collisions.hpp"
#include "Sfx.hpp"
#include "Bullet.hpp"
#include "../AliveLibAE/stdlib.hpp"
#include "Abe.hpp"
#include "Engine.hpp"
#include "../relive_lib/GameObjects/CheatController.hpp"
#include "../relive_lib/GameObjects/Particle.hpp"
#include "../relive_lib/GameObjects/Flash.hpp"
#include "../relive_lib/GameObjects/Dove.hpp"
#include "Math.hpp"
#include "../relive_lib/Grid.hpp"
#include "../relive_lib/Events.hpp"
#include "../relive_lib/FixedPoint.hpp"
#include "../relive_lib/data_conversion/relive_tlvs.hpp"
#include "Path.hpp"
#include "../relive_lib/FatalError.hpp"
#include "Map.hpp"

namespace AO {

using TSlingMudStateFunction = decltype(&SlingMudokon::Motion_0_Idle);

const TSlingMudStateFunction gSlingMudMotionTable_4CFCB0[] = {
    &SlingMudokon::Motion_0_Idle,
    &SlingMudokon::Motion_1_Angry,
    &SlingMudokon::Motion_2_Speak,
    &SlingMudokon::Motion_3_ShootStart,
    &SlingMudokon::Motion_4_ShootEnd,
    &SlingMudokon::Motion_5_AngryToIdle};


void SlingMudokon::LoadAnimations()
{
    for (auto& animId : sSlingMudMotionAnimIds)
    {
        mLoadedAnims.push_back(mResMan.LoadAnimation(animId));
    }
}

SlingMudokon::SlingMudokon(relive::Path_SlingMudokon* pTlv, const Guid& tlvId, ResourceManagerWrapper& resMan, BaseMap& map)
    : BaseAliveGameObject(0, resMan, map),
    mGiveCodeBrain(*this),
    mSpawnBrain(*this),
    mAskForPasswordBrain(*this)
{
    SetType(ReliveTypes::SlingMud);

    LoadAnimations();

    mBufferStart = 0;
    mBufferIdx = -1;
    
    Animation_Init(GetAnimRes(AnimId::Mudokon_Sling_Idle));

    mCurrentMotion = eSlingMudMotions::Motion_0_Idle;

    GetAnimation().SetAnimate(false);
    GetAnimation().SetRender(false);
    GetAnimation().SetSemiTrans(true);

    FP hitX = {};
    FP hitY = {};
    if (gCollisions->Raycast(
            FP_FromInteger(pTlv->mTopLeftX),
            FP_FromInteger(pTlv->mTopLeftY),
            FP_FromInteger(pTlv->mBottomRightX),
            FP_FromInteger(pTlv->mBottomRightY),
            &BaseAliveGameObjectCollisionLine,
            &hitX,
            &hitY,
            GetSpriteScale() != FP_FromDouble(0.5) ? kFgFloor : kBgFloor)
        == 1)
    {
        mXPos = hitX;
        mYPos = hitY;
    }

    if (pTlv->mScale == relive::reliveScale::eHalf)
    {
        SetSpriteScale(FP_FromDouble(0.5));
        SetScale(Scale::Bg);
    }
    else
    {
        SetSpriteScale(FP_FromInteger(1));
        SetScale(Scale::Fg);
    }

    if (pTlv->mDontWhistlePassword)
    {
        SetBrain(ISlingMudokonBrain::EBrainTypes::AskForPassword);
    }
    else
    {
        SetBrain(ISlingMudokonBrain::EBrainTypes::Spawn);
    }

    mCodeConverted = Code_Convert(pTlv->mCode1, pTlv->mCode2);
    mCodeLength = Code_Length(mCodeConverted);

    mTlvId = tlvId;

    mAbeGettingCloser = false;
    mDontSetDestroyed = true;
}

SlingMudokon::~SlingMudokon()
{
    if (mDontSetDestroyed)
    {
        mMap.TLV_Reset(mTlvId);
    }
    else
    {
        mMap.TLV_Delete(mTlvId);
    }

    /*
    // TODO: Check it isn't 2 resources freed here
    if (mAnim.field_20_ppBlock != field_150_res)
    {
        if (field_150_res)
        {
            ResourceManager::FreeResource_455550(field_150_res);
        }
    }*/
}

void SlingMudokon::VScreenChanged()
{
    SetDead(true);
}

void SlingMudokon::VUpdate()
{
    const auto old_motion = mCurrentMotion;

    VCallBrain();

    const auto old_x = mXPos;
    const auto old_y = mYPos;

    VCallMotion();

    if (old_x != mXPos || old_y != mYPos)
    {
        BaseAliveGameObjectPathTLV = mMap.TLV_Get_At(
            TlvIterator::Invalid(),
            mXPos,
            mYPos,
            mXPos,
            mYPos);

        VOnTlvCollision(BaseAliveGameObjectPathTLV);
    }

    mbGotShot = 0;

    if (old_motion != mCurrentMotion)
    {
        VUpdateAnimData();
    }
}

void SlingMudokon::VCallBrain()
{
    if (mMap.Is_Point_In_Current_Camera(
            mCurrentLevel,
            mCurrentPath,
            mXPos,
            mYPos,
            0))
    {
        mCurrentBrain->VUpdate();
    }
    else
    {
        SetDead(true);
    }
}

void SlingMudokon::VCallMotion()
{
    InvokeMemberFunction(this, gSlingMudMotionTable_4CFCB0, mCurrentMotion);
}

void SlingMudokon::VUpdateAnimData()
{
    if (static_cast<u32>(mCurrentMotion) < 6) // out of bounds check
    {
        GetAnimation().Set_Animation_Data(GetAnimRes(sSlingMudMotionAnimIds[static_cast<u32>(mCurrentMotion)]));
    }
}

void SlingMudokon::Motion_0_Idle()
{
    if (mNextMotion == eSlingMudMotions::Motion_1_Angry)
    {
        mCurrentMotion = eSlingMudMotions::Motion_1_Angry;
        mNextMotion = eSlingMudMotions::None_m1;
    }
    else if (!GetAnimation().GetCurrentFrame() && mNextMotion == eSlingMudMotions::Motion_2_Speak)
    {
        mCurrentMotion = eSlingMudMotions::Motion_2_Speak;
        mNextMotion = eSlingMudMotions::None_m1;
    }
}

void SlingMudokon::Motion_1_Angry()
{
    if (!GetAnimation().GetCurrentFrame())
    {
        SfxPlayMono(relive::SoundEffects::SlingshotExtend, 0);
    }

    if (mNextMotion == eSlingMudMotions::Motion_3_ShootStart)
    {
        mCurrentMotion = eSlingMudMotions::Motion_3_ShootStart;
        mNextMotion = eSlingMudMotions::None_m1;
    }
    else if (mNextMotion == eSlingMudMotions::Motion_5_AngryToIdle)
    {
        mCurrentMotion = eSlingMudMotions::Motion_5_AngryToIdle;
        mNextMotion = eSlingMudMotions::None_m1;
    }
}

void SlingMudokon::Motion_2_Speak()
{
    if (GetAnimation().GetIsLastFrame())
    {
        if (GetAnimation().mAnimRes.mId == AnimId::Mudokon_Sling_Speak)
        {
            mCurrentMotion = eSlingMudMotions::Motion_0_Idle;
        }
    }
}

void SlingMudokon::Motion_3_ShootStart()
{
    if (!GetAnimation().GetCurrentFrame())
    {
        SfxPlayMono(relive::SoundEffects::SlingshotShoot, 0);
    }

    if (GetAnimation().GetIsLastFrame())
    {
        if (GetAnimation().mAnimRes.mId == AnimId::Mudokon_Sling_ShootStart)
        {
            // TODO: Check field_8_data.points[2].x
            const FP frame_x = FP_FromInteger(GetAnimation().Get_FrameHeader(-1)->mBoundMax.x);
            FP bulletXPos = {};
            FP xDistance = {};

            if (GetAnimation().GetFlipX())
            {
                bulletXPos = mXPos - frame_x;
                xDistance = -FP_FromInteger(640);
            }
            else
            {
                bulletXPos = frame_x + mXPos;
                xDistance = FP_FromInteger(640);
            }

            relive_new Bullet(
                this,
                BulletType::eNormalBullet_1,
                bulletXPos,
                mYPos - FP_FromInteger(24),
                xDistance,
                GetSpriteScale(),
                0,
                mResMan, mMap);
            mCurrentMotion = eSlingMudMotions::Motion_4_ShootEnd;
        }
    }
}

void SlingMudokon::Motion_4_ShootEnd()
{
    if (GetAnimation().GetIsLastFrame())
    {
        mCurrentMotion = eSlingMudMotions::Motion_0_Idle;
    }
}

void SlingMudokon::Motion_5_AngryToIdle()
{
    if (GetAnimation().GetIsLastFrame())
    {
        mCurrentMotion = eSlingMudMotions::Motion_0_Idle;
    }
}

GameSpeakEvents SlingMudokon::getLastIdx()
{
    return mListener.Get(*gEventSystem);
}

void SlingMudokon::SetBrain(ISlingMudokonBrain::EBrainTypes brain)
{
    switch (brain)
    {
        case ISlingMudokonBrain::EBrainTypes::GiveCode:
            mCurrentBrain = &mGiveCodeBrain;
            break;
        case ISlingMudokonBrain::EBrainTypes::Spawn:
            mCurrentBrain = &mSpawnBrain;
            break;
        case ISlingMudokonBrain::EBrainTypes::AskForPassword:
            mCurrentBrain = &mAskForPasswordBrain;
            break;

        default:
            ALIVE_FATAL("Invalid fleech brain type %d", static_cast<s32>(brain));
    }
}

void GiveCodeBrain::VUpdate()
{
    switch (mBrainState)
    {
        case EState::Init:
            mSlingMudokon.mCodePos = 0;
            mBrainState = EState::GiveCode;
            return;

        case EState::GiveCode:
            if (mSlingMudokon.mCurrentMotion != eSlingMudMotions::Motion_0_Idle || mSlingMudokon.GetAnimation().GetCurrentFrame() == 0)
            {
                mSlingMudokon.mNextMotion = eSlingMudMotions::Motion_2_Speak;
                switch (Code_LookUp(mSlingMudokon.mCodeConverted, mSlingMudokon.mCodePos, mSlingMudokon.mCodeLength))
                {
                    case GameSpeakEvents::eAbe_WhistleHigh:
                        Mudokon_SFX(MudSounds::eWhistleHigh_1, 0, 0, &mSlingMudokon, mSlingMudokon.mMap);
                        break;
                    case GameSpeakEvents::eAbe_WhistleLow:
                        Mudokon_SFX(MudSounds::eWhistleLow_2, 0, 0, &mSlingMudokon, mSlingMudokon.mMap);
                        break;
                    case GameSpeakEvents::eAbe_Fart:
                        Mudokon_SFX(MudSounds::eFart_7, 0, 300, &mSlingMudokon, mSlingMudokon.mMap);
                        break;
                    case GameSpeakEvents::eAbe_Laugh:
                        Mudokon_SFX(MudSounds::eLaugh1_8, 0, 300, &mSlingMudokon, mSlingMudokon.mMap);
                        break;
                    default:
                        break;
                }

                mSlingMudokon.mCodePos++;
                if (mSlingMudokon.mCodePos >= mSlingMudokon.mCodeLength)
                {
                    mSlingMudokon.mBufferStart = GameSpeak::FillBuffer(mSlingMudokon.mCodeConverted, mSlingMudokon.mCodeBuffer);
                    mBrainState = EState::WaitForCode;
                    return;
                }
                else
                {
                    mSlingMudokon.field_140_timer = BaseGameObject::MakeTimer(30);
                    mBrainState = EState::PauseABit;
                    return;
                }
            }
            mBrainState = EState::GiveCode;
            return;

        case EState::PauseABit:
            if (static_cast<s32>(sGnFrame) <= mSlingMudokon.field_140_timer)
            {
                if (mSlingMudokon.VIsObj_GettingNear_On_X(gAbe))
                {
                    break;
                }

                return;
            }
            mBrainState = EState::GiveCode;
            return;

        case EState::WaitForCode:
        {
            if (mSlingMudokon.VIsObj_GettingNear_On_X(gAbe))
            {
                break;
            }

            GameSpeakEvents lastIdx = mSlingMudokon.getLastIdx();
            if (lastIdx == GameSpeakEvents::eNone)
            {
                return;
            }

            mSlingMudokon.field_144_timer2 = BaseGameObject::MakeTimer(40);
            mSlingMudokon.mBufferIdx = static_cast<s16>(gEventSystem->mLastEventIndex);
            mBrainState = EState::CheckCodeMatching;
            return;
        }

        case EState::CheckCodeMatching:
            if (mSlingMudokon.VIsObj_GettingNear_On_X(gAbe))
            {
                break;
            }
            else
            {
                GameSpeakEvents lastIdx = mSlingMudokon.getLastIdx();
                if (lastIdx != GameSpeakEvents::eNone)
                {
                    mSlingMudokon.field_144_timer2 = BaseGameObject::MakeTimer(40);
                }

                if (static_cast<s32>(sGnFrame) <= mSlingMudokon.field_144_timer2)
                {
                    const auto result = gEventSystem->MatchBuffer(mSlingMudokon.mCodeBuffer, mSlingMudokon.mBufferStart, mSlingMudokon.mBufferIdx);
                    if (result != GameSpeakMatch::eFullMatch &&
                        result > GameSpeakMatch::eFullMatch)
                    {
                        return;
                    }
                }

                const GameSpeakMatch MatchBuffer = gEventSystem->MatchBuffer(mSlingMudokon.mCodeBuffer, mSlingMudokon.mBufferStart, mSlingMudokon.mBufferIdx);
                if (MatchBuffer == GameSpeakMatch::eFullMatch || CheatController::gVoiceCheat)
                {
                    mSlingMudokon.field_140_timer = BaseGameObject::MakeTimer(30);
                    mSlingMudokon.mCodeMatches = true;
                }
                else
                {
                    mSlingMudokon.field_140_timer = BaseGameObject::MakeTimer(10);
                    mSlingMudokon.mCodeMatches = false;
                }
                mBrainState = EState::RespondToProvidedCode;
            }
            return;

        case EState::RespondToProvidedCode:
            if (mSlingMudokon.GetAnimation().GetCurrentFrame() || static_cast<s32>(sGnFrame) <= mSlingMudokon.field_140_timer)
            {
                return;
            }

            if (mSlingMudokon.mCodeMatches)
            {
                Mudokon_SFX(MudSounds::eOkay_13, 0, 300, &mSlingMudokon, mSlingMudokon.mMap);
            }
            else
            {
                Mudokon_SFX(MudSounds::eRefuse_14, 0, 300, &mSlingMudokon, mSlingMudokon.mMap);
            }

            mSlingMudokon.SetBrain(ISlingMudokonBrain::EBrainTypes::Spawn);
            mSlingMudokon.mSpawnBrain.SetState(SpawnBrain::EState::ObserveAbe);

            mSlingMudokon.mNextMotion = eSlingMudMotions::Motion_2_Speak;
            return;

        default:
            return;
    }

    mSlingMudokon.mAbeGettingCloser = true;
    mSlingMudokon.SetBrain(ISlingMudokonBrain::EBrainTypes::Spawn);
    mSlingMudokon.mSpawnBrain.SetState(SpawnBrain::EState::ObserveAbe);
}

void SpawnBrain::VUpdate()
{
    switch (mBrainState)
    {
        case EState::Init:
            mSlingMudokon.field_140_timer = BaseGameObject::MakeTimer(10);
            mBrainState = EState::CreateParticle;
            return;

        case EState::CreateParticle:
            if (static_cast<s32>(sGnFrame) <= mSlingMudokon.field_140_timer)
            {
                return;
            }

            SFX_Play_Pitch(relive::SoundEffects::MenuNavigation, 45, 400);
            New_DestroyOrCreateObject_Particle(
                mSlingMudokon.mXPos,
                (mSlingMudokon.GetSpriteScale() * FP_FromInteger(20)) + mSlingMudokon.mYPos,
                mSlingMudokon.GetSpriteScale(), mSlingMudokon.mResMan, mSlingMudokon.mMap);
            mSlingMudokon.field_140_timer = BaseGameObject::MakeTimer(2);
            mBrainState = EState::CreateFlash;
            return;

        case EState::CreateFlash:
            if (static_cast<s32>(sGnFrame) > mSlingMudokon.field_140_timer)
            {
                mSlingMudokon.GetAnimation().SetAnimate(true);
                mSlingMudokon.GetAnimation().SetRender(true);
                mSlingMudokon.mCurrentMotion = eSlingMudMotions::Motion_0_Idle;

                relive_new Flash(Layer::eLayer_Above_FG1_39, 255u, 0, 255u, mSlingMudokon.mResMan, mSlingMudokon.mMap);

                if (mSlingMudokon.mXPos > gAbe->mXPos)
                {
                    mSlingMudokon.GetAnimation().SetFlipX(true);
                }
                mSlingMudokon.field_140_timer = BaseGameObject::MakeTimer(40);
                mBrainState = EState::GetAngry;
                return;
            }
            return;

        case EState::GetAngry:
            if (mSlingMudokon.VIsObj_GettingNear_On_X(gAbe))
            {
                mSlingMudokon.mNextMotion = eSlingMudMotions::Motion_1_Angry;
                mSlingMudokon.field_140_timer = BaseGameObject::MakeTimer(40);
                Mudokon_SFX(MudSounds::eAngry_5, 0, 300, &mSlingMudokon, mSlingMudokon.mMap);
                mBrainState = EState::PrepareToShoot;
                return;
            }

            if (mSlingMudokon.field_140_timer > static_cast<s32>(sGnFrame))
            {
                return;
            }
            
            mSlingMudokon.SetBrain(EBrainTypes::GiveCode);
            mSlingMudokon.mGiveCodeBrain.SetState(GiveCodeBrain::EState::Init);
            return;

        case EState::ObserveAbe:
            if (mSlingMudokon.mCodeMatches)
            {
                mSlingMudokon.field_140_timer = BaseGameObject::MakeTimer(30);
                SFX_Play_Pitch(relive::SoundEffects::PossessEffect, 0, -600);
                mBrainState = EState::DisappearAsDoves;
                return;
            }
            else if (mSlingMudokon.mAbeGettingCloser)
            {
                mSlingMudokon.mNextMotion = eSlingMudMotions::Motion_1_Angry;
                mSlingMudokon.field_140_timer = BaseGameObject::MakeTimer(40);
                Mudokon_SFX(MudSounds::eAngry_5, 0, 300, &mSlingMudokon, mSlingMudokon.mMap);
                mBrainState = EState::PrepareToShoot;
                return;
            }
            else
            {
                mSlingMudokon.field_140_timer = BaseGameObject::MakeTimer(40);
                mBrainState = EState::GetAngry;
                return;
            }
            break;

        case EState::PrepareToShoot:
            if (mSlingMudokon.VIsObjNearby((ScaleToGridSize(mSlingMudokon.GetSpriteScale()) * FP_FromInteger(4)), gAbe))
            {
                mSlingMudokon.mDontSetDestroyed = true;
                mSlingMudokon.mNextMotion = eSlingMudMotions::Motion_3_ShootStart;
                mSlingMudokon.field_140_timer = BaseGameObject::MakeTimer(15);
                mBrainState = EState::Shoot;
                return;
            }

            if (mSlingMudokon.VIsObj_GettingNear_On_X(gAbe))
            {
                mSlingMudokon.field_140_timer = BaseGameObject::MakeTimer(40);
            }
            else
            {
                if (mSlingMudokon.field_140_timer <= static_cast<s32>(sGnFrame))
                {
                    mSlingMudokon.mAbeGettingCloser = false;
                    mSlingMudokon.field_140_timer = BaseGameObject::MakeTimer(40);
                    mSlingMudokon.mNextMotion = eSlingMudMotions::Motion_5_AngryToIdle;
                    mBrainState = EState::GetAngry;
                    return;
                }
            }
            return;

        case EState::Shoot:
            if (EventGet(Event::kEventDeathReset))
            {
                mSlingMudokon.SetDead(true);
            }

            if (mSlingMudokon.field_140_timer > static_cast<s32>(sGnFrame) || gAbe->mHealth <= FP_FromInteger(0))
            {
                return;
            }

            mSlingMudokon.field_140_timer = BaseGameObject::MakeTimer(30);
            mBrainState = EState::GetAngry;
            return;

        case EState::DisappearAsDoves:
            if (static_cast<s32>(sGnFrame) >= mSlingMudokon.field_140_timer)
            {
                for (s32 i = 0; i < 8; i++)
                {
                    auto pDove = relive_new Dove(
                        AnimId::Dove_Flying,
                        mSlingMudokon.mXPos + FP_FromInteger(Math_NextRandom() % 16),
                        mSlingMudokon.mYPos - FP_FromInteger(Math_NextRandom() % 16),
                        mSlingMudokon.GetSpriteScale(), mSlingMudokon.mResMan, mSlingMudokon.mMap);
                    if (pDove)
                    {
                        if (pDove->GetAnimation().GetFlipX())
                        {
                            pDove->mXPos += FP_FromInteger(8);
                        }
                        else
                        {
                            pDove->mXPos -= FP_FromInteger(8);
                        }
                    }
                }

                SfxPlayMono(relive::SoundEffects::FlyingDoves, 0);

                mSlingMudokon.GetAnimation().SetRender(false);

                mSlingMudokon.mDontSetDestroyed = !mSlingMudokon.mCodeMatches;

                mSlingMudokon.SetDead(true);
                New_DestroyOrCreateObject_Particle(mSlingMudokon.mXPos, (mSlingMudokon.GetSpriteScale() * FP_FromInteger(20)) + mSlingMudokon.mYPos, mSlingMudokon.GetSpriteScale(), mSlingMudokon.mResMan, mSlingMudokon.mMap);

                relive_new Flash(Layer::eLayer_Above_FG1_39, 255u, 0, 255u, mSlingMudokon.mResMan, mSlingMudokon.mMap);
            }
            return;

        default:
            return;
    }
}

void AskForPasswordBrain::VUpdate()
{
    LOG_WARNING("never expected Brain_2_AskForPassword_4707B0() to be used in the original levels");
    
    // NOTE: most of this stuff seems to be inlined ( see Brain_0_GiveCode() )
    switch (mBrainState)
    {
        case EState::Unknown_0:
            mSlingMudokon.field_140_timer = BaseGameObject::MakeTimer(10);
            SFX_Play_Pitch(relive::SoundEffects::PossessEffect, 0, -600);
            mBrainState = EState::Unknown_1;
            return;

        case EState::Unknown_1:
            if (static_cast<s32>(sGnFrame) <= mSlingMudokon.field_140_timer)
            {
                return;
            }

            New_DestroyOrCreateObject_Particle(
                mSlingMudokon.mXPos,
                (mSlingMudokon.GetSpriteScale() * FP_FromInteger(20)) + mSlingMudokon.mYPos,
                mSlingMudokon.GetSpriteScale(), mSlingMudokon.mResMan, mSlingMudokon.mMap);

            mSlingMudokon.field_140_timer = BaseGameObject::MakeTimer(2);
            mBrainState = EState::Unknown_2;
            return;

        case EState::Unknown_2:
            if (static_cast<s32>(sGnFrame) > mSlingMudokon.field_140_timer)
            {
                mSlingMudokon.GetAnimation().SetAnimate(true);
                mSlingMudokon.GetAnimation().SetRender(true);

                relive_new Flash(Layer::eLayer_Above_FG1_39, 255u, 0, 255u, mSlingMudokon.mResMan, mSlingMudokon.mMap);

                mSlingMudokon.field_140_timer = BaseGameObject::MakeTimer(30);

                mSlingMudokon.mCurrentMotion = eSlingMudMotions::Motion_0_Idle;
                if (mSlingMudokon.mXPos > gAbe->mXPos)
                {
                    mSlingMudokon.GetAnimation().SetFlipX(true);
                }

                mSlingMudokon.mBufferStart = GameSpeak::FillBuffer(mSlingMudokon.mCodeConverted, mSlingMudokon.mCodeBuffer);
                mBrainState = EState::Unknown_3;
                return;
            }
            return;

        case EState::Unknown_3:
            if (mSlingMudokon.VIsObj_GettingNear_On_X(gAbe))
            {
                mSlingMudokon.mNextMotion = eSlingMudMotions::Motion_1_Angry;
                mSlingMudokon.field_140_timer = BaseGameObject::MakeTimer(40);
                Mudokon_SFX(MudSounds::eAngry_5, 0, 300, &mSlingMudokon, mSlingMudokon.mMap);
                mBrainState = EState::Unknown_7;
                return;
            }

            if (mSlingMudokon.field_140_timer > static_cast<s32>(sGnFrame) || mSlingMudokon.GetAnimation().GetCurrentFrame())
            {
                return;
            }
            Mudokon_SFX(MudSounds::ePassword_9, 0, 300, &mSlingMudokon, mSlingMudokon.mMap);
            mSlingMudokon.mNextMotion = eSlingMudMotions::Motion_2_Speak;
            mBrainState = EState::Unknown_4;
            return;

        case EState::Unknown_4:
            if (mSlingMudokon.VIsObj_GettingNear_On_X(gAbe))
            {
                mSlingMudokon.mNextMotion = eSlingMudMotions::Motion_1_Angry;
                mSlingMudokon.field_140_timer = BaseGameObject::MakeTimer(40);
                Mudokon_SFX(MudSounds::eAngry_5, 0, 300, &mSlingMudokon, mSlingMudokon.mMap);
                mBrainState = EState::Unknown_7;
                return;
            }
            else
            {
                const GameSpeakEvents speak = mSlingMudokon.getLastIdx();
                if (speak == GameSpeakEvents::eNone)
                {
                    return;
                }

                mSlingMudokon.mBufferIdx = static_cast<s16>(gEventSystem->mLastEventIndex);
                mSlingMudokon.field_144_timer2 = BaseGameObject::MakeTimer(40);
                mBrainState = EState::Unknown_5;
                return;
            }
            break;

        case EState::Unknown_5:
            if (!mSlingMudokon.VIsObj_GettingNear_On_X(gAbe))
            {
                const GameSpeakEvents speak = mSlingMudokon.getLastIdx();
                if (speak != GameSpeakEvents::eNone)
                {
                    mSlingMudokon.field_144_timer2 = BaseGameObject::MakeTimer(40);
                }

                if (static_cast<s32>(sGnFrame) <= mSlingMudokon.field_144_timer2)
                {
                    if (gEventSystem->MatchBuffer(
                            mSlingMudokon.mCodeBuffer,
                            mSlingMudokon.mBufferStart,
                            mSlingMudokon.mBufferIdx)
                        != GameSpeakMatch::eFullMatch)
                    {
                        if (gEventSystem->MatchBuffer(
                                mSlingMudokon.mCodeBuffer,
                                mSlingMudokon.mBufferStart,
                                mSlingMudokon.mBufferIdx)
                            != GameSpeakMatch::eNoMatch)
                        {
                            return;
                        }
                    }
                }

                mSlingMudokon.mCodeMatches = gEventSystem->MatchBuffer(
                                mSlingMudokon.mCodeBuffer,
                                mSlingMudokon.mBufferStart,
                                mSlingMudokon.mBufferIdx)
                             == GameSpeakMatch::eFullMatch
                         || CheatController::gVoiceCheat;

                mSlingMudokon.field_140_timer = mSlingMudokon.mCodeMatches ? BaseGameObject::MakeTimer(30) : BaseGameObject::MakeTimer(10);
                mBrainState = EState::Unknown_6;
                return;
            }

            mSlingMudokon.mNextMotion = eSlingMudMotions::Motion_1_Angry;
            mSlingMudokon.field_140_timer = BaseGameObject::MakeTimer(40);
            Mudokon_SFX(MudSounds::eAngry_5, 0, 300, &mSlingMudokon, mSlingMudokon.mMap);
            mBrainState = EState::Unknown_7;
            return;

        case EState::Unknown_6:
            if (mSlingMudokon.GetAnimation().GetCurrentFrame())
            {
                return;
            }

            if (static_cast<s32>(sGnFrame) <= mSlingMudokon.field_140_timer)
            {
                return;
            }

            mSlingMudokon.mNextMotion = eSlingMudMotions::Motion_2_Speak;

            if (!mSlingMudokon.mCodeMatches)
            {
                mSlingMudokon.field_140_timer = BaseGameObject::MakeTimer(60);
                Mudokon_SFX(MudSounds::eRefuse_14, 0, 300, &mSlingMudokon, mSlingMudokon.mMap);
                mBrainState = EState::Unknown_3;
                return;
            }
            else
            {
                mSlingMudokon.field_140_timer = BaseGameObject::MakeTimer(30);
                Mudokon_SFX(MudSounds::eOkay_13, 0, 300, &mSlingMudokon, mSlingMudokon.mMap);
                SFX_Play_Pitch(relive::SoundEffects::PossessEffect, 0, -600);
                mBrainState = EState::Unknown_9;
                return;
            }
            break;

        case EState::Unknown_7:
            if (mSlingMudokon.VIsObjNearby((ScaleToGridSize(mSlingMudokon.GetSpriteScale()) * FP_FromInteger(4)), gAbe))
            {
                mSlingMudokon.mDontSetDestroyed = true;
                mSlingMudokon.mNextMotion = eSlingMudMotions::Motion_3_ShootStart;
                mSlingMudokon.field_140_timer = BaseGameObject::MakeTimer(15);
                mBrainState = EState::Unknown_8;
                return;
            }

            if (mSlingMudokon.field_140_timer > static_cast<s32>(sGnFrame))
            {
                return;
            }

            mSlingMudokon.mNextMotion = eSlingMudMotions::Motion_5_AngryToIdle;
            mSlingMudokon.field_140_timer = BaseGameObject::MakeTimer(30);
            mBrainState = EState::Unknown_3;
            return;

        case EState::Unknown_8:
            if (EventGet(Event::kEventDeathReset))
            {
                mSlingMudokon.SetDead(true);
            }

            if (mSlingMudokon.field_140_timer > static_cast<s32>(sGnFrame) || gAbe->mHealth <= FP_FromInteger(0))
            {
                return;
            }

            mSlingMudokon.field_140_timer = BaseGameObject::MakeTimer(30);
            mBrainState = EState::Unknown_3;
            return;

        case EState::Unknown_9:
            if (static_cast<s32>(sGnFrame) >= mSlingMudokon.field_140_timer)
            {
                for (s32 i = 0; i < 8; i++)
                {
                    auto pDove = relive_new Dove(
                        AnimId::Dove_Flying,
                        mSlingMudokon.mXPos + FP_FromInteger(Math_NextRandom() % 16),
                        mSlingMudokon.mYPos - FP_FromInteger(Math_NextRandom() % 16),
                        mSlingMudokon.GetSpriteScale(), mSlingMudokon.mResMan, mSlingMudokon.mMap);;
                    if (pDove)
                    {
                        if (pDove->GetAnimation().GetFlipX())
                        {
                            pDove->mXPos += FP_FromInteger(8);
                        }
                        else
                        {
                            pDove->mXPos -= FP_FromInteger(8);
                        }
                    }
                }

                SfxPlayMono(relive::SoundEffects::Dove, 0);

                mSlingMudokon.GetAnimation().SetRender(false);

                mSlingMudokon.mDontSetDestroyed = !mSlingMudokon.mCodeMatches;

                mSlingMudokon.SetDead(true);
                New_DestroyOrCreateObject_Particle(mSlingMudokon.mXPos, (mSlingMudokon.GetSpriteScale() * FP_FromInteger(20)) + mSlingMudokon.mYPos, mSlingMudokon.GetSpriteScale(), mSlingMudokon.mResMan, mSlingMudokon.mMap);

                relive_new Flash(Layer::eLayer_Above_FG1_39, 255u, 0, 255u, mSlingMudokon.mResMan, mSlingMudokon.mMap);
            }
            return;

        default:
            return;
    }
}

void SlingMudokon::VGetSaveState(SerializedObjectData& pSaveBuffer)
{
    if (GetElectrocuted())
    {
        return;
    }

    SlingMudokonSaveState data = {};

    data.mXPos = mXPos;
    data.mYPos = mYPos;
    data.mCurrentPath = mCurrentPath;
    data.mCurrentLevel = mCurrentLevel;
    data.mSpriteScale = GetSpriteScale();
    data.mScale = GetScale();
    data.bFlipX = GetAnimation().GetFlipX();
    data.mCurrentMotion = mCurrentMotion;
    data.mNextMotion = mNextMotion;
    data.mCurrentFrame = static_cast<s32>(GetAnimation().GetCurrentFrame());
    data.mFrameChangeCounter = static_cast<u16>(GetAnimation().GetFrameChangeCounter());
    data.mRender = GetAnimation().GetRender();
    data.mAnimate = GetAnimation().GetAnimate();

    data.mTlvId = mTlvId;
    data.mCodeConverted = mCodeConverted;
    data.mCodeLength = mCodeLength;
    data.mDontSetDestroyed = mDontSetDestroyed;
    data.mAbeGettingCloser = mAbeGettingCloser;

    for (s32 i = 0; i < 16; i++)
    {
        data.mCodeBuffer[i] = mCodeBuffer[i];
    }

    data.mBufferStart = mBufferStart;
    data.mBufferIdx = mBufferIdx;
    data.mTimer140 = field_140_timer;
    data.mTimer144 = field_144_timer2;
    data.mPreviousGiveCodeState = field_154_previous_brain_state;
    data.mCodePos = mCodePos;
    data.mCodeMatches = mCodeMatches;

    data.mActiveBrain = mCurrentBrain->VGetBrain();
    data.mGiveCodeState = mGiveCodeBrain.State();
    data.mSpawnState = mSpawnBrain.State();
    data.mAskForPasswordState = mAskForPasswordBrain.State();

    pSaveBuffer.Write(data);
}

void SlingMudokon::CreateFromSaveState(SerializedObjectData& pBuffer, ResourceManagerWrapper& resMan, BaseMap& map)
{
    const auto pState = pBuffer.ReadTmpPtr<SlingMudokonSaveState>();
    auto tlvIterator = map.TLV_From_Offset_Lvl_Cam(pState->mTlvId);
    auto pTlv = tlvIterator.GetTlvChecked<relive::Path_SlingMudokon>(ReliveTypes::SlingMud);

    auto pSlingMud = relive_new SlingMudokon(pTlv, pState->mTlvId, resMan, map);
    if (pSlingMud)
    {
        pSlingMud->BaseAliveGameObjectPathTLV = TlvIterator::Invalid();
        pSlingMud->mXPos = pState->mXPos;
        pSlingMud->mYPos = pState->mYPos;
        pSlingMud->mCurrentPath = pState->mCurrentPath;
        pSlingMud->mCurrentLevel = pState->mCurrentLevel;
        pSlingMud->SetSpriteScale(pState->mSpriteScale);
        pSlingMud->SetScale(pState->mScale);
        pSlingMud->GetAnimation().SetFlipX(pState->bFlipX);
        pSlingMud->mCurrentMotion = pState->mCurrentMotion;
        pSlingMud->mNextMotion = pState->mNextMotion;
        pSlingMud->GetAnimation().Set_Animation_Data(pSlingMud->GetAnimRes(sSlingMudMotionAnimIds[static_cast<u32>(pState->mCurrentMotion)]));
        pSlingMud->GetAnimation().SetCurrentFrame(pState->mCurrentFrame);
        pSlingMud->GetAnimation().SetFrameChangeCounter(pState->mFrameChangeCounter);
        pSlingMud->GetAnimation().SetRender(pState->mRender);
        pSlingMud->GetAnimation().SetAnimate(pState->mAnimate);

        if (IsLastFrame(&pSlingMud->GetAnimation()))
        {
            pSlingMud->GetAnimation().SetIsLastFrame(true);
        }

        pSlingMud->mTlvId = pState->mTlvId;
        pSlingMud->mCodeConverted = pState->mCodeConverted;
        pSlingMud->mCodeLength = pState->mCodeLength;
        pSlingMud->mDontSetDestroyed = pState->mDontSetDestroyed;
        pSlingMud->mAbeGettingCloser = pState->mAbeGettingCloser;

        for (s32 i = 0; i < 16; i++)
        {
            pSlingMud->mCodeBuffer[i] = pState->mCodeBuffer[i];
        }

        pSlingMud->mBufferStart = pState->mBufferStart;
        pSlingMud->mBufferIdx = pState->mBufferIdx;
        pSlingMud->field_140_timer = pState->mTimer140;
        pSlingMud->field_144_timer2 = pState->mTimer144;
        pSlingMud->field_154_previous_brain_state = pState->mPreviousGiveCodeState;
        pSlingMud->mCodePos = pState->mCodePos;
        pSlingMud->mCodeMatches = pState->mCodeMatches;

        pSlingMud->mGiveCodeBrain.SetState(pState->mGiveCodeState);
        pSlingMud->mSpawnBrain.SetState(pState->mSpawnState);
        pSlingMud->mAskForPasswordBrain.SetState(pState->mAskForPasswordState);
        pSlingMud->SetBrain(pState->mActiveBrain);
    }
}

} // namespace AO
