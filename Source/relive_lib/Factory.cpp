#include "BinaryPath.hpp"
#include "MapWrapper.hpp"
#include "../AliveLibAO/Map.hpp"
#include "stdafx.h"
#include "Factory.hpp"

#include "ResourceManagerWrapper.hpp"
#include "GameType.hpp"

#include "../AliveLibAE/stdlib.hpp"

#include "../relive_lib/Function.hpp"
#include "../relive_lib/GameObjects/Mine.hpp"
#include "../relive_lib/GameObjects/TimedMine.hpp"
#include "../relive_lib/GameObjects/BackgroundAnimation.hpp"
#include "../relive_lib/GameObjects/Alarm.hpp"
#include "../relive_lib/SwitchStates.hpp"
#include "../relive_lib/GameObjects/RollingBallStopper.hpp"
#include "../relive_lib/GameObjects/ZzzSpawner.hpp"
#include "../relive_lib/GameObjects/BoomMachine.hpp"
#include "../relive_lib/GameObjects/ShadowZone.hpp"
#include "../relive_lib/GameObjects/TrapDoor.hpp"
#include "../relive_lib/GameObjects/TimerTrigger.hpp"
#include "../relive_lib/GameObjects/ElectricWall.hpp"
#include "../relive_lib/GameObjects/GasEmitter.hpp"
#include "../relive_lib/GameObjects/Dove.hpp"
#include "../relive_lib/GameObjects/GasCountDown.hpp"
#include "../relive_lib/GameObjects/Door.hpp"

#include "../relive_lib/Engine.hpp"
#include "../AliveLibAO/Abe.hpp"
#include "../AliveLibAO/Elum.hpp"
#include "../AliveLibAO/PauseMenu.hpp"
#include "../AliveLibAO/ZBall.hpp"
#include "../AliveLibAO/SligSpawner.hpp"
#include "../AliveLibAO/Slig.hpp"
#include "../AliveLibAO/Bat.hpp"
#include "../AliveLibAO/BellHammer.hpp"
#include "../AliveLibAO/LiftMover.hpp"
#include "../AliveLibAO/DoorLight.hpp"
#include "../AliveLibAO/LightEffect.hpp"
#include "../AliveLibAO/ChimeLock.hpp"
#include "../AliveLibAO/CreditsController.hpp"
#include "../AliveLibAO/Meat.hpp"
#include "../AliveLibAO/Math.hpp"
#include "../AliveLibAO/MusicTrigger.hpp"
#include "../AliveLibAO/SecurityOrb.hpp"
#include "../AliveLibAO/SecurityClaw.hpp"
#include "../AliveLibAO/BeeSwarmHole.hpp"
#include "../AliveLibAO/BeeNest.hpp"
#include "../AliveLibAO/Honey.hpp"
#include "../AliveLibAO/HoneySack.hpp"
#include "../AliveLibAO/PullRingRope.hpp"
#include "../AliveLibAO/InvisibleSwitch.hpp"
#include "../AliveLibAO/FallingItem.hpp"
#include "../AliveLibAO/FootSwitch.hpp"
#include "../AliveLibAO/HoistRocksEffect.hpp"
#include "../AliveLibAO/RollingBall.hpp"
#include "../AliveLibAO/Lever.hpp"
#include "../AliveLibAO/SecurityDoor.hpp"
#include "../AliveLibAO/BackgroundGlukkon.hpp"
#include "../AliveLibAO/Well.hpp"
#include "../AliveLibAO/BirdPortal.hpp"
#include "../AliveLibAO/SlogSpawner.hpp"
#include "../AliveLibAO/DoorFlame.hpp"
#include "../AliveLibAO/MotionDetector.hpp"
#include "../AliveLibAO/LCDStatusBoard.hpp"
#include "../AliveLibAO/FlintLockFire.hpp"
#include "../AliveLibAO/MeatSaw.hpp"
#include "../AliveLibAO/Mudokon.hpp"
#include "../AliveLibAO/HintFly.hpp"
#include "../AliveLibAO/SlingMudokon.hpp"
#include "../AliveLibAO/MainMenu.hpp"
#include "../AliveLibAO/DDCheat.hpp"
#include "../AliveLibAO/Slog.hpp"
#include "../AliveLibAO/LCDScreen.hpp"
#include "../AliveLibAO/UXB.hpp"
#include "../AliveLibAO/Scrab.hpp"
#include "../AliveLibAO/Paramite.hpp"
#include "../AliveLibAO/MovingBomb.hpp"
#include "../AliveLibAO/LiftPoint.hpp"
#include "../AliveLibAO/RockSack.hpp"
#include "../AliveLibAO/MeatSack.hpp"
#include "../AliveLibAO/Path.hpp"

#include "../AliveLibAE/MainMenu.hpp"
#include "../AliveLibAE/Map.hpp"
#include "../AliveLibAE/LCDScreen.hpp"
#include "../AliveLibAE/UXB.hpp"
#include "../AliveLibAE/LCDStatusBoard.hpp"
#include "../AliveLibAE/HoistRocksEffect.hpp"
#include "../AliveLibAE/WorkWheel.hpp"
#include "../AliveLibAE/StatusLight.hpp"
#include "../AliveLibAE/WheelSyncer.hpp"
#include "../AliveLibAE/LevelLoader.hpp"
#include "../AliveLibAE/InvisibleSwitch.hpp"
#include "../AliveLibAE/Lever.hpp"
#include "../AliveLibAE/MultiSwitchController.hpp"
#include "../AliveLibAE/Drill.hpp"
#include "../AliveLibAE/Teleporter.hpp"
#include "../AliveLibAE/Well.hpp"
#include "../AliveLibAE/Water.hpp"
#include "../AliveLibAE/SlamDoor.hpp"
#include "../AliveLibAE/MusicTrigger.hpp"
#include "../AliveLibAE/PauseMenu.hpp"
#include "../AliveLibAE/Abe.hpp"
#include "../AliveLibAE/LiftPoint.hpp"
#include "../AliveLibAE/PullRingRope.hpp"
#include "../AliveLibAE/LiftMover.hpp"
#include "../AliveLibAE/CreditsController.hpp"
#include "../AliveLibAE/FootSwitch.hpp"
#include "../AliveLibAE/BrewMachine.hpp"
#include "../AliveLibAE/FallingItem.hpp"
#include "../AliveLibAE/MineCar.hpp"
#include "../AliveLibAE/DemoPlayback.hpp"
#include "../AliveLibAE/SecurityOrb.hpp"
#include "../AliveLibAE/Greeter.hpp"
#include "../AliveLibAE/MotionDetector.hpp"
#include "../AliveLibAE/FlyingSlig.hpp"
#include "../AliveLibAE/FlyingSligSpawner.hpp"
#include "../AliveLibAE/Mudokon.hpp"
#include "../AliveLibAE/BirdPortal.hpp"
#include "../AliveLibAE/TorturedMudokon.hpp"
#include "../AliveLibAE/DoorBlocker.hpp"
#include "../AliveLibAE/SlapLock.hpp"
#include "../AliveLibAE/Glukkon.hpp"
#include "../AliveLibAE/MovingBomb.hpp"
#include "../AliveLibAE/Slurg.hpp"
#include "../AliveLibAE/DoorFlame.hpp"
#include "../AliveLibAE/CrawlingSlig.hpp"
#include "../AliveLibAE/Scrab.hpp"
#include "../AliveLibAE/Paramite.hpp"
#include "../AliveLibAE/Fleech.hpp"
#include "../AliveLibAE/Slog.hpp"
#include "../AliveLibAE/Slig.hpp"
#include "../AliveLibAE/GlukkonSwitch.hpp"
#include "../AliveLibAE/CrawlingSligButton.hpp"
#include "../AliveLibAE/SligGetPantsAndWings.hpp"
#include "../AliveLibAE/Meat.hpp"
#include "../AliveLibAE/Bone.hpp"
#include "../AliveLibAE/RockSack.hpp"
#include "../AliveLibAE/SligSpawner.hpp"
#include "../AliveLibAE/ScrabSpawner.hpp"
#include "../AliveLibAE/SlogSpawner.hpp"
#include "../AliveLibAE/ParamiteWebLine.hpp"
#include "../AliveLibAE/ExplosionSet.hpp"
#include "../AliveLibAE/ColourfulMeter.hpp"
#include "../AliveLibAE/SecurityDoor.hpp"
#include "../AliveLibAE/LaughingGas.hpp"
#include "../relive_lib/Engine.hpp"
#include "../AliveLibAE/MeatSack.hpp"
#include "../AliveLibAE/BoneBag.hpp"
#include "../AliveLibAE/RollingBall.hpp"

namespace relive {

static constexpr AnimId kAbeGibs[3] = {
    AnimId::Abe_Head_Gib,
    AnimId::Abe_Arm_Gib,
    AnimId::Abe_Body_Gib};

static constexpr AnimId kSlogGibs[2] = {
    AnimId::Slog_Head_Gib,
    AnimId::Slog_Body_Gib};

static constexpr AnimId kElumGibs[3] = {
    AnimId::Elum_Head_Gib,
    AnimId::Elum_Arm_Gib,
    AnimId::Elum_Body_Gib};

static constexpr AnimId kSligGibs[3] = {
    AnimId::Slig_Head_Gib,
    AnimId::Slig_Arm_Gib,
    AnimId::Slig_Body_Gib};

void Factory::HoistAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        switch (map.mCurrentLevel)
        {
            case EReliveLevelIds::eRuptureFarms:
            case EReliveLevelIds::eRuptureFarmsReturn:
                resMan.PendAnimation(AnimId::RuptureFarms_HoistRock1);
                resMan.PendAnimation(AnimId::RuptureFarms_HoistRock2);
                resMan.PendAnimation(AnimId::RuptureFarms_HoistRock3);
                break;

            default:
                resMan.PendAnimation(AnimId::AO_HoistRock1);
                resMan.PendAnimation(AnimId::AO_HoistRock2);
                resMan.PendAnimation(AnimId::AO_HoistRock3);
                break;
        }
    }
    else
    {
        auto pHoistTlv = static_cast<Path_Hoist*>(pTlv);
        if (pHoistTlv->mHoistType == Path_Hoist::Type::eOffScreen)
        {
            relive_new AO::HoistRocksEffect(pHoistTlv, tlvId, resMan, map);
            // OG issue, no reset on failure ??
        }
        else
        {
            map.TLV_Reset(tlvId);
        }
    }
}

void Factory::EdgeAO(Path_TLV* /*pTlv*/, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper&, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        // Empty
    }
    else
    {
        map.TLV_Reset(tlvId);
    }
}

void Factory::ShadowZoneAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode != LoadMode::LoadResourceFromList_1 && loadMode != LoadMode::LoadResource_2)
    {
        relive_new ShadowZone(static_cast<Path_ShadowZone*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::LiftPointAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        switch (map.mCurrentLevel)
        {
            case EReliveLevelIds::eRuptureFarms:
            case EReliveLevelIds::eBoardRoom:
            case EReliveLevelIds::eRuptureFarmsReturn:
                resMan.PendAnimation(AnimId::Rope_R1);
                resMan.PendAnimation(AnimId::LiftPlatform_RuptureFarms);
                resMan.PendAnimation(AnimId::LiftBottomWheel_RuptureFarms);
                resMan.PendAnimation(AnimId::LiftTopWheel_RuptureFarms);
                break;

            case EReliveLevelIds::eLines:
                resMan.PendAnimation(AnimId::Rope_Lines);
                resMan.PendAnimation(AnimId::LiftPlatform_Lines);
                resMan.PendAnimation(AnimId::LiftBottomWheel_Lines);
                resMan.PendAnimation(AnimId::LiftTopWheel_Lines);
                break;

            case EReliveLevelIds::eDesert:
                resMan.PendAnimation(AnimId::Rope_R1);
                resMan.PendAnimation(AnimId::LiftPlatform_Desert);
                resMan.PendAnimation(AnimId::LiftBottomWheel_Desert);
                resMan.PendAnimation(AnimId::LiftTopWheel_Desert);
                break;

            case EReliveLevelIds::eDesertTemple:
            case EReliveLevelIds::eDesertEscape:
                resMan.PendAnimation(AnimId::Rope_R1);
                resMan.PendAnimation(AnimId::LiftPlatform_Desert2);
                resMan.PendAnimation(AnimId::LiftBottomWheel_Desert2);
                resMan.PendAnimation(AnimId::LiftTopWheel_Desert2);
                break;

            default:
                resMan.PendAnimation(AnimId::Rope_Lines);
                resMan.PendAnimation(AnimId::LiftPlatform_Forest);
                resMan.PendAnimation(AnimId::LiftBottomWheel_Forest);
                resMan.PendAnimation(AnimId::LiftTopWheel_Forest);
                break;
        }
    }
    else
    {
        for (s32 idx = 0; idx < gBaseGameObjects->Size(); idx++)
        {
            BaseGameObject* pObjIter = gBaseGameObjects->ItemAt(idx);
            if (!pObjIter)
            {
                break;
            }

            if (!pObjIter->GetDead() && pObjIter->Type() == ReliveTypes::eLiftPoint)
            {
                auto pLiftObj = static_cast<AO::LiftPoint*>(pObjIter);

                const s16 xpos_i = FP_GetExponent(pLiftObj->mXPos);
                if (pTlv->mTopLeftX <= xpos_i
                    && xpos_i <= pTlv->mBottomRightX
                    && pLiftObj->mCurrentLevel == map.mCurrentLevel
                    && pLiftObj->mCurrentPath == map.mCurrentPath)
                {
                    map.TLV_Reset(tlvId);
                    return;
                }
            }
        }

        if (pTlv->mTlvSpecificMeaning & 2 || (pTlv->mTlvSpecificMeaning == 0 && static_cast<Path_LiftPoint*>(pTlv)->mIsStartPoint))
        {
            relive_new AO::LiftPoint(static_cast<Path_LiftPoint*>(pTlv), tlvId, resMan, map);
        }
        else
        {
            TlvIterator tlvIterator = TlvIterator::Invalid();
            s16 pointNumber = 1;
            while (pointNumber < 8)
            {
                tlvIterator = map.Get_First_TLV_For_Offsetted_Camera(
                    0,
                    pointNumber / 2 * (pointNumber % 2 != 0 ? -1 : 1));
                while (tlvIterator.GetTlv())
                {
                    if (tlvIterator.GetTlv()->mTlvType == ReliveTypes::eLiftPoint)
                    {
                        const auto tlv_x = pTlv->mTopLeftX;
                        const auto absX = tlvIterator.GetTlv()->mTopLeftX - tlv_x >= 0 ? tlvIterator.GetTlv()->mTopLeftX - tlv_x : tlv_x - tlvIterator.GetTlv()->mTopLeftX;
                        if (absX < 5)
                        {
                            if (tlvIterator.GetTlv()->mTlvSpecificMeaning & 2 || (tlvIterator.GetTlv()->mTlvSpecificMeaning == 0 && tlvIterator.GetTlv<Path_LiftPoint>()->mIsStartPoint))
                            {
                                relive_new AO::LiftPoint(tlvIterator.GetTlv<Path_LiftPoint>(), tlvId, resMan, map);
                                return;
                            }
                        }
                    }
                    tlvIterator = tlvIterator.Next_TLV();
                }
                pointNumber++;
            }

            relive_new AO::LiftPoint(static_cast<Path_LiftPoint*>(pTlv), tlvId, resMan, map);
        }
    }
}

void Factory::WellAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::Well_Leaf);
    }
    else
    {
        const FP xpos = FP_FromInteger(pTlv->mTopLeftX);
        const FP ypos = FP_FromInteger(pTlv->mTopLeftY);
        relive_new AO::Well(static_cast<Path_WellBase*>(pTlv), xpos, ypos, tlvId, resMan, map);
    }
}

void Factory::DoveAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode != LoadMode::LoadResourceFromList_1 && loadMode != LoadMode::LoadResource_2)
    {

        auto pDoveTlv = static_cast<Path_Dove*>(pTlv);

        const s16 width = pDoveTlv->Width();
        const s16 height = pDoveTlv->Height();

        for (s32 i = 0; i < pDoveTlv->mDoveCount; i++)
        {
            auto pDove = relive_new Dove(
                AnimId::Dove_Idle,
                tlvId,
                pDoveTlv->mScale != reliveScale::eFull ? FP_FromDouble(0.5) : FP_FromInteger(1),
                resMan, map);

            if (pDove)
            {
                s16 ypos = 0;
                if (pDoveTlv->mPixelPerfect)
                {
                    pDove->mXPos = FP_FromInteger(pDoveTlv->mTopLeftX);
                    ypos = pDoveTlv->mTopLeftY;
                }
                else
                {
                    pDove->mXPos = FP_FromInteger(pDoveTlv->mTopLeftX + width * Math_NextRandom() / 256);
                    ypos = pDoveTlv->mTopLeftY + height * Math_NextRandom() / 256;
                }

                pDove->mYPos = FP_FromInteger(ypos) + FP_FromInteger(10);
            }
        }
    }
    else
    {
        resMan.PendAnimation(AnimId::Dove_Idle);
        resMan.PendAnimation(AnimId::Dove_Flying);
    }
}

void Factory::RockSackAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::Rock);
        resMan.PendAnimation(AnimId::RockSack_Idle);
        resMan.PendAnimation(AnimId::RockSack_SoftHit);
        resMan.PendAnimation(AnimId::RockSack_HardHit);

        if (map.mCurrentLevel == EReliveLevelIds::eStockYards || map.mCurrentLevel == EReliveLevelIds::eStockYardsReturn)
        {
        }
    }
    else
    {
        relive_new AO::RockSack(static_cast<Path_RockSack*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::ZBallAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::Swinging_Ball_Fast);
        resMan.PendAnimation(AnimId::Swinging_Ball_Normal);
        resMan.PendAnimation(AnimId::Swinging_Ball_Slow);
    }
    else
    {
        relive_new AO::ZBall(static_cast<Path_ZBall*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::FallingItemAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::Explosion_Stick);
        switch (map.mCurrentLevel)
        {
            case EReliveLevelIds::eRuptureFarms:
            case EReliveLevelIds::eRuptureFarmsReturn:
                resMan.PendAnimation(AnimId::FallingMeat_Waiting);
                resMan.PendAnimation(AnimId::FallingMeat_Falling);
                resMan.PendAnimation(AnimId::Meat_Gib);
                break;

            case EReliveLevelIds::eLines:
            case EReliveLevelIds::eBoardRoom:
            case EReliveLevelIds::eStockYards:
            case EReliveLevelIds::eDesert:
            case EReliveLevelIds::eDesertTemple:
            case EReliveLevelIds::eDesertEscape:
                resMan.PendAnimation(AnimId::AO_FallingRock_Falling);
                resMan.PendAnimation(AnimId::AO_FallingRock_Waiting);
                resMan.PendAnimation(AnimId::Explosion_Rock);
                break;

            default:
                resMan.PendAnimation(AnimId::AO_FallingRock_Falling);
                resMan.PendAnimation(AnimId::AO_FallingRock_Waiting);
                resMan.PendAnimation(AnimId::Explosion_Rock);
                break;
        }
    }
    else
    {
        relive_new AO::FallingItem(static_cast<Path_FallingItem*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::PullRingRopeAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        switch (map.mCurrentLevel)
        {
            case EReliveLevelIds::eRuptureFarms:
            case EReliveLevelIds::eBoardRoom:
            case EReliveLevelIds::eRuptureFarmsReturn:
                resMan.PendAnimation(AnimId::Rope_R1);
                resMan.PendAnimation(AnimId::Pullring_Farms_Idle);
                resMan.PendAnimation(AnimId::Pullring_Farms_UseBegin);
                resMan.PendAnimation(AnimId::Pullring_Farms_UseEnd);
                break;

            case EReliveLevelIds::eDesert:
            case EReliveLevelIds::eDesertTemple:
                resMan.PendAnimation(AnimId::Rope_R1);
                resMan.PendAnimation(AnimId::Pullring_Desert_Idle);
                resMan.PendAnimation(AnimId::Pullring_Desert_UseBegin);
                resMan.PendAnimation(AnimId::Pullring_Desert_UseEnd);
                break;

            default:
                resMan.PendAnimation(AnimId::Rope_Lines);
                resMan.PendAnimation(AnimId::Pullring_Desert_Idle);
                resMan.PendAnimation(AnimId::Pullring_Desert_UseBegin);
                resMan.PendAnimation(AnimId::Pullring_Desert_UseEnd);
                break;
        }
    }
    else
    {
        relive_new AO::PullRingRope(static_cast<Path_PullRingRope*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::BackgroundAnimationAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode != LoadMode::LoadResourceFromList_1 && loadMode != LoadMode::LoadResource_2)
    {
        auto pBgAnim = static_cast<Path_BackgroundAnimation*>(pTlv);
        //resMan.PendAnimation(static_cast<AnimId>(pBgAnim->mAnimId));
        relive_new BackgroundAnimation(pBgAnim, tlvId, resMan, map);
    }
}

void Factory::HoneyAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::Honey);
        resMan.PendAnimation(AnimId::Honey_Drip);
    }
    else
    {

        const auto midPoint = pTlv->Width() / 2;
        auto pHoney = relive_new AO::Honey(
            FP_FromInteger(midPoint + pTlv->mTopLeftX),
            FP_FromInteger(pTlv->mTopLeftY + 24), resMan, map);
        if (pHoney)
        {
            pHoney->mTlvInfo = tlvId;
        }
    }
}

void Factory::TimedMineAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    auto pTimedMineTlv = static_cast<Path_TimedMine*>(pTlv);
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnims(kAbeGibs);
        resMan.PendAnims(kSlogGibs);
        resMan.PendAnims(kElumGibs);
        resMan.PendAnimation(AnimId::TimedMine_Activated);
        resMan.PendAnimation(AnimId::TimedMine_Idle);
        resMan.PendAnimation(AnimId::Bomb_Flash);
        resMan.PendAnimation(AnimId::Bomb_RedGreenTick);
        resMan.PendAnimation(AnimId::Explosion_Rock);
        resMan.PendAnimation(AnimId::GroundExplosion);
        if (map.mCurrentLevel == EReliveLevelIds::eStockYards || map.mCurrentLevel == EReliveLevelIds::eStockYardsReturn)
        {
        }

    }
    else
    {
        relive_new TimedMine(pTimedMineTlv, tlvId, resMan, map);
    }
}

static void LoadWalkingSligResourcesAO(Factory::LoadMode loadMode, BitField16<AO::SligFlags_DisabledRes> disabledResources, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == Factory::LoadMode::LoadResourceFromList_1 || loadMode == Factory::LoadMode::LoadResource_2)
    {
        resMan.PendAnims(AO::Slig::sSligMotionAnimIds);
        resMan.PendAnims(kSligGibs);
        resMan.PendAnimation(AnimId::ObjectShadow);
        resMan.PendAnimation(AnimId::ShootingZFire_Particle);
        resMan.PendAnimation(AnimId::ShootingFire_Particle);
        resMan.PendAnimation(AnimId::Bullet_Shell);

        if (map.mCurrentLevel == EReliveLevelIds::eStockYards || map.mCurrentLevel == EReliveLevelIds::eStockYardsReturn)
        {
        }
    }
    else
    {
        if (!(disabledResources.Get(AO::eDisabledRes_Bit1_SligLever)))
        {
        }

        if (!(disabledResources.Get(AO::eDisabledRes_Bit2_SligLift)))
        {
        }

        if (!(disabledResources.Get(AO::eDisabledRes_Bit3_SligZ)))
        {
        }

        if (!(disabledResources.Get(AO::eDisabledRes_Bit7_SligSleep)))
        {
        }

        if (!(disabledResources.Get(AO::eDisabledRes_Bit9_SligEdge)))
        {
        }

        if (!(disabledResources.Get(AO::eDisabledRes_Bit10_SligSmash)))
        {
        }

        if (!(disabledResources.Get(AO::eDisabledRes_Bit11_SligBeat)))
        {
        }

        if (!(disabledResources.Get(AO::eDisabledRes_Bit8_SligKnfd)))
        {
        }

    }
}

void Factory::SligAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    auto pSligTlv = static_cast<Path_Slig*>(pTlv);
    LoadWalkingSligResourcesAO(loadMode, pSligTlv->mDisabledResourcesAO, resMan, map);

    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        // Do nothing
    }
    else
    {
        relive_new AO::Slig(pSligTlv, tlvId, resMan, map);
    }
}


void Factory::SlogAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnims(AO::Slog::sSlogMotionAnimIds);
    }
    else
    {
        relive_new AO::Slog(static_cast<Path_Slog*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::LeverAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        switch (map.mCurrentLevel)
        {
            case EReliveLevelIds::eRuptureFarms:
            case EReliveLevelIds::eStockYards:
            case EReliveLevelIds::eStockYardsReturn:
            case EReliveLevelIds::eBoardRoom:
            case EReliveLevelIds::eRuptureFarmsReturn:
                resMan.PendAnimation(AnimId::RuptureFarms_Lever_Idle);
                resMan.PendAnimation(AnimId::RuptureFarms_Lever_Pull_Left);
                resMan.PendAnimation(AnimId::RuptureFarms_Lever_Pull_Release_Left);
                resMan.PendAnimation(AnimId::RuptureFarms_Lever_Pull_Right);
                resMan.PendAnimation(AnimId::RuptureFarms_Lever_Pull_Release_Right);
                break;

            case EReliveLevelIds::eLines:
                resMan.PendAnimation(AnimId::Lines_Lever_Idle);
                resMan.PendAnimation(AnimId::Lines_Lever_Pull_Left);
                resMan.PendAnimation(AnimId::Lines_Lever_Pull_Release_Left);
                resMan.PendAnimation(AnimId::Lines_Lever_Pull_Right);
                resMan.PendAnimation(AnimId::Lines_Lever_Pull_Release_Right);
                break;

            case EReliveLevelIds::eDesert:
            case EReliveLevelIds::eDesertTemple:
            case EReliveLevelIds::eDesertEscape:
                resMan.PendAnimation(AnimId::Lever_Idle);
                resMan.PendAnimation(AnimId::Lever_Pull_Left);
                resMan.PendAnimation(AnimId::Lever_Pull_Release_Left);
                resMan.PendAnimation(AnimId::Lever_Pull_Right);
                resMan.PendAnimation(AnimId::Lever_Pull_Release_Right);
                break;

            default:
                resMan.PendAnimation(AnimId::Lever_Idle);
                resMan.PendAnimation(AnimId::Lever_Pull_Left);
                resMan.PendAnimation(AnimId::Lever_Pull_Release_Left);
                resMan.PendAnimation(AnimId::Lever_Pull_Right);
                resMan.PendAnimation(AnimId::Lever_Pull_Release_Right);
                break;
        }
    }
    else
    {
        relive_new AO::Lever(static_cast<Path_Lever*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::BellHammerAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode != LoadMode::LoadResourceFromList_1 && loadMode != LoadMode::LoadResource_2)
    {
        relive_new AO::BellHammer(static_cast<Path_BellHammer*>(pTlv), tlvId, resMan, map);
    }
    else
    {
        resMan.PendAnimation(AnimId::BellHammer_Idle);
        resMan.PendAnimation(AnimId::BellHammer_Smashing);
        resMan.PendAnims(AO::Elum::gElumMotionAnimIds);
    }
}

void Factory::SecurityOrbAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    auto pSecurityOrbTlv = static_cast<Path_SecurityOrb*>(pTlv);
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::Security_Orb);
        resMan.PendAnimation(AnimId::Zap_Line_Blue);
        resMan.PendAnimation(AnimId::Zap_Line_Red);
        resMan.PendAnims(kAbeGibs);
        resMan.PendAnims(kSlogGibs);
        resMan.PendAnims(kElumGibs);
        resMan.PendAnimation(AnimId::Metal_Gib);
        resMan.PendAnimation(AnimId::AirExplosion);
    }
    else
    {
        relive_new AO::SecurityOrb(pSecurityOrbTlv, tlvId, resMan, map);
    }
}

void Factory::LiftMudAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnims(AO::Mudokon::sMudMotionAnimIds);
    }
    else
    {
        relive_new AO::Mudokon(pTlv, tlvId, resMan, map);
    }
}

void Factory::BeeSwarmHoleAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::Bee_Swarm);
        if (map.mCurrentLevel == EReliveLevelIds::eForest || map.mCurrentLevel == EReliveLevelIds::eDesert)
        {
        }
    }
    else
    {
        relive_new AO::BeeSwarmHole(static_cast<Path_BeeSwarmHole*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::HoneySackAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::Honey);
        resMan.PendAnimation(AnimId::Honey_Drip);
        resMan.PendAnimation(AnimId::HoneySack_Hanging);
        resMan.PendAnimation(AnimId::HoneySack_OnGround);
        resMan.PendAnimation(AnimId::HoneySack_Falling);
        resMan.PendAnimation(AnimId::HoneySack_FallingToSmashed);
    }
    else
    {
        relive_new AO::HoneySack(static_cast<Path_HoneySack*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::AbeStartAO(Path_TLV* pTlv, const Guid& /*tlvId*/, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode != LoadMode::LoadResourceFromList_1 && loadMode != LoadMode::LoadResource_2 && !gAttract)
    {
        if (!AO::gPauseMenu)
        {
            AO::gPauseMenu = relive_new AO::PauseMenu(resMan, map);
        }

        if (!AO::gAbe)
        {
            AO::gAbe = relive_new AO::Abe(resMan, map);
            if (AO::gAbe)
            {
                AO::gAbe->mXPos = FP_FromInteger(pTlv->mTopLeftX + 12);
                AO::gAbe->mYPos = FP_FromInteger(pTlv->mTopLeftY);
            }
        }
    }
    else
    {
        resMan.PendAnims(AO::Abe::sAbeMotionAnimIds);
    }
}

void Factory::ElumWallAO(Path_TLV* /*pTlv*/, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper&, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
    }
    else
    {
        map.TLV_Reset(tlvId);
    }
}

void Factory::SlingMudAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode != LoadMode::LoadResourceFromList_1 && loadMode != LoadMode::LoadResource_2)
    {
        relive_new AO::SlingMudokon(static_cast<Path_SlingMudokon*>(pTlv), tlvId, resMan, map);
    }
    else
    {
        resMan.PendAnims(AO::SlingMudokon::sSlingMudMotionAnimIds);
        // kSparksAOResID was loaded but there is no AnimId entry for it?...
    }
}

void Factory::BeeNestAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::Bee_Swarm);
    }
    else
    {
        relive_new AO::BeeNest(static_cast<Path_BeeNest*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::MineAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    auto pMineTlv = static_cast<Path_Mine*>(pTlv);

    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnims(kAbeGibs);
        resMan.PendAnims(kSlogGibs);
        resMan.PendAnims(kElumGibs);
        resMan.PendAnimation(AnimId::Mine);
        resMan.PendAnimation(AnimId::Mine_Flash);
        resMan.PendAnimation(AnimId::Explosion_Rock);
        resMan.PendAnimation(AnimId::GroundExplosion);
        if (map.mCurrentLevel == EReliveLevelIds::eStockYards || map.mCurrentLevel == EReliveLevelIds::eStockYardsReturn)
        {
        }
    }
    else
    {
        relive_new Mine(pMineTlv, tlvId, resMan, map);
    }
}

void Factory::UXBAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    auto pUxbTlv = static_cast<Path_UXB*>(pTlv);

    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnims(kAbeGibs);
        resMan.PendAnims(kSlogGibs);
        resMan.PendAnims(kElumGibs);
        resMan.PendAnimation(AnimId::UXB_Active);
        resMan.PendAnimation(AnimId::UXB_Toggle);
        resMan.PendAnimation(AnimId::UXB_Disabled);
        resMan.PendAnimation(AnimId::Bomb_Flash);
        resMan.PendAnimation(AnimId::Bomb_RedGreenTick);
        resMan.PendAnimation(AnimId::Explosion_Rock);
        resMan.PendAnimation(AnimId::GroundExplosion);
        if (map.mCurrentLevel == EReliveLevelIds::eStockYards || map.mCurrentLevel == EReliveLevelIds::eStockYardsReturn)
        {
        }
    }
    else
    {
        relive_new AO::UXB(pUxbTlv, tlvId, resMan, map);
    }
}

void Factory::ParamiteAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnims(AO::Paramite::sParamiteMotionAnimIds);
        resMan.PendAnimation(AnimId::ParamiteWeb);
        return;
    }
    relive_new AO::Paramite(static_cast<Path_Paramite*>(pTlv), tlvId, resMan, map);
}

void Factory::BatAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::Bat_Flying);
        resMan.PendAnimation(AnimId::Bat);
        resMan.PendAnimation(AnimId::Bat_Unknown);
    }
    else
    {
        relive_new AO::Bat(static_cast<Path_Bat*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::RingMudAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnims(AO::Mudokon::sMudMotionAnimIds);
    }
    else
    {
        relive_new AO::Mudokon(pTlv, tlvId, resMan, map);
    }
}

void Factory::BirdPortalAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    auto pBirdPortalTlv = static_cast<Path_BirdPortal*>(pTlv);
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::BirdPortal_TerminatorShrink);
        resMan.PendAnimation(AnimId::BirdPortal_TerminatorIdle);
        resMan.PendAnimation(AnimId::BirdPortal_TerminatorGrow);
        resMan.PendAnimation(AnimId::BirdPortal_TerminatorShrink);
        resMan.PendAnimation(AnimId::BirdPortal_Sparks);
        resMan.PendAnimation(AnimId::BirdPortal_Flash);

        if (pBirdPortalTlv->mPortalType == Path_BirdPortal::PortalType::eShrykull)
        {
            resMan.PendAnimation(AnimId::ShrykullStart);
            resMan.PendAnimation(AnimId::ShrykullTransform);
            resMan.PendAnimation(AnimId::ShrykullDetransform);
            resMan.PendAnimation(AnimId::Zap_Line_Blue);
            resMan.PendAnimation(AnimId::Zap_Line_Red);
        }
    }
    else
    {
        relive_new AO::BirdPortal(pBirdPortalTlv, tlvId, resMan, map);
    }
}

void Factory::BellSongStoneAO(Path_TLV* /*pTlv*/, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper&, BaseMap& map)
{
    if (loadMode != LoadMode::LoadResourceFromList_1 && loadMode != LoadMode::LoadResource_2)
    {
        map.TLV_Reset(tlvId);
    }
}


void Factory::TrapDoorAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        switch (map.mCurrentLevel)
        {
            case EReliveLevelIds::eRuptureFarms:
            case EReliveLevelIds::eBoardRoom:
            case EReliveLevelIds::eRuptureFarmsReturn:
                resMan.PendAnimation(AnimId::R1_TrapDoor_Open);
                resMan.PendAnimation(AnimId::R1_TrapDoor_Closed);
                resMan.PendAnimation(AnimId::R1_TrapDoor_Opening);
                resMan.PendAnimation(AnimId::R1_TrapDoor_Closing);
                break;
            case EReliveLevelIds::eLines:
            case EReliveLevelIds::eStockYards:
                resMan.PendAnimation(AnimId::Lines_TrapDoor_Open);
                resMan.PendAnimation(AnimId::Lines_TrapDoor_Closed);
                resMan.PendAnimation(AnimId::Lines_TrapDoor_Opening);
                resMan.PendAnimation(AnimId::Lines_TrapDoor_Closing);
                break;
            case EReliveLevelIds::eDesert:
            case EReliveLevelIds::eDesertTemple:
            case EReliveLevelIds::eDesertEscape:
                resMan.PendAnimation(AnimId::Desert_TrapDoor_Open);
                resMan.PendAnimation(AnimId::Desert_TrapDoor_Closed);
                resMan.PendAnimation(AnimId::Desert_TrapDoor_Opening);
                resMan.PendAnimation(AnimId::Desert_TrapDoor_Closing);
                break;
            default:
                resMan.PendAnimation(AnimId::Lines_TrapDoor_Open);
                resMan.PendAnimation(AnimId::Lines_TrapDoor_Closed);
                resMan.PendAnimation(AnimId::Lines_TrapDoor_Opening);
                resMan.PendAnimation(AnimId::Lines_TrapDoor_Closing);
                break;
        }
    }
    else
    {
        relive_new TrapDoor(static_cast<Path_TrapDoor*>(pTlv), tlvId, resMan, map);
    }
}


void Factory::RollingBallAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::Stone_Ball);
        resMan.PendAnimation(AnimId::Stone_Ball_Rolling);
        resMan.PendAnimation(AnimId::Explosion_Rock);
    }
    else
    {
        relive_new AO::RollingBall(static_cast<Path_RollingBall*>(pTlv), tlvId, resMan, map);
    }
}

static TlvIterator FindMatchingSligTLVAO(TlvIterator pTlvIter, Path_SligBound* pTlv)
{
    while (pTlvIter.GetTlv())
    {
        if (pTlvIter.GetTlv()->mTlvType == ReliveTypes::eSlig && pTlv->mSligBoundId == pTlvIter.GetTlv<Path_Slig>()->mSligBoundId && !pTlvIter.GetTlv()->mTlvFlags.Get(TlvFlags::eBit2_Destroyed))
        {
            return pTlvIter;
        }
        pTlvIter = pTlvIter.Next_TLV();
    }
    return TlvIterator::Invalid();
}

void Factory::SligBoundLeftAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    auto pBound = static_cast<Path_SligBound*>(pTlv);
    LoadWalkingSligResourcesAO(loadMode, pBound->mDisabledResourcesAO, resMan, map);

    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        // Do nothing, handled in LoadSligResources
    }
    else
    {
        pBound->mTlvFlags.Clear(TlvFlags::eBit1_Created);
        pBound->mTlvFlags.Clear(TlvFlags::eBit2_Destroyed);

        for (s16 camX_idx = -2; camX_idx < 3; camX_idx++)
        {
            TlvIterator pTlvIter = map.Get_First_TLV_For_Offsetted_Camera(camX_idx, 0);
            Path_TLV* pSligTlv = FindMatchingSligTLVAO(pTlvIter, pBound).GetTlv();
            if (pSligTlv)
            {
                pSligTlv->mTlvFlags.Set(TlvFlags::eBit1_Created);
                pSligTlv->mTlvFlags.Set(TlvFlags::eBit2_Destroyed);
                // AO OG bug tlvId not recalculated??
                relive_new AO::Slig(static_cast<Path_Slig*>(pSligTlv), tlvId, resMan, map);
                return;
            }
        }
    }
}

void Factory::RollingBallStopperAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::Stone_Ball_Stopper);
    }
    else
    {
        relive_new RollingBallStopper(static_cast<Path_RollingBallStopper*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::FootSwitchAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::Foot_Switch_Temple);
        resMan.PendAnimation(AnimId::Foot_Switch_Temple_Pressed);
    }
    else
    {
        relive_new AO::FootSwitch(static_cast<Path_FootSwitch*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::SecurityClawAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    auto pSecurityClawTlv = static_cast<Path_SecurityClaw*>(pTlv);
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::Security_Claw_Lower_Open);
        resMan.PendAnimation(AnimId::Security_Claw_Lower_Close);
        resMan.PendAnimation(AnimId::Security_Claw_Lower_Idle);
        resMan.PendAnimation(AnimId::Security_Claw_Upper_Rotating);
        resMan.PendAnimation(AnimId::Security_Claw_Upper_NoRotation);
        resMan.PendAnimation(AnimId::Zap_Line_Blue);
        resMan.PendAnimation(AnimId::Zap_Line_Red);
        resMan.PendAnims(kAbeGibs);
        resMan.PendAnims(kSlogGibs);
        resMan.PendAnims(kElumGibs);
        resMan.PendAnimation(AnimId::Metal_Gib);
        resMan.PendAnimation(AnimId::AirExplosion);
    }
    else
    {
        relive_new AO::SecurityClaw(pSecurityClawTlv, tlvId, resMan, map);
    }
}


void Factory::MotionDetectorAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode != LoadMode::LoadResourceFromList_1 && loadMode != LoadMode::LoadResource_2)
    {
        relive_new AO::MotionDetector(static_cast<Path_MotionDetector*>(pTlv), tlvId, resMan, map);
    }
    else
    {
        resMan.PendAnimation(AnimId::MotionDetector_Flare);
        resMan.PendAnimation(AnimId::MotionDetector_Laser);
    }
}

void Factory::SligSpawnerAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    auto pSligTlv = static_cast<Path_Slig*>(pTlv);
    LoadWalkingSligResourcesAO(loadMode, pSligTlv->mDisabledResourcesAO, resMan, map);

    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        // Do nothing, handled in LoadSligResources
    }
    else
    {
        relive_new AO::SligSpawner(pTlv, pSligTlv, tlvId, resMan, map);
    }
}

void Factory::ElectricWallAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode != LoadMode::LoadResourceFromList_1 && loadMode != LoadMode::LoadResource_2)
    {
        relive_new ElectricWall(static_cast<Path_ElectricWall*>(pTlv), tlvId, resMan, map);
    }
    else
    {
        resMan.PendAnimation(AnimId::Electric_Wall);
    }
}


void Factory::LiftMoverAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode != LoadMode::LoadResourceFromList_1 && loadMode != LoadMode::LoadResource_2)
    {
        relive_new AO::LiftMover(static_cast<Path_LiftMover*>(pTlv), tlvId, resMan, map);
    }
}


void Factory::ChimeLockAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode != LoadMode::LoadResourceFromList_1 && loadMode != LoadMode::LoadResource_2)
    {
        relive_new AO::ChimeLock(static_cast<Path_ChimeLock*>(pTlv), tlvId, resMan, map);
    }
    else
    {
        resMan.PendAnimation(AnimId::Chime_Ball);
        resMan.PendAnimation(AnimId::BigChime);
        resMan.PendAnimation(AnimId::MediumChime);
        resMan.PendAnimation(AnimId::SmallChime);
        resMan.PendAnimation(AnimId::BigChime_Moving);
        resMan.PendAnimation(AnimId::MediumChime_Moving);
        resMan.PendAnimation(AnimId::SmallChime_Moving);
    }
}

void Factory::ElumAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnims(AO::Elum::gElumMotionAnimIds);
    }
    else
    {
        AO::Elum::Spawn(tlvId, resMan, map);
        AO::gElum->mXPos = FP_FromInteger(pTlv->mTopLeftX);
        AO::gElum->mYPos = FP_FromInteger(pTlv->mTopLeftY);
    }
}


void Factory::MeatSackAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::Meat);
        resMan.PendAnimation(AnimId::MeatSack_Idle);
        resMan.PendAnimation(AnimId::MeatSack_Hit);
    }
    else
    {
        relive_new AO::MeatSack(static_cast<Path_MeatSack*>(pTlv), tlvId, resMan, map);
    }
}


void Factory::ScrabAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnims(AO::Scrab::sScrabMotionAnimIds);
        return;
    }
    relive_new AO::Scrab(static_cast<Path_Scrab*>(pTlv), tlvId, resMan, map);
}


void Factory::FlintLockFireAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::Fire);
        resMan.PendAnimation(AnimId::FlintLock_Gourd);
        resMan.PendAnimation(AnimId::FlintLock_Hammers_Disabled);
        resMan.PendAnimation(AnimId::FlintLock_Hammers_Activating);
    }
    else
    {
        relive_new AO::FlintLockFire(static_cast<Path_FlintLockFire*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::SligBoundRightAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    SligBoundLeftAO(pTlv, tlvId, loadMode, resMan, map);
}

void Factory::InvisibleSwitchAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode != LoadMode::LoadResourceFromList_1 && loadMode != LoadMode::LoadResource_2)
    {
        relive_new InvisibleSwitch(static_cast<Path_InvisibleSwitch*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::WorkerMudokonAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    auto pMudTlv = static_cast<Path_Mudokon*>(pTlv);
    if (loadMode != LoadMode::LoadResourceFromList_1 && loadMode != LoadMode::LoadResource_2)
    {
        switch (pMudTlv->mJob)
        {
            case Path_Mudokon::MudJobs::eStandScrub:
                break;

            case Path_Mudokon::MudJobs::eSitChant:
                break;
        }

        relive_new AO::Mudokon(pTlv, tlvId, resMan, map);
    }
    else
    {
        resMan.PendAnims(AO::Mudokon::sMudMotionAnimIds);
        switch (pMudTlv->mJob)
        {
            case Path_Mudokon::MudJobs::eStandScrub:
            {
            }
            break;

            case Path_Mudokon::MudJobs::eSitScrub:
            {
            }
            break;

            case Path_Mudokon::MudJobs::eSitChant:
            {
            }
            break;
        }
    }
}

void Factory::DoorFlameAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::Fire);
        resMan.PendAnimation(AnimId::Door_FireBackgroundGlow);
    }
    else
    {
        relive_new AO::DoorFlame(static_cast<Path_DoorFlame*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::MovingBombAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    auto pMovingBombTlv = static_cast<Path_MovingBomb*>(pTlv);

    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::MovingBomb);
        resMan.PendAnimation(AnimId::AirExplosion);
        resMan.PendAnimation(AnimId::Metal_Gib);
        resMan.PendAnims(kAbeGibs);
        resMan.PendAnims(kElumGibs);

        if (map.mCurrentLevel == EReliveLevelIds::eStockYards || map.mCurrentLevel == EReliveLevelIds::eStockYardsReturn)
        {
        }
    }
    else
    {
        relive_new AO::MovingBomb(pMovingBombTlv, tlvId, resMan, map);
    }
}

void Factory::MeatSawAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::MeatSawMotor);
        resMan.PendAnimation(AnimId::MeatSaw_Idle);
        resMan.PendAnimation(AnimId::MeatSaw_Moving);
        resMan.PendAnims(kAbeGibs);
    }
    else
    {
        relive_new AO::MeatSaw(static_cast<Path_MeatSaw*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::MainMenuControllerAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (AO::gMainMenuInstanceCount == 0)
    {
        if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
        {
            resMan.PendAnims(AO::Menu::kAnims);
        }
        else
        {
            relive_new AO::Menu(pTlv, tlvId, resMan, map);
        }
    }
}

void Factory::HintFlyAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode != LoadMode::LoadResourceFromList_1 && loadMode != LoadMode::LoadResource_2)
    {
        relive_new AO::HintFly(static_cast<Path_HintFly*>(pTlv), tlvId, resMan, map);
    }
    else
    {
        resMan.PendAnimation(AnimId::HintFly);
    }
}

void Factory::TimerTriggerAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode != LoadMode::LoadResourceFromList_1 && loadMode != LoadMode::LoadResource_2)
    {
        relive_new TimerTrigger(static_cast<Path_TimerTrigger*>(pTlv), tlvId, resMan, map);
    }
}


void Factory::SecurityDoorAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::Security_Door_Idle);
        resMan.PendAnimation(AnimId::Security_Door_Speak);
    }
    else
    {
        relive_new AO::SecurityDoor(static_cast<Path_SecurityDoor*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::BoomMachineAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    auto pBoomMachineTlv = static_cast<Path_BoomMachine*>(pTlv);
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::Grenade);
        resMan.PendAnimation(AnimId::AirExplosion);
        resMan.PendAnims(kAbeGibs);
        resMan.PendAnimation(AnimId::Metal_Gib);
        resMan.PendAnims(kSlogGibs);
        resMan.PendAnimation(AnimId::BoomMachine_Pipe_DropGrenade);
        resMan.PendAnimation(AnimId::BoomMachine_Pipe_Idle);
        resMan.PendAnimation(AnimId::BoomMachine_Button_Off);
        resMan.PendAnimation(AnimId::BoomMachine_Button_On);
        return;
    }

    relive_new BoomMachine(pBoomMachineTlv, tlvId, resMan, map);
}


void Factory::LCDScreenAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
    }
    else
    {
        relive_new AO::LCDScreen(static_cast<Path_LCDScreen*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::HandStoneAO(Path_TLV* /*pTlv*/, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::Circular_Fade);
    }
    else
    {
        map.TLV_Reset(tlvId);
    }
}


void Factory::CreditsControllerAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode != LoadMode::LoadResourceFromList_1 && loadMode != LoadMode::LoadResource_2)
    {
        if (!AO::gCreditsControllerExists)
        {
            relive_new AO::CreditsController(static_cast<Path_CreditsController*>(pTlv), tlvId, resMan, map);
        }
    }
}

void Factory::LCDStatusBoardAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
    }
    else
    {
        relive_new AO::LCDStatusBoard(static_cast<Path_LCDStatusBoard*>(pTlv), tlvId, resMan, map);
    }
}


void Factory::SwitchStateBooleanLogicAO(Path_TLV* /*pTlv*/, const Guid& /*tlvId*/, LoadMode loadMode, ResourceManagerWrapper&, BaseMap&)
{
    if (loadMode != LoadMode::LoadResourceFromList_1 && loadMode != LoadMode::LoadResource_2)
    {
        LOG_WARNING("Factory_SwitchStateBooleanLogic_487B80 tlv converter not implemented");
        //relive_new SwitchStateBooleanLogic(static_cast<Path_SwitchStateBooleanLogic*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::MusicTriggerAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode != LoadMode::LoadResourceFromList_1 && loadMode != LoadMode::LoadResource_2)
    {
        relive_new AO::MusicTrigger(static_cast<Path_MusicTrigger*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::LightEffectAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    auto pPathLightTlv = static_cast<Path_LightEffect*>(pTlv);
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        switch (pPathLightTlv->mType)
        {
            case Path_LightEffect::Type::Star:
                resMan.PendAnimation(AnimId::Star);
                break;

            case Path_LightEffect::Type::GoldGlow:
                resMan.PendAnimation(AnimId::GoldGlow);
                break;

            case Path_LightEffect::Type::GreenGlow:
                resMan.PendAnimation(AnimId::GreenGlow);
                break;

            case Path_LightEffect::Type::FlintGlow:
                resMan.PendAnimation(AnimId::FlintGlow);
                break;

            case Path_LightEffect::Type::Switchable_RedGreenDoorLights:
                resMan.PendAnimation(AnimId::RedDoorLight);
                resMan.PendAnimation(AnimId::GreenDoorLight);
                break;

            case Path_LightEffect::Type::Switchable_RedGreenHubLight:
                resMan.PendAnimation(AnimId::RedHubLight);
                resMan.PendAnimation(AnimId::GreenHubLight);
                break;

            default:
                return;
        }
    }
    else
    {
        switch (pPathLightTlv->mType)
        {
            case Path_LightEffect::Type::Star:
            {
                relive_new AO::LightEffect(pPathLightTlv, tlvId, resMan, map);
                break;
            }

            case Path_LightEffect::Type::GoldGlow:
            {
                relive_new AO::DoorLight(pPathLightTlv, tlvId, resMan, map);
                break;
            }

            case Path_LightEffect::Type::GreenGlow:
            {
                relive_new AO::DoorLight(pPathLightTlv, tlvId, resMan, map);
                break;
            }

            case Path_LightEffect::Type::FlintGlow:
            {
                relive_new AO::DoorLight(pPathLightTlv, tlvId, resMan, map);
                break;
            }

            case Path_LightEffect::Type::Switchable_RedGreenDoorLights:
            case Path_LightEffect::Type::Switchable_RedGreenHubLight:
            {
                relive_new AO::DoorLight(pPathLightTlv, tlvId, resMan, map);
                break;
            }

            default:
                return;
        }
    }
}


void Factory::SlogSpawnerAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnims(AO::Slog::sSlogMotionAnimIds);
    }
    else
    {
        relive_new AO::SlogSpawner(static_cast<Path_SlogSpawner*>(pTlv), tlvId, resMan, map);
    }
}


void Factory::GasCountDownAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
    }
    else
    {
        relive_new GasCountDown(static_cast<Path_GasCountDown*>(pTlv), tlvId, resMan, map);
    }
}



void Factory::RingCancelAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper&, BaseMap& map)
{
    if (loadMode != LoadMode::LoadResourceFromList_1 && loadMode != LoadMode::LoadResource_2)
    {
        auto pRingCancel = static_cast<Path_RingCancel*>(pTlv);
        if (pRingCancel->mRemovesShrykull)
        {
            if (AO::gAbe->mRingPulseTimer)
            {
                if (AO::gAbe->mHaveShrykull)
                {
                    AO::gAbe->mRingPulseTimer = 0;
                }
            }
        }
        else
        {
            if (!AO::gAbe->mHaveShrykull)
            {
                AO::gAbe->mRingPulseTimer = 0;
            }
        }
        map.TLV_Reset(tlvId);
    }
}


void Factory::GasEmitterAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode != LoadMode::LoadResourceFromList_1 && loadMode != LoadMode::LoadResource_2)
    {
        relive_new GasEmitter(static_cast<Path_GasEmitter*>(pTlv), tlvId, resMan, map);
    }
}


void Factory::ZzzSpawnerAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode != LoadMode::LoadResourceFromList_1 && loadMode != LoadMode::LoadResource_2)
    {
        relive_new ZzzSpawner(static_cast<Path_ZzzSpawner*>(pTlv), tlvId, resMan, map);
    }
}


void Factory::BackgroundGlukkonAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::Background_Glukkon_Idle);
        resMan.PendAnimation(AnimId::Background_Glukkon_Laugh);
        resMan.PendAnimation(AnimId::Background_Glukkon_KillHim1);
        resMan.PendAnimation(AnimId::Background_Glukkon_KillHim2);
        resMan.PendAnimation(AnimId::Background_Glukkon_Dying);
        resMan.PendAnims(kSlogGibs);
        resMan.PendAnimation(AnimId::AirExplosion);
    }
    else
    {
        relive_new AO::BackgroundGlukkon(static_cast<Path_BackgroundGlukkon*>(pTlv), tlvId, resMan, map);
    }
}


void Factory::KillUnsavedMudsAO(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper&, BaseMap& map)
{
    if (loadMode != LoadMode::LoadResourceFromList_1 && loadMode != LoadMode::LoadResource_2)
    {
        // TODO: Check removed OG bug fix - added an extra check that checks if the map has changed
        // which prevents that the killed mudokon count becomes inaccurate or even negative.
        auto pTlvKillMuds = static_cast<Path_KillUnsavedMuds*>(pTlv);
        if (!pTlvKillMuds->mTlvSpecificMeaning)
        {
            AO::gKilledMudokons = static_cast<s16>(pTlvKillMuds->mMudsToKillCount - AO::gRescuedMudokons);
            pTlvKillMuds->mTlvSpecificMeaning = 1;
            map.TLV_Delete(tlvId);
        }
    }
}

// NAMESPACE AO END

void Factory::MainMenuControllerAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (sMainMenuObjectCounter_BB4400 == 0)
    {
        if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
        {
            for (const auto& info : MainMenuController::sMainMenuFrameTable_561CC8)
            {
                if (info.field_0_animation != AnimId::None)
                {
                    resMan.PendAnimation(info.field_0_animation);
                }
            }
            resMan.PendAnims(MainMenuController::kMenuAnims);

            // Originally loaded in the PauseMenu::Init()
            resMan.PendAnimation(AnimId::NormalMudIcon);
            resMan.PendAnimation(AnimId::AngryMudIcon);
            resMan.PendAnimation(AnimId::HappyMudIcon);

            // Originally loaded in Game_Init_LoadingIcon()
            resMan.PendAnimation(AnimId::Loading_Icon2);
        }
        else
        {
            relive_new MainMenuController(pTlv, tlvId, resMan, map);
        }
    }
}

void Factory::HoistAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    Path_Hoist* pHoistTlv = static_cast<Path_Hoist*>(pTlv);
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::HoistRock1);
        resMan.PendAnimation(AnimId::HoistRock2);
        resMan.PendAnimation(AnimId::HoistRock3);
    }
    else if (pHoistTlv->mHoistType == Path_Hoist::Type::eOffScreen)
    {
        // Its an off screen hoist so create the falling rocks effect
        relive_new HoistRocksEffect(pHoistTlv, tlvId, resMan, map);
    }
    else
    {
        map.TLV_Reset(tlvId);
    }
}

void Factory::EdgeAE(Path_TLV* /*pTlv*/, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper&, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        // Empty
    }
    else
    {
        map.TLV_Reset(tlvId);
    }
}

void Factory::DoorAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        auto pDoorTlv = static_cast<Path_Door*>(pTlv);
        resMan.PendAnimation(AnimId::Door_Themed_Closed, pDoorTlv->mTheme);
        resMan.PendAnimation(AnimId::Door_Themed_Open, pDoorTlv->mTheme);
    }
    else
    {
        relive_new Door(static_cast<Path_Door*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::ShadowZoneAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode != LoadMode::LoadResourceFromList_1 && loadMode != LoadMode::LoadResource_2)
    {
        relive_new ShadowZone(static_cast<Path_ShadowZone*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::LiftPointAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::AE_Rope);
        switch (map.mCurrentLevel)
        {
            case EReliveLevelIds::eNecrum:
            case EReliveLevelIds::eMudomoVault:
            case EReliveLevelIds::eMudomoVault_Ender:
            case EReliveLevelIds::eMudancheeVault:
            case EReliveLevelIds::eMudancheeVault_Ender:
                resMan.PendAnimation(AnimId::LiftPlatform_Necrum);
                resMan.PendAnimation(AnimId::LiftBottomWheel_Necrum);
                resMan.PendAnimation(AnimId::LiftTopWheel_Necrum);
                break;

            case EReliveLevelIds::eFeeCoDepot:
            case EReliveLevelIds::eFeeCoDepot_Ender:
            case EReliveLevelIds::eBarracks:
            case EReliveLevelIds::eBarracks_Ender:
            case EReliveLevelIds::eBonewerkz:
            case EReliveLevelIds::eBonewerkz_Ender:
            case EReliveLevelIds::eBrewery:
            case EReliveLevelIds::eBrewery_Ender:
                resMan.PendAnimation(AnimId::LiftPlatform_Mines);
                resMan.PendAnimation(AnimId::LiftBottomWheel_Mines);
                resMan.PendAnimation(AnimId::LiftTopWheel_Mines);
                break;

            default:
                resMan.PendAnimation(AnimId::LiftPlatform_Mines);
                resMan.PendAnimation(AnimId::LiftBottomWheel_Mines);
                resMan.PendAnimation(AnimId::LiftTopWheel_Mines);
                break;
        }
    }
    else
    {
        Path_LiftPoint* pLiftTlv = static_cast<Path_LiftPoint*>(pTlv);
        for (s32 i = 0; i < gBaseGameObjects->Size(); i++)
        {
            BaseGameObject* pObj = gBaseGameObjects->ItemAt(i);
            if (!pObj)
            {
                break;
            }

            if (!(pObj->GetDead()) && pObj->Type() == ReliveTypes::eLiftPoint)
            {
                // Is there already an existing LiftPoint object for this TLV?
                LiftPoint* pLiftPoint = static_cast<LiftPoint*>(pObj);
                const s16 xpos = FP_GetExponent(pLiftPoint->mXPos);
                if (pTlv->mTopLeftX <= xpos && xpos <= pTlv->mBottomRightX && pLiftPoint->mLiftPointId == pLiftTlv->mLiftPointId && pLiftPoint->mCurrentLevel == map.mCurrentLevel && pLiftPoint->mCurrentPath == map.mCurrentPath)
                {
                    // Yes so just reset its data
                    map.TLV_Reset(tlvId);
                    return;
                }
            }
        }

        // TODO: Meaning of the data in mTlvSpecificMeaning for lift point
        if (pLiftTlv->mTlvSpecificMeaning & 2 || (pLiftTlv->mTlvSpecificMeaning == 0 && pLiftTlv->mIsStartPoint))
        {
            relive_new LiftPoint(pLiftTlv, tlvId, resMan, map);
            return;
        }
        else
        {
            // Find out where to create the lift point
            s16 pointNumber = 1;
            while (pointNumber < 8)
            {
                TlvIterator tlvIterator = map.Get_First_TLV_For_Offsetted_Camera(
                    0,
                    pointNumber / 2 * (pointNumber % 2 != 0 ? -1 : 1));

                while (tlvIterator.GetTlv())
                {
                    if (tlvIterator.GetTlv()->mTlvType == ReliveTypes::eLiftPoint)
                    {
                        auto pLiftPointIter = tlvIterator.GetTlv<Path_LiftPoint>();

                        const s32 tlvX = pTlv->mTopLeftX;
                        const s32 absX = tlvIterator.GetTlv()->mTopLeftX - tlvX >= 0 ? tlvIterator.GetTlv()->mTopLeftX - tlvX : tlvX - tlvIterator.GetTlv()->mTopLeftX;

                        if (absX < 5 && pLiftPointIter->mLiftPointId == pLiftTlv->mLiftPointId && (pLiftPointIter->mTlvSpecificMeaning & 2 || pLiftPointIter->mTlvSpecificMeaning == 0) && pLiftPointIter->mIsStartPoint)
                        {
                            relive_new LiftPoint(pLiftPointIter, tlvId, resMan, map);
                            return;
                        }
                    }

                    tlvIterator = tlvIterator.Next_TLV();
                }
                pointNumber++;
            }

            // Default to original
            relive_new LiftPoint(pLiftTlv, tlvId, resMan, map);
        }
    }
}

void Factory::WellAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::Well_Leaf);
    }
    else
    {
        Path_WellBase* pWellTlv = static_cast<Path_WellBase*>(pTlv);
        const FP xpos = FP_FromInteger(pWellTlv->mTopLeftX);
        const FP ypos = FP_FromInteger(pWellTlv->mTopLeftY + 5);
        relive_new Well(pWellTlv, xpos, ypos, tlvId, resMan, map);
    }
}

void Factory::DoveAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode != LoadMode::LoadResourceFromList_1 && loadMode != LoadMode::LoadResource_2)
    {
        auto pDoveTlv = static_cast<Path_Dove*>(pTlv);

        const s16 width = pDoveTlv->Width();
        const s16 height = pDoveTlv->Height();

        for (s32 i = 0; i < pDoveTlv->mDoveCount; i++)
        {
            auto pDove = relive_new Dove(
                AnimId::Dove_Idle,
                tlvId,
                pDoveTlv->mScale != reliveScale::eFull ? FP_FromDouble(0.5) : FP_FromInteger(1),
                resMan, map);

            s16 ypos = 0;
            if (pDoveTlv->mPixelPerfect)
            {
                pDove->mXPos = FP_FromInteger(pTlv->mTopLeftX);
                ypos = pTlv->mTopLeftY;
            }
            else
            {
                pDove->mXPos = FP_FromInteger(pTlv->mTopLeftX + width * Math_NextRandom() / 256);
                ypos = pTlv->mTopLeftY + height * Math_NextRandom() / 256;
            }
            pDove->mYPos = FP_FromInteger(ypos) + FP_FromInteger(10);
        }
    }
}

void Factory::RockSackAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::Rock);
        resMan.PendAnimation(AnimId::RockSack_Idle);
        resMan.PendAnimation(AnimId::RockSack_SoftHit);
        resMan.PendAnimation(AnimId::RockSack_HardHit);
    }
    else
    {
        relive_new RockSack(static_cast<Path_RockSack*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::FallingItemAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::Explosion_Stick);
        resMan.PendAnimation(AnimId::ObjectShadow);
        resMan.PendAnimation(AnimId::Explosion_Rock);
        if (map.mCurrentLevel == EReliveLevelIds::eBonewerkz)
        {
            resMan.PendAnimation(AnimId::FallingCrate_Falling);
            resMan.PendAnimation(AnimId::FallingCrate_Waiting);
            resMan.PendAnimation(AnimId::AirExplosion);
        }
        else
        {
            resMan.PendAnimation(AnimId::AE_FallingRock_Falling);
            resMan.PendAnimation(AnimId::AE_FallingRock_Waiting);
        }
    }
    else
    {
        relive_new FallingItem(static_cast<Path_FallingItem*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::PullRingRopeAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::AE_Rope);
        resMan.PendAnimation(AnimId::PullRingRope_Idle);
        resMan.PendAnimation(AnimId::PullRingRope_UseBegin);
        resMan.PendAnimation(AnimId::PullRingRope_UseEnd);
    }
    else
    {
        relive_new PullRingRope(static_cast<Path_PullRingRope*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::TimedMineAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    auto mine_tlv = static_cast<Path_TimedMine*>(pTlv);

    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnims(kAbeGibs);
        resMan.PendAnims(kSlogGibs);
        resMan.PendAnimation(AnimId::TimedMine_Activated);
        resMan.PendAnimation(AnimId::TimedMine_Idle);
        resMan.PendAnimation(AnimId::Bomb_Flash);
        resMan.PendAnimation(AnimId::Bomb_RedGreenTick);
        resMan.PendAnimation(AnimId::Explosion_Rock);
        resMan.PendAnimation(AnimId::GroundExplosion);
    }
    else
    {
        relive_new TimedMine(mine_tlv, tlvId, resMan, map);
    }
}

static void LoadWalkingSligResourcesAE(ResourceManagerWrapper& resMan)
{
    resMan.PendAnims(Slig::sSligAnimIdTable);
    resMan.PendAnimation(AnimId::ShootingZFire_Particle);
    resMan.PendAnimation(AnimId::ShootingFire_Particle);
    resMan.PendAnimation(AnimId::Bullet_Shell);
    resMan.PendAnims(kSligGibs);
    resMan.PendAnimation(AnimId::ObjectShadow);
}

void Factory::SligAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    auto pSligTlv = static_cast<Path_Slig*>(pTlv);
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        LoadWalkingSligResourcesAE(resMan);
    }
    else
    {
        relive_new Slig(pSligTlv, tlvId, resMan, map);
    }
}

static void LoadSlogResourcesAE(ResourceManagerWrapper& resMan)
{
    resMan.PendAnims(Slog::sSlogAnimIdTable);
}

void Factory::SlogAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        LoadSlogResourcesAE(resMan);
    }
    else
    {
        relive_new Slog(static_cast<Path_Slog*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::LeverAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::Lever_Pull_Release_Left);
        resMan.PendAnimation(AnimId::Lever_Pull_Release_Right);
        resMan.PendAnimation(AnimId::Lever_Idle);
        resMan.PendAnimation(AnimId::Lever_Pull_Left);
        resMan.PendAnimation(AnimId::Lever_Pull_Right);
    }
    else
    {
        relive_new Lever(static_cast<Path_Lever*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::SecurityOrbAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    Path_SecurityOrb* pSecurityOrbTlv = static_cast<Path_SecurityOrb*>(pTlv);
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::Security_Orb);
        resMan.PendAnimation(AnimId::Zap_Line_Blue);
        resMan.PendAnimation(AnimId::Zap_Line_Red);
        resMan.PendAnimation(AnimId::AE_ZapSpark);
        resMan.PendAnims(kAbeGibs);
        resMan.PendAnims(kSlogGibs);
        resMan.PendAnimation(AnimId::Metal_Gib);
        resMan.PendAnimation(AnimId::AirExplosion);
    }
    else
    {
        relive_new SecurityOrb(pSecurityOrbTlv, tlvId, resMan, map);
    }
}

void Factory::AbeStartAE(Path_TLV* pTlv,  const Guid& /*tlvId*/, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode != LoadMode::LoadResourceFromList_1 && loadMode != LoadMode::LoadResource_2 && !gAttract)
    {
        if (!gPauseMenu)
        {
            gPauseMenu = relive_new PauseMenu(resMan, map);
        }

        if (!gAbe)
        {
            gAbe = relive_new Abe(resMan, map);
            if (gAbe)
            {
                gAbe->mXPos = FP_FromInteger(pTlv->mTopLeftX + 12);
                gAbe->mYPos = FP_FromInteger(pTlv->mTopLeftY);
            }
        }
    }
}

void Factory::MineAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    auto mine_tlv = static_cast<Path_Mine*>(pTlv);

    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnims(kAbeGibs);
        resMan.PendAnims(kSlogGibs);
        resMan.PendAnimation(AnimId::Mine);
        resMan.PendAnimation(AnimId::Mine_Flash);
        resMan.PendAnimation(AnimId::Explosion_Rock);
        resMan.PendAnimation(AnimId::GroundExplosion);
    }
    else
    {
        relive_new Mine(mine_tlv, tlvId, resMan, map);
    }
}

void Factory::UXBAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    auto uxb_tlv = static_cast<Path_UXB*>(pTlv);
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnims(kAbeGibs);
        resMan.PendAnims(kSlogGibs);
        resMan.PendAnimation(AnimId::UXB_Active);
        resMan.PendAnimation(AnimId::UXB_Toggle);
        resMan.PendAnimation(AnimId::UXB_Disabled);
        resMan.PendAnimation(AnimId::Bomb_Flash);
        resMan.PendAnimation(AnimId::Bomb_RedGreenTick);
        resMan.PendAnimation(AnimId::Explosion_Rock);
        resMan.PendAnimation(AnimId::GroundExplosion);
    }
    else
    {
        relive_new UXB(uxb_tlv, tlvId, resMan, map);
    }
}

void Factory::ParamiteAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnims(Paramite::sParamiteMotionAnimIds);
        resMan.PendAnimation(AnimId::ParamiteWeb);
    }
    else
    {
         relive_new Paramite(static_cast<Path_Paramite*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::MovieHandStoneAE(Path_TLV*, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper&, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        // Empty
    }
    else
    {
        map.TLV_Reset(tlvId);
    }
}

void Factory::BirdPortalAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    auto pBirdPortalTlv = static_cast<Path_BirdPortal*>(pTlv);
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::BirdPortal_TerminatorShrink);
        resMan.PendAnimation(AnimId::BirdPortal_TerminatorIdle);
        resMan.PendAnimation(AnimId::BirdPortal_TerminatorGrow);
        resMan.PendAnimation(AnimId::BirdPortal_Sparks);
        resMan.PendAnimation(AnimId::BirdPortal_Flash);

        if (pBirdPortalTlv->mPortalType == Path_BirdPortal::PortalType::eShrykull)
        {
            resMan.PendAnimation(AnimId::ShrykullStart);
            resMan.PendAnimation(AnimId::ShrykullTransform);
            resMan.PendAnimation(AnimId::ShrykullDetransform);
            resMan.PendAnimation(AnimId::Zap_Line_Blue);
            resMan.PendAnimation(AnimId::Zap_Line_Red);
        }
    }
    else if (SwitchStates_Get(pBirdPortalTlv->mCreatePortalSwitchId))
    {
        relive_new BirdPortal(pBirdPortalTlv, tlvId, resMan, map);
    }
    else
    {
        map.TLV_Reset(tlvId);
    }
}

void Factory::TrapDoorAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        switch (map.mCurrentLevel)
        {
            case EReliveLevelIds::eMudomoVault:
            case EReliveLevelIds::eMudancheeVault:
            case EReliveLevelIds::eMudancheeVault_Ender:
            case EReliveLevelIds::eMudomoVault_Ender:
                resMan.PendAnimation(AnimId::Trap_Door_Tribal_Closed);
                resMan.PendAnimation(AnimId::Trap_Door_Tribal_Closing);
                resMan.PendAnimation(AnimId::Trap_Door_Tribal_Open);
                resMan.PendAnimation(AnimId::Trap_Door_Tribal_Opening);
                break;

            default:
                resMan.PendAnimation(AnimId::Trap_Door_Open);
                resMan.PendAnimation(AnimId::Trap_Door_Closed);
                resMan.PendAnimation(AnimId::Trap_Door_Closing);
                resMan.PendAnimation(AnimId::Trap_Door_Opening);
                break;
        }
    }
    else
    {
        relive_new TrapDoor(static_cast<Path_TrapDoor*>(pTlv), tlvId, resMan, map);
    }
}

static TlvIterator FindMatchingSligTLVAE(TlvIterator pTlvIter, Path_SligBound* pTlv)
{
    while (pTlvIter.GetTlv())
    {
        if (pTlvIter.GetTlv()->mTlvType == ReliveTypes::eSlig && pTlv->mSligBoundId == pTlvIter.GetTlv<Path_Slig>()->mSligBoundId && !pTlvIter.GetTlv()->mTlvFlags.Get(TlvFlags::eBit2_Destroyed))
        {
            return pTlvIter;
        }
        pTlvIter = pTlvIter.Next_TLV();
    }
    return TlvIterator::Invalid();
}

void Factory::SligBoundLeftAE(Path_TLV* pTlv,  const Guid& /*tlvId*/, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    auto pBound = static_cast<Path_SligBound*>(pTlv);
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        LoadWalkingSligResourcesAE(resMan);
    }
    else
    {
        pBound->mTlvFlags.Clear(TlvFlags::eBit1_Created);
        pBound->mTlvFlags.Clear(TlvFlags::eBit2_Destroyed);

        for (s16 camX_idx = -2; camX_idx < 3; camX_idx++)
        {
            TlvIterator pTlvIter = FindMatchingSligTLVAE( map.Get_First_TLV_For_Offsetted_Camera(camX_idx, 0), pBound);
            if (pTlvIter.GetTlv())
            {
                pTlvIter.GetTlv()->mTlvFlags.Set(TlvFlags::eBit1_Created);
                pTlvIter.GetTlv()->mTlvFlags.Set(TlvFlags::eBit2_Destroyed);

                relive_new Slig(pTlvIter.GetTlv<Path_Slig>(), pTlvIter.GetTlv()->mId, resMan, map); // id of the slig to spawn at the bound, not the bound itself

                return;
            }
        }
    }
}

void Factory::FootSwitchAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        switch (map.mCurrentLevel)
        {
            case EReliveLevelIds::eMudomoVault:
            case EReliveLevelIds::eMudancheeVault:
            case EReliveLevelIds::eMudancheeVault_Ender:
            case EReliveLevelIds::eMudomoVault_Ender:
                resMan.PendAnimation(AnimId::Foot_Switch_Vault_Idle);
                resMan.PendAnimation(AnimId::Foot_Switch_Vault_Pressed);
                break;
            case EReliveLevelIds::eBonewerkz:
                resMan.PendAnimation(AnimId::Foot_Switch_Bonewerkz_Idle);
                resMan.PendAnimation(AnimId::Foot_Switch_Bonewerkz_Pressed);
                break;
            default:
                resMan.PendAnimation(AnimId::Foot_Switch_Industrial_Idle);
                resMan.PendAnimation(AnimId::Foot_Switch_Industrial_Pressed);
                break;
        }
    }
    else
    {
        relive_new FootSwitch(static_cast<Path_FootSwitch*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::ChantSuppressor_WithArmsAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    // TODO: Most of the code for the chant suppressor with arms is still in the game
    // check if it can be re-added at some point.
    SecurityOrbAE(pTlv, tlvId, loadMode, resMan, map);
}

void Factory::MotionDetectorAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode != LoadMode::LoadResourceFromList_1 && loadMode != LoadMode::LoadResource_2)
    {
        relive_new MotionDetector(static_cast<Path_MotionDetector*>(pTlv), tlvId, nullptr, resMan, map);
    }
    else
    {
        resMan.PendAnimation(AnimId::MotionDetector_Flare);
        resMan.PendAnimation(AnimId::MotionDetector_Laser);
    }
}

void Factory::SligSpawnerAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    auto pSligTlv = static_cast<Path_Slig*>(pTlv);
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        LoadWalkingSligResourcesAE(resMan);
    }
    else
    {
        relive_new SligSpawner(pSligTlv, tlvId, resMan, map);
    }
}

void Factory::ElectricWallAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::Electric_Wall);
    }
    else
    {
        relive_new ElectricWall(static_cast<Path_ElectricWall*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::LiftMoverAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode != LoadMode::LoadResourceFromList_1 && loadMode != LoadMode::LoadResource_2)
    {
        relive_new LiftMover(static_cast<Path_LiftMover*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::MeatSackAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::Meat);
        resMan.PendAnimation(AnimId::MeatSack_Idle);
        resMan.PendAnimation(AnimId::MeatSack_Hit);
    }
    else
    {
        relive_new MeatSack(static_cast<Path_MeatSack*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::ScrabAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnims(Scrab::sScrabMotionAnimIds);
    }
    else
    {
        relive_new Scrab(static_cast<Path_Scrab*>(pTlv), tlvId, Path_ScrabSpawner::SpawnDirection::eNone, resMan, map);
    }
}

void Factory::SligBoundRightAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    SligBoundLeftAE(pTlv, tlvId, loadMode, resMan, map);
}

void Factory::InvisibleSwitchAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode != LoadMode::LoadResourceFromList_1 && loadMode != LoadMode::LoadResource_2)
    {
        relive_new InvisibleSwitch(static_cast<Path_InvisibleSwitch*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::MudokonAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    auto pMudTlv = static_cast<Path_Mudokon*>(pTlv);
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnims(Mudokon::kMudMotionAnimIds);
        resMan.PendAnimation(AnimId::ObjectShadow);
    }
    else
    {
        relive_new Mudokon(pMudTlv, tlvId, resMan, map);
    }
}

void Factory::DoorFlameAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::Fire);
        resMan.PendAnimation(AnimId::Door_FireBackgroundGlow);
    }
    else
    {
        relive_new DoorFlame(static_cast<Path_DoorFlame*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::MovingBombAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    auto pMovingBombTlv = static_cast<Path_MovingBomb*>(pTlv);
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::MovingBomb);
        resMan.PendAnimation(AnimId::AirExplosion);
        resMan.PendAnimation(AnimId::Metal_Gib);
        resMan.PendAnims(kAbeGibs);
    }
    else
    {
        relive_new MovingBomb(pMovingBombTlv, tlvId, resMan, map);
    }
}

void Factory::TimerTriggerAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::ConstructObject_0)
    {
        relive_new TimerTrigger(static_cast<Path_TimerTrigger*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::SecurityDoorAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::Security_Door_Idle);
        resMan.PendAnimation(AnimId::Security_Door_Speak);
    }
    else
    {
        relive_new SecurityDoor(static_cast<Path_SecurityDoor*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::BoomMachineAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    auto pTlvBooMachine = static_cast<Path_BoomMachine*>(pTlv);
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::Grenade);
        resMan.PendAnimation(AnimId::AirExplosion);
        resMan.PendAnims(kAbeGibs);
        resMan.PendAnimation(AnimId::Metal_Gib);
        resMan.PendAnims(kSlogGibs);
        resMan.PendAnimation(AnimId::BoomMachine_Pipe_DropGrenade);
        resMan.PendAnimation(AnimId::BoomMachine_Pipe_Idle);
        resMan.PendAnimation(AnimId::BoomMachine_Button_Off);
        resMan.PendAnimation(AnimId::BoomMachine_Button_On);
    }
    else
    {
        relive_new BoomMachine(pTlvBooMachine, tlvId, resMan, map);
    }
}

void Factory::BackgroundAnimationAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode != LoadMode::LoadResourceFromList_1 && loadMode != LoadMode::LoadResource_2)
    {
        relive_new BackgroundAnimation(static_cast<Path_BackgroundAnimation*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::LCDScreenAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        //Map::LoadResource("LCDFONT.FNT", ResourceManager::ResourceType::Resource_Font, 2, loadMode); // TODO: Add to resource ID enum
        return;
    }

    relive_new LCDScreen(static_cast<Path_LCDScreen*>(pTlv), tlvId, resMan, map);
}

void Factory::HandStoneAE(Path_TLV*, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::SpotLight);
    }
    else
    {
        map.TLV_Reset(tlvId);
    }
}

void Factory::CreditsControllerAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode != LoadMode::LoadResourceFromList_1 && loadMode != LoadMode::LoadResource_2 && !gCreditsControllerExists)
    {
        relive_new CreditsController(pTlv, tlvId, resMan, map);
    }
}

void Factory::LCDStatusBoardAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        //Map::LoadResource("LCDFONT.FNT", ResourceManager::ResourceType::Resource_Font, AEResourceID::kLcdfontResID, loadMode);
        return;
    }
    
    relive_new LCDStatusBoard(static_cast<Path_LCDStatusBoard*>(pTlv), tlvId, resMan, map);
}

void Factory::WheelSyncerAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode != LoadMode::LoadResourceFromList_1 && loadMode != LoadMode::LoadResource_2)
    {
        relive_new WheelSyncer(static_cast<Path_WheelSyncer*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::MusicTriggerAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode != LoadMode::LoadResourceFromList_1 && loadMode != LoadMode::LoadResource_2)
    {
        relive_new MusicTrigger(static_cast<Path_MusicTrigger*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::SlogSpawnerAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        LoadSlogResourcesAE(resMan);
    }
    else
    {
        relive_new SlogSpawner(static_cast<Path_SlogSpawner*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::GasCountDownAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        //map.LoadResource("LCDFONT.FNT", ResourceManager::Resource_Font, AEResourceID::kLcdfontResID, loadMode);
        //map.LoadResource("ABEGAS.BAN", ResourceManager::Resource_Animation, AEResourceID::kAbegasResID, loadMode);
    }
    else
    {
        relive_new GasCountDown(static_cast<Path_GasCountDown*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::GasEmitterAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode != LoadMode::LoadResourceFromList_1 && loadMode != LoadMode::LoadResource_2)
    {
        relive_new GasEmitter(static_cast<Path_GasEmitter*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::ZzzSpawnerAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode != LoadMode::LoadResourceFromList_1 && loadMode != LoadMode::LoadResource_2)
    {
        relive_new ZzzSpawner(static_cast<Path_ZzzSpawner*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::GlukkonAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    Path_Glukkon* pGlukkonTlv = static_cast<Path_Glukkon*>(pTlv);
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        switch (pGlukkonTlv->mGlukkonType)
        {
            case Path_Glukkon::GlukkonTypes::eStoryAslik:
                for (s32 i = 0; i < 25; i++)
                {
                    const auto& anim = Glukkon::sGlukkonsAnimIdTable[1][i];
                    resMan.PendAnimation(anim);
                }
                resMan.PendAnimation(AnimId::Aslik_Head_Gib);
                resMan.PendAnimation(AnimId::Aslik_Arm_Gib);
                resMan.PendAnimation(AnimId::Aslik_Body_Gib);
                break;

            case Path_Glukkon::GlukkonTypes::eStoryDripik:
                for (s32 i = 0; i < 25; i++)
                {
                    const auto& anim = Glukkon::sGlukkonsAnimIdTable[2][i];
                    resMan.PendAnimation(anim);
                }
                resMan.PendAnimation(AnimId::Dripik_Head_Gib);
                resMan.PendAnimation(AnimId::Dripik_Arm_Gib);
                resMan.PendAnimation(AnimId::Dripik_Body_Gib);
                break;

            case Path_Glukkon::GlukkonTypes::eStoryPhleg:
                for (s32 i = 0; i < 25; i++)
                {
                    const auto& anim = Glukkon::sGlukkonsAnimIdTable[3][i];
                    resMan.PendAnimation(anim);
                }
                resMan.PendAnimation(AnimId::Phleg_Head_Gib);
                resMan.PendAnimation(AnimId::Phleg_Arm_Gib);
                resMan.PendAnimation(AnimId::Phleg_Body_Gib);
                break;

            case Path_Glukkon::GlukkonTypes::eNormal:
                for (s32 i = 0; i < 25; i++)
                {
                    const auto& anim = Glukkon::sGlukkonsAnimIdTable[0][i];
                    resMan.PendAnimation(anim);
                }
                resMan.PendAnimation(AnimId::Glukkon_Head_Gib);
                resMan.PendAnimation(AnimId::Glukkon_Arm_Gib);
                resMan.PendAnimation(AnimId::Glukkon_Body_Gib);
                break;
        }
    }
    else
    {
        relive_new Glukkon(pGlukkonTlv, tlvId, resMan, map);
    }
}

void Factory::WaterAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::WaterDrop);
        resMan.PendAnimation(AnimId::WaterSplash);
    }
    else
    {
        relive_new Water(static_cast<Path_Water*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::WorkWheelAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::Work_Wheel_Idle);
        resMan.PendAnimation(AnimId::Work_Wheel_Turning);
    }
    else
    {
        relive_new WorkWheel(static_cast<Path_WorkWheel*>(pTlv), tlvId, resMan, map);
    }
}

static void Create_LaughingGasAE(Layer layer, Path_TLV* pTlv, const Guid& tlvId, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (!gLaughingGasInstanceCount)
    {
        relive_new LaughingGas(layer, static_cast<Path_LaughingGas*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::LaughingGasAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode != LoadMode::LoadResourceFromList_1 && loadMode != LoadMode::LoadResource_2)
    {
        Create_LaughingGasAE(Layer::eLayer_Above_FG1_39, pTlv, tlvId, resMan, map);
    }
}

static void LoadFlyingSligResourcesAE(ResourceManagerWrapper& resMan)
{
    resMan.PendAnims(FlyingSlig::sFlyingSligAnimIdTable);
    resMan.PendAnims(kSligGibs);
    resMan.PendAnimation(AnimId::Grenade);
    resMan.PendAnimation(AnimId::AirExplosion_Small);
    resMan.PendAnimation(AnimId::Metal_Gib);
    resMan.PendAnims(kAbeGibs);
    resMan.PendAnimation(AnimId::ShootingFire_Particle);
    resMan.PendAnimation(AnimId::Vaporize_Particle);
    resMan.PendAnims(kSlogGibs);
}

void Factory::FlyingSligAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        LoadFlyingSligResourcesAE(resMan);
    }
    else
    {
        relive_new FlyingSlig(static_cast<Path_FlyingSlig*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::FleechAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnims(Fleech::sFleechAnimFromMotion);
        resMan.PendAnimation(AnimId::Fleech_Head_Gib);
        resMan.PendAnimation(AnimId::Fleech_Body_Gib);
    }
    else if (map.mCurrentLevel != EReliveLevelIds::eMudancheeVault_Ender || map.mCurrentPath != 9 || map.mCurrentCamera != 4)
    {
        relive_new Fleech(static_cast<Path_Fleech*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::SlurgAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::Slurg_Move);
        resMan.PendAnimation(AnimId::Slurg_Burst);
        resMan.PendAnimation(AnimId::Slurg_Turn_Around);
    }
    else
    {
        relive_new Slurg(static_cast<Path_Slurg*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::SlamDoorAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        switch (map.mCurrentLevel)
        {
            case EReliveLevelIds::eNecrum:
            case EReliveLevelIds::eMudomoVault:
            case EReliveLevelIds::eMudancheeVault:
            case EReliveLevelIds::eMudancheeVault_Ender:
            case EReliveLevelIds::eMudomoVault_Ender:
                resMan.PendAnimation(AnimId::Slam_Door_Vault_Closed);
                resMan.PendAnimation(AnimId::Slam_Door_Vault_Closing);
                resMan.PendAnimation(AnimId::Slam_Door_Vault_Opening);
                break;
            default:
                resMan.PendAnimation(AnimId::Slam_Door_Industrial_Closed);
                resMan.PendAnimation(AnimId::Slam_Door_Industrial_Closing);
                resMan.PendAnimation(AnimId::Slam_Door_Industrial_Opening);
                break;
        }
    }
    else
    {
        relive_new SlamDoor(static_cast<Path_SlamDoor*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::LevelLoaderAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode != LoadMode::LoadResourceFromList_1 && loadMode != LoadMode::LoadResource_2)
    {
        relive_new LevelLoader(static_cast<Path_LevelLoader*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::DemoSpawnPointAE(Path_TLV*, const Guid& /*tlvId*/, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode != LoadMode::LoadResourceFromList_1 && loadMode != LoadMode::LoadResource_2)
    {
        if (gAttract)
        {
            if (!gActiveDemoPlayback)
            {
                relive_new DemoPlayback(resMan, map);
            }
        }
    }
}

void Factory::TeleporterAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode != LoadMode::LoadResourceFromList_1 && loadMode != LoadMode::LoadResource_2)
    {
        relive_new Teleporter(static_cast<Path_Teleporter*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::SlurgSpawnerAE(Path_TLV* /*pTlv*/,  const Guid& /*tlvId*/, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap&)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::Slurg_Move);
        resMan.PendAnimation(AnimId::Slurg_Burst);
        resMan.PendAnimation(AnimId::Slurg_Turn_Around);
    }
    else
    {
       /*
       // TODO/HACK disabled because it crashes the game when a quick load is done
       // after you save when a slurg is spawned as it has no TLV.
       // Its only used to spawn 1 slurg in 1 screen of the game. Fix in abi_break
       // branch by converting to a normal slurg.
        relive_new SlurgSpawner(static_cast<Path_SlurgSpawner*>(pTlv), tlvId, resMan, map);
        */
    }
}

void Factory::DrillAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::Drill_Horizontal_Off);
        resMan.PendAnimation(AnimId::Drill_Horizontal_On);
        resMan.PendAnimation(AnimId::Drill_Vertical_Off);
        resMan.PendAnimation(AnimId::Drill_Vertical_On);
        resMan.PendAnims(kAbeGibs);
        resMan.PendAnims(kSlogGibs);
    }
    else
    {
        relive_new Drill(static_cast<Path_Drill*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::ColourfulMeterAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        //map.LoadResource("LCDFONT.FNT", ResourceManager::Resource_Font, AEResourceID::kLcdfontResID, loadMode);
    }
    else
    {
        relive_new ColourfulMeter(static_cast<Path_ColourfulMeter*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::FlyingSligSpawnerAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        LoadFlyingSligResourcesAE(resMan);
    }
    else
    {
        relive_new FlyingSligSpawner(static_cast<Path_FlyingSligSpawner*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::MineCarAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::Mine_Car_Closed);
        resMan.PendAnimation(AnimId::Mine_Car_Open);
        resMan.PendAnimation(AnimId::Mine_Car_Shake_A);
        resMan.PendAnimation(AnimId::Mine_Car_Shake_B);
        resMan.PendAnimation(AnimId::Mine_Car_Tread_Idle);
        resMan.PendAnimation(AnimId::Mine_Car_Tread_Move_A);
        resMan.PendAnimation(AnimId::Mine_Car_Tread_Move_B);
        resMan.PendAnimation(AnimId::Metal_Gib);
        resMan.PendAnimation(AnimId::AirExplosion);
        resMan.PendAnims(kAbeGibs);
    }
    else
    {
        relive_new MineCar(static_cast<Path_MineCar*>(pTlv), tlvId, 0, 0, 0, resMan, map);
    }
}

void Factory::BoneBagAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::BoneBag_Idle);
        resMan.PendAnimation(AnimId::BoneBag_HardHit);
        resMan.PendAnimation(AnimId::BoneBag_SoftHit);
        resMan.PendAnimation(AnimId::Bone);
    }
    else
    {
        relive_new BoneBag(static_cast<Path_BoneBag*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::ExplosionSetAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {

        if (map.mCurrentLevel == EReliveLevelIds::eBonewerkz)
        {
            resMan.PendAnimation(AnimId::FallingCrate_Falling);
            resMan.PendAnimation(AnimId::FallingCrate_Waiting);
        }
        else
        {
            resMan.PendAnimation(AnimId::AE_FallingRock_Falling);
            resMan.PendAnimation(AnimId::AE_FallingRock_Waiting);
        }
        resMan.PendAnimation(AnimId::HoistRock1);
        resMan.PendAnimation(AnimId::HoistRock2);
        resMan.PendAnimation(AnimId::HoistRock3);
        resMan.PendAnimation(AnimId::Explosion_Stick);
        resMan.PendAnimation(AnimId::ObjectShadow);
        resMan.PendAnimation(AnimId::Explosion_Rock);
        resMan.PendAnimation(AnimId::AirExplosion);
    }
    else
    {
        if (!gExplosionSet)
        {
            // gExplosionSet is assigned in the ctor (OWI programming)
            relive_new ExplosionSet(resMan, map);
        }
        gExplosionSet->Init(static_cast<Path_ExplosionSet*>(pTlv));
        map.TLV_Reset(tlvId);
    }
}

void Factory::MultiSwitchControllerAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode != LoadMode::LoadResourceFromList_1 && loadMode != LoadMode::LoadResource_2)
    {
        relive_new MultiSwitchController(static_cast<Path_MultiSwitchController*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::StatusLightAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::Status_Light_Green);
        resMan.PendAnimation(AnimId::Status_Light_Red);
    }
    else
    {
        relive_new StatusLight(static_cast<Path_StatusLight*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::SlapLockAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::SlapLock_Initiate);
        resMan.PendAnimation(AnimId::SlapLock_Punched);
        resMan.PendAnimation(AnimId::SlapLock_Shaking);
    }
    else
    {
        relive_new SlapLock(static_cast<Path_SlapLock*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::ParamiteWebLineAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::ParamiteWeb);
    }
    else
    {
        relive_new ParamiteWebLine(static_cast<Path_ParamiteWebLine*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::AlarmAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::ConstructObject_0)
    {
        relive_new Alarm(static_cast<Path_Alarm*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::BrewMachineAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::BrewMachine_Button);
        resMan.PendAnimation(AnimId::Fart);
        resMan.PendAnimation(AnimId::AirExplosion);
        resMan.PendAnims(kAbeGibs);
        //map.LoadResource("LCDFONT.FNT", ResourceManager::Resource_Font, AEResourceID::kLcdfontResID, loadMode);
    }
    else
    {
        relive_new BrewMachine(static_cast<Path_BrewMachine*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::ScrabSpawnerAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnims(Scrab::sScrabMotionAnimIds);
    }
    else
    {
        relive_new ScrabSpawner(static_cast<Path_ScrabSpawner*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::CrawlingSligAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnims(CrawlingSlig::sCrawlingSligAnimIdTable);
        resMan.PendAnims(kSligGibs);
        resMan.PendAnimation(AnimId::Grenade);
        resMan.PendAnimation(AnimId::AirExplosion_Small);
        resMan.PendAnimation(AnimId::Metal_Gib);
        resMan.PendAnims(kAbeGibs);
        resMan.PendAnimation(AnimId::ShootingFire_Particle);
        resMan.PendAnimation(AnimId::Vaporize_Particle);
        resMan.PendAnims(kSlogGibs);
    }
    else
    {
        relive_new CrawlingSlig(static_cast<Path_CrawlingSlig*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::SligGetPantsAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        LoadWalkingSligResourcesAE(resMan);
        resMan.PendAnimation(AnimId::CrawlingSligLocker_Closed);
        resMan.PendAnimation(AnimId::CrawlingSligLocker_Open);
    }
    else
    {
        relive_new SligGetPantsAndWings(pTlv, tlvId, resMan, map);
    }
}

void Factory::SligGetWingsAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        LoadFlyingSligResourcesAE(resMan);
        resMan.PendAnimation(AnimId::CrawlingSligLocker_Closed);
        resMan.PendAnimation(AnimId::CrawlingSligLocker_Open);
    }
    else
    {
        relive_new SligGetPantsAndWings(pTlv, tlvId, resMan, map);
    }
}

void Factory::GreeterAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::MotionDetector_Flare);
        resMan.PendAnimation(AnimId::MotionDetector_Laser);
        resMan.PendAnimation(AnimId::Greeter_Moving);
        resMan.PendAnimation(AnimId::Greeter_Turn);
        resMan.PendAnimation(AnimId::Greeter_Hit);
        resMan.PendAnimation(AnimId::Greeter_Speak);
        resMan.PendAnimation(AnimId::Greeter_Chase);
        resMan.PendAnimation(AnimId::Greeter_Falling);
        resMan.PendAnimation(AnimId::Zap_Line_Blue);
        resMan.PendAnimation(AnimId::Zap_Line_Red);
        resMan.PendAnimation(AnimId::Metal_Gib);
        resMan.PendAnimation(AnimId::AirExplosion);
        resMan.PendAnims(kAbeGibs);
    }
    else
    {
        relive_new Greeter(static_cast<Path_Greeter*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::CrawlingSligButtonAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::CrawlingSligButton);
        resMan.PendAnimation(AnimId::CrawlingSligButtonUse);
    }
    else
    {
        relive_new CrawlingSligButton(static_cast<Path_CrawlingSligButton*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::GlukkonSwitchAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::Security_Door_Idle);
        resMan.PendAnimation(AnimId::Security_Door_Speak);
    }
    else
    {
        relive_new GlukkonSwitch(static_cast<Path_GlukkonSwitch*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::DoorBlockerAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::Door_Lock_Idle);
        resMan.PendAnimation(AnimId::Door_Lock_Open);
    }
    else
    {
        relive_new DoorBlocker(static_cast<Path_DoorBlocker*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::RollingBallStopperAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::Stone_Ball_Stopper);
    }
    else
    {
        relive_new RollingBallStopper(static_cast<Path_RollingBallStopper*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::TorturedMudokonAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (!pTlv->mTlvSpecificMeaning)
    {
        if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
        {
            resMan.PendAnimation(AnimId::Tortured_Mudokon);
            resMan.PendAnimation(AnimId::Tortured_Mudokon_Released);
            resMan.PendAnimation(AnimId::Tortured_Mudokon_Zap);
            resMan.PendAnimation(AnimId::Tortured_Mudokon_Tears);
            resMan.PendAnimation(AnimId::Electric_Wall);
        }
        else
        {
            relive_new TorturedMudokon(static_cast<Path_TorturedMudokon*>(pTlv), tlvId, resMan, map);
        }
    }
}

void Factory::TrainDoorAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::Door_Train_Closed);
        resMan.PendAnimation(AnimId::Door_Train_Closing);
    }
    else
    {
        relive_new TrainDoor(static_cast<Path_TrainDoor*>(pTlv), tlvId, resMan, map);
    }
}

void Factory::RollingBallAE(Path_TLV* pTlv, const Guid& tlvId, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (loadMode == LoadMode::LoadResourceFromList_1 || loadMode == LoadMode::LoadResource_2)
    {
        resMan.PendAnimation(AnimId::Stone_Ball);
        resMan.PendAnimation(AnimId::Stone_Ball_Rolling);
        resMan.PendAnimation(AnimId::Explosion_Rock);
    }
    else
    {
        relive_new RollingBall(static_cast<Path_RollingBall*>(pTlv), tlvId, resMan, map);
    }
}

// namespace AE END

void Factory::ConstructTLVObject(Path_TLV* pTlv, const Guid& tlvInfo, LoadMode loadMode, ResourceManagerWrapper& resMan, BaseMap& map)
{
#define FACTORY(method, pTlv, tlvInfo, loadMode, resMan, map) \
    if (GetGameType() == GameType::eAo) \
    { \
        method##AO(pTlv, tlvInfo, loadMode, resMan, map); \
    } \
    else \
    { \
        method##AE(pTlv, tlvInfo, loadMode, resMan, map); \
    }

    switch (pTlv->mTlvType)
    {
        case ReliveTypes::eBackgroundAnimation:
            FACTORY(BackgroundAnimation, pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eLiftMover:
            FACTORY(LiftMover, pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eTimedMine:
            FACTORY(TimedMine, pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eGasCountDown:
            FACTORY(GasCountDown, pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eDoor:
            //if (GetGameType() == GameType::eAo)
            //Door(pTlv, tlvInfo, loadMode, resMan, map);
            //else
            DoorAE(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eElectricWall:
            FACTORY(ElectricWall, pTlv, tlvInfo, loadMode, resMan, map);
            break;            
        case ReliveTypes::eSecurityClaw:
            if (GetGameType() == GameType::eAo)
            SecurityClawAO(pTlv, tlvInfo, loadMode, resMan, map);
            else
            ChantSuppressor_WithArmsAE(pTlv, tlvInfo, loadMode, resMan, map);
            break;            
        case ReliveTypes::eBoomMachine:
            FACTORY(BoomMachine, pTlv, tlvInfo, loadMode, resMan, map);
            break;           
        case ReliveTypes::eMudokon:
            if (GetGameType() == GameType::eAo)
            WorkerMudokonAO(pTlv, tlvInfo, loadMode, resMan, map);
            else
            MudokonAE(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eLiftPoint:
            FACTORY(LiftPoint, pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eSecurityOrb:
            FACTORY(SecurityOrb, pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eMeatSack:
            FACTORY(MeatSack, pTlv, tlvInfo, loadMode, resMan, map);
            break;           
        case ReliveTypes::eMine:
            FACTORY(Mine, pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eMotionDetector:
            FACTORY(MotionDetector, pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eRollingBallStopper:
            FACTORY(RollingBallStopper, pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eParamite:
            FACTORY(Paramite, pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eBirdPortal:
            FACTORY(BirdPortal, pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::ePullRingRope:
            FACTORY(PullRingRope, pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eRockSack:
            FACTORY(RockSack, pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eRollingBall:
            FACTORY(RollingBall, pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eScrab:
            FACTORY(Scrab, pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eSlig:
            FACTORY(Slig, pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eSlog:
            FACTORY(Slog, pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eSligSpawner:
            FACTORY(SligSpawner, pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eLever:
            FACTORY(Lever, pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eTrapDoor:
            FACTORY(TrapDoor, pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eUXB:
            FACTORY(UXB, pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eMusicTrigger:
            FACTORY(MusicTrigger, pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eWellLocal:
        case ReliveTypes::eWellExpress:
            FACTORY(Well, pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eZzzSpawner:
            FACTORY(ZzzSpawner, pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eHoist:
            FACTORY(Hoist, pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eEdge:
            FACTORY(Edge, pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eShadowZone:
            FACTORY(ShadowZone, pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eDove:
            FACTORY(Dove, pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eFallingItem:
            FACTORY(FallingItem, pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eAbeStart:
            FACTORY(AbeStart, pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eFootSwitch:
            FACTORY(FootSwitch, pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eSligBoundRight:
            FACTORY(SligBoundRight, pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eSligBoundLeft:
            FACTORY(SligBoundLeft, pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eGasEmitter:
            FACTORY(GasEmitter, pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eSlogSpawner:
            FACTORY(SlogSpawner, pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eLCDStatusBoard:
            FACTORY(LCDStatusBoard, pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eCreditsController:
            FACTORY(CreditsController, pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eHandStone:
            FACTORY(HandStone, pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eSecurityDoor:
            FACTORY(SecurityDoor, pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eMainMenuController:
            FACTORY(MainMenuController, pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eMovingBomb:
            FACTORY(MovingBomb, pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eDoorFlame:
            // TODO: Almost exactly the same - can be merged
            FACTORY(DoorFlame, pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eInvisibleSwitch:
            FACTORY(InvisibleSwitch, pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eTimerTrigger:
            FACTORY(TimerTrigger, pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eLCDScreen:
            FACTORY(LCDScreen, pTlv, tlvInfo, loadMode, resMan, map);
            break;

        // AO only currently
        case ReliveTypes::eRingCancel:
            RingCancelAO(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::SlingMud:
            SlingMudAO(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eKillUnsavedMuds:
            KillUnsavedMudsAO(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eBat:
            BatAO(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eChimeLock:
            ChimeLockAO(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eElum:
            ElumAO(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eBellHammer:
            BellHammerAO(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eFlintLockFire:
            FlintLockFireAO(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eBackgroundGlukkon:
            BackgroundGlukkonAO(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eHoneySack:
            HoneySackAO(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eHoney:
            HoneyAO(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eMeatSaw:
            MeatSawAO(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eZBall:
            ZBallAO(pTlv, tlvInfo, loadMode, resMan, map);
            break;          
        case ReliveTypes::eElumWall:
            ElumWallAO(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eBeeNest:
            BeeNestAO(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eRingMudokon:
            RingMudAO(pTlv, tlvInfo, loadMode, resMan, map);
            break;        
        case ReliveTypes::eLiftMudokon:
            LiftMudAO(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eBeeSwarmHole:
            BeeSwarmHoleAO(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eLightEffect:
            LightEffectAO(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eSwitchStateBooleanLogic:
            SwitchStateBooleanLogicAO(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eHintFly:
            HintFlyAO(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eBellSongStone:
            BellSongStoneAO(pTlv, tlvInfo, loadMode, resMan, map);
            break;

        // AE only currently
        case ReliveTypes::eMovieHandStone:
            MovieHandStoneAE(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eWheelSyncer:
            WheelSyncerAE(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eGlukkon:
            GlukkonAE(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eWater:
            WaterAE(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eWorkWheel:
            WorkWheelAE(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eLaughingGas:
            LaughingGasAE(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eFlyingSlig:
            FlyingSligAE(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eFleech:
            FleechAE(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eSlurg:
            SlurgAE(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eSlamDoor:
            SlamDoorAE(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eLevelLoader:
            LevelLoaderAE(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eDemoSpawnPoint:
            DemoSpawnPointAE(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eTeleporter:
            TeleporterAE(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eSlurgSpawner:
            SlurgSpawnerAE(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eDrill:
            DrillAE(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eColourfulMeter:
            ColourfulMeterAE(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eFlyingSligSpawner:
            FlyingSligSpawnerAE(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eMineCar:
            MineCarAE(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eBoneBag:
            BoneBagAE(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eExplosionSet:
            ExplosionSetAE(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eMultiSwitchController:
            MultiSwitchControllerAE(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eStatusLight:
            StatusLightAE(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eSlapLock:
            SlapLockAE(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eParamiteWebLine:
            ParamiteWebLineAE(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eAlarm:
            AlarmAE(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eBrewMachine:
            BrewMachineAE(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eScrabSpawner:
            ScrabSpawnerAE(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eCrawlingSlig:
            CrawlingSligAE(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eSligGetPants:
            SligGetPantsAE(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eSligGetWings:
            SligGetWingsAE(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eGreeter:
            GreeterAE(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eCrawlingSligButton:
            CrawlingSligButtonAE(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eGlukkonSwitch:
            GlukkonSwitchAE(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eDoorBlocker:
            DoorBlockerAE(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eTorturedMud:
            TorturedMudokonAE(pTlv, tlvInfo, loadMode, resMan, map);
            break;
        case ReliveTypes::eTrainDoor:
            TrainDoorAE(pTlv, tlvInfo, loadMode, resMan, map);
            break;

    }

    #undef FACTORY
}

} // namespace relive

