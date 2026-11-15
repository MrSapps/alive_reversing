#include <array>
#include "GameView.hpp"
#include "../GameObjects/IBirdPortal.hpp"
#include "../BaseMap.hpp"
#include "../BinaryPath.hpp"
#include "../Collisions.hpp"
#include "../data_conversion/relive_tlvs.hpp"
#include "../Engine.hpp"
#include "../GameType.hpp"
#include "../GameObjects/BaseAliveGameObject.hpp"
#include "../data_conversion/EnumSerialization.hpp"
#include "../../AliveLibAE/Abe.hpp"
#include "../../AliveLibAE/Input.hpp"
#include "../../AliveLibAO/Abe.hpp"
#include "../../AliveLibAE/Mudokon.hpp"
#include "../../AliveLibAE/UXB.hpp"
#include "../../AliveLibAE/Drill.hpp"
#include "../../AliveLibAE/MotionDetector.hpp"
#include "../../AliveLibAE/Slig.hpp"
#include "../../AliveLibAE/Slog.hpp"
#include "../../AliveLibAE/Fleech.hpp"
#include "../../AliveLibAE/Paramite.hpp"
#include "../../AliveLibAE/SlapLock.hpp"
#include "../../AliveLibAE/DDCheat.hpp"
#include "../../AliveLibAO/Mudokon.hpp"
#include "../../AliveLibAO/DDCheat.hpp"
#include "../SwitchStates.hpp"
#include "../GameObjects/PlatformBase.hpp"

extern s16 gNumCamSwappers; // Engine.cpp

namespace Bot {

static_assert(Btn::eUp == static_cast<u32>(InputCommands::eUp), "Btn must match InputCommands");
static_assert(Btn::eRun == static_cast<u32>(InputCommands::eRun), "Btn must match InputCommands");
static_assert(Btn::eAction == static_cast<u32>(InputCommands::eDoAction), "Btn must match InputCommands");
static_assert(Btn::eHop == static_cast<u32>(InputCommands::eHop), "Btn must match InputCommands");
static_assert(Btn::eFartOrRoll == static_cast<u32>(InputCommands::eFartOrRoll), "Btn must match InputCommands");
static_assert(Btn::eSpeak1 == static_cast<u32>(InputCommands::eGameSpeak1), "Btn must match InputCommands");
static_assert(Btn::eSpeak8 == static_cast<u32>(InputCommands::eGameSpeak8), "Btn must match InputCommands");
static_assert(Btn::eChant == static_cast<u32>(InputCommands::eChant), "Btn must match InputCommands");

const char* MoveName(Move move)
{
    switch (move)
    {
        case Move::eIdle: return "idle";
        case Move::eCrouch: return "crouch";
        case Move::eHang: return "hang";
        case Move::eLiftIdle: return "lift";
        case Move::eTurn: return "turn";
        case Move::eWalk: return "walk";
        case Move::eRun: return "run";
        case Move::eSneak: return "sneak";
        case Move::eRoll: return "roll";
        case Move::eAir: return "air";
        case Move::eDead: return "dead";
        case Move::eBusy: return "busy";
    }
    return "?";
}

static Move ClassifyAE(s16 motion)
{
    using M = ::eAbeMotions;
    switch (static_cast<M>(motion))
    {
        case M::Motion_0_Idle_44EEB0:
            return Move::eIdle;
        case M::Motion_2_StandingTurn_451830:
            return Move::eTurn;
        case M::Motion_1_WalkLoop_44FBA0:
        case M::Motion_4_WalkToIdle_44FFC0:
        case M::Motion_5_MidWalkToIdle_450080:
        case M::Motion_6_WalkBegin_44FEE0:
            return Move::eWalk;
        case M::Motion_40_SneakLoop_450550:
        case M::Motion_41_WalkToSneak_450250:
        case M::Motion_42_SneakToWalk_4503D0:
        case M::Motion_43_MidWalkToSneak_450380:
        case M::Motion_44_MidSneakToWalk_450500:
        case M::Motion_45_SneakBegin_4507A0:
        case M::Motion_46_SneakToIdle_450870:
        case M::jMotion_47_MidSneakToIdle_4508C0:
            return Move::eSneak;
        case M::Motion_33_RunLoop_4508E0:
        case M::Motion_39_StandingToRun_450D40:
        case M::Motion_48_WalkToRun_4500A0:
        case M::Motion_49_MidWalkToRun_450200:
        case M::Motion_50_RunToWalk_450E20:
        case M::Motion_51_MidRunToWalk_450F50:
        case M::Motion_25_RunSlideStop_451330:
        case M::Motion_26_RunTurn_451500:
        case M::Motion_52_RunTurnToRun_451710:
        case M::Motion_53_RunTurnToWalk_451800:
        case M::Motion_54_RunJumpLandRun_4538F0:
        case M::Motion_55_RunJumpLandWalk_453970:
        case M::Motion_61_TurnToRun_456530:
            return Move::eRun;
        case M::Motion_3_Fall_459B60:
        case M::Motion_27_HopBegin_4521C0:
        case M::Motion_28_HopMid_451C50:
        case M::Motion_30_RunJumpBegin_4532E0:
        case M::Motion_31_RunJumpMid_452C10:
        case M::jMotion_85_Fall_455070:
        case M::Motion_91_FallingFromGrab:
        case M::Motion_93_WalkOffEdge:
        case M::Motion_94_RunOffEdge:
        case M::Motion_95_SneakOffEdge:
        case M::Motion_96_HopToFall:
        case M::Motion_97_RunJumpToFall:
        case M::Motion_98_RollOffEdge:
            return Move::eAir;
        case M::Motion_17_CrouchIdle_456BC0:
            return Move::eCrouch;
        case M::Motion_22_RollBegin_4539A0:
        case M::Motion_23_RollLoop_453A90:
        case M::Motion_24_453D00:
        case M::jMotion_38_RunToRoll_453A70:
            return Move::eRoll;
        case M::Motion_67_LedgeHang_454E20:
        case M::Motion_69_LedgeHangWobble_454EF0:
            return Move::eHang;
        case M::Motion_123_LiftGrabIdle:
            return Move::eLiftIdle;
        case M::Motion_56_DeathDropFall_4591F0:
        case M::Motion_57_Dead_4589A0:
        case M::Motion_58_DeadPre_4593E0:
        case M::Motion_84_FallLandDie_45A420:
        case M::Motion_109_ZShotRolling:
        case M::Motion_110_ZShot:
        case M::Motion_129_PoisonGasDeath:
            return Move::eDead;
        default:
            return Move::eBusy;
    }
}

static Move ClassifyAO(s16 motion)
{
    using M = AO::eAbeMotions;
    switch (static_cast<M>(motion))
    {
        case M::Motion_0_Idle:
        case M::Motion_159_Idle_RubEyes:
        case M::Motion_160_Idle_Stretch_Arms:
        case M::Motion_161_Idle_Yawn:
            return Move::eIdle;
        case M::Motion_2_StandingTurn:
            return Move::eTurn;
        case M::Motion_1_WalkLoop:
        case M::Motion_4_WalkToIdle:
        case M::Motion_5_MidWalkToIdle:
        case M::Motion_6_WalkBegin:
            return Move::eWalk;
        case M::Motion_42_SneakLoop:
        case M::Motion_43_WalkToSneak:
        case M::Motion_44_SneakToWalk:
        case M::Motion_45_MidWalkToSneak:
        case M::Motion_46_MidSneakToWalk:
        case M::Motion_47_SneakBegin:
        case M::Motion_48_SneakToIdle:
        case M::Motion_49_MidSneakToIdle:
            return Move::eSneak;
        case M::Motion_35_RunLoop:
        case M::Motion_41_StandingToRun:
        case M::Motion_50_WalkToRun:
        case M::Motion_51_MidWalkToRun:
        case M::Motion_52_RunToWalk:
        case M::Motion_53_MidRunToWalk:
        case M::Motion_27_RunSlideStop:
        case M::Motion_28_RunTurn:
        case M::Motion_54_RunTurnToRun:
        case M::Motion_55_RunTurnToWalk:
        case M::Motion_56_RunJumpLandRun:
        case M::Motion_57_RunJumpLand_Walk:
        case M::Motion_63_TurnToRun:
            return Move::eRun;
        case M::Motion_3_Fall:
        case M::Motion_29_HopBegin:
        case M::Motion_30_HopMid:
        case M::Motion_32_RunJumpBegin:
        case M::Motion_33_RunJumpMid:
        case M::Motion_87_ToFall:
        case M::Motion_91_FallingFromGrab:
        case M::Motion_93_WalkOffEdge:
        case M::Motion_94_RunOffEdge:
        case M::Motion_95_SneakOffEdge:
        case M::Motion_96_HopToFall:
        case M::Motion_97_RunJumpToFall:
        case M::Motion_100_RollOffLedge:
            return Move::eAir;
        case M::Motion_19_CrouchIdle:
            return Move::eCrouch;
        case M::Motion_24_RollBegin:
        case M::Motion_25_RollLoop:
        case M::Motion_26_RollEnd:
        case M::Motion_40_RunToRoll:
            return Move::eRoll;
        case M::Motion_66_LedgeHang:
        case M::Motion_68_LedgeHangWobble:
            return Move::eHang;
        case M::Motion_135_LiftGrabIdle:
            return Move::eLiftIdle;
        case M::Motion_59_DeathDropFall:
        case M::Motion_60_Dead:
        case M::Motion_86_FallLandDie:
        case M::Motion_147_ShotRolling:
        case M::Motion_148_Shot:
        case M::Motion_164_PoisonGasDeath:
            return Move::eDead;
        default:
            return Move::eBusy;
    }
}

Move GameView::ClassifyMotion(bool isAe, s16 motion)
{
    return isAe ? ClassifyAE(motion) : ClassifyAO(motion);
}

std::string GameView::LevelName(s32 level)
{
    return nlohmann::json(static_cast<EReliveLevelIds>(level)).get<std::string>();
}

AbeState GameView::ReadAbe(BaseMap& map)
{
    AbeState s;
    const bool isAe = GetGameType() == GameType::eAe;
    BaseAliveGameObject* pAbe = isAe ? static_cast<BaseAliveGameObject*>(gAbe) : static_cast<BaseAliveGameObject*>(AO::gAbe);
    s.mFrame = sGnFrame;
    s.mLevel = static_cast<s32>(map.mCurrentLevel);
    s.mPath = map.mCurrentPath;
    s.mCamera = map.mCurrentCamera;
    if (!pAbe)
    {
        return s;
    }

    s.mValid = true;
    s.mXFp = pAbe->mXPos.fpValue;
    s.mYFp = pAbe->mYPos.fpValue;
    s.mX = FP_GetExponent(pAbe->mXPos);
    s.mY = FP_GetExponent(pAbe->mYPos);
    s.mVelXFp = pAbe->mVelX.fpValue;
    s.mVelYFp = pAbe->mVelY.fpValue;
    s.mFacingLeft = pAbe->GetAnimation().GetFlipX();
    s.mHalfScale = pAbe->GetSpriteScale() != FP_FromInteger(1);
    s.mTop = pAbe->VGetBoundingRect().y;
    s.mMotion = pAbe->VGetMotion(eMotionType::eCurrentMotion);
    s.mAnimFrame = static_cast<s16>(pAbe->GetAnimation().GetCurrentFrame());
    s.mMove = ClassifyMotion(isAe, s.mMotion);
    s.mHealthFp = pAbe->mHealth.fpValue;
    if (!(pAbe->mHealth > FP_FromInteger(0)))
    {
        s.mMove = Move::eDead;
    }
    s.mOnGround = pAbe->BaseAliveGameObjectCollisionLine != nullptr;
    s.mTransition = gNumCamSwappers != 0 || map.mCamState != BaseMap::CamChangeStates::eInactive_0 || map.GetActiveModal() != nullptr
                 || pAbe->mCurrentLevel != map.mCurrentLevel || pAbe->mCurrentPath != map.mCurrentPath;
    return s;
}

static NavFeature MakeFeature(FeatureKind kind, const relive::Path_TLV& tlv, s32 camera, u32 id)
{
    NavFeature f;
    f.mKind = kind;
    f.mX1 = tlv.mTopLeftX;
    f.mY1 = tlv.mTopLeftY;
    f.mX2 = tlv.mBottomRightX;
    f.mY2 = tlv.mBottomRightY;
    f.mCamera = camera;
    f.mId = id;
    return f;
}

static s32 OpOf(relive::reliveSwitchOp op)
{
    switch (op)
    {
        case relive::reliveSwitchOp::eSetTrue: return 0;
        case relive::reliveSwitchOp::eSetFalse: return 1;
        default: return 2;
    }
}

static s32 DirOf(s32 grabDirection)
{
    // eFacingLeft, eFacingRight, eFacingAnyDirection in both Path_Hoist and Path_Edge
    return grabDirection;
}

NavInput GameView::BuildNavInput(BinaryPath& path, s32 level, s32 pathId)
{
    NavInput in;
    std::vector<std::array<s32, 3>> movers;
    const bool isAe = GetGameType() == GameType::eAe;
    in.mIsAe = isAe;
    in.mCamW = isAe ? 375 : 1024;
    in.mCamH = isAe ? 260 : 480;

    for (const PathLine& line : path.GetCollisions())
    {
        NavLine l;
        l.mX1 = line.mRect.x;
        l.mY1 = line.mRect.y;
        l.mX2 = line.mRect.w;
        l.mY2 = line.mRect.h;
        l.mType = static_cast<s32>(line.mLineType);
        in.mLines.push_back(l);
    }

    const auto setDest = [&](NavFeature& f, EReliveLevelIds destLevel, s32 destPath, s32 destCamera)
    {
        f.mDestCamera = destCamera;
        if (static_cast<s32>(destLevel) != level || destPath != pathId)
        {
            f.mDestLevel = static_cast<s32>(destLevel);
            f.mDestPath = destPath;
        }
    };

    for (auto& cam : path.GetCameras())
    {
        if (!cam)
        {
            continue;
        }
        const s32 camera = std::atoi(cam->mName.c_str());
        u32 idx = 0;
        for (auto& upTlv : cam->mTlvs.mTlvs)
        {
            const relive::Path_TLV& tlv = *upTlv;
            const u32 id = static_cast<u32>(camera) * 1000 + idx++;
            switch (tlv.mTlvType)
            {
                case ReliveTypes::eHoist:
                {
                    const auto& t = static_cast<const relive::Path_Hoist&>(tlv);
                    NavFeature f = MakeFeature(FeatureKind::eHoist, tlv, camera, id);
                    f.mDir = DirOf(static_cast<s32>(t.mGrabDirection));
                    f.mHalf = t.mScale == relive::reliveScale::eHalf;
                    f.mAnyScale = !isAe;
                    f.mA = t.mHoistType == relive::Path_Hoist::Type::eOffScreen ? 1 : 0;
                    in.mFeatures.push_back(f);
                    break;
                }
                case ReliveTypes::eInvisibleSwitch:
                {
                    const auto& t = static_cast<const relive::Path_InvisibleSwitch&>(tlv);
                    NavFeature f = MakeFeature(FeatureKind::eSwitchZone, tlv, camera, id);
                    f.mSwitch = t.mSwitchId;
                    f.mA = t.mAction == relive::reliveSwitchOp::eSetTrue ? 0 : t.mAction == relive::reliveSwitchOp::eSetFalse ? 1 : 2;
                    f.mHalf = t.mScale == relive::Path_InvisibleSwitch::InvisibleSwitchScale::eHalf;
                    f.mAnyScale = t.mScale == relive::Path_InvisibleSwitch::InvisibleSwitchScale::eAny;
                    in.mFeatures.push_back(f);
                    break;
                }
                case ReliveTypes::eEdge:
                {
                    const auto& t = static_cast<const relive::Path_Edge&>(tlv);
                    NavFeature f = MakeFeature(FeatureKind::eEdge, tlv, camera, id);
                    f.mDir = DirOf(static_cast<s32>(t.mGrabDirection));
                    f.mHalf = t.mScale == relive::reliveScale::eHalf;
                    f.mAnyScale = !isAe;
                    f.mA = t.mCanGrab ? 1 : 0;
                    in.mFeatures.push_back(f);
                    break;
                }
                case ReliveTypes::eDoor:
                {
                    const auto& t = static_cast<const relive::Path_Door&>(tlv);
                    NavFeature f = MakeFeature(FeatureKind::eDoor, tlv, camera, id);
                    f.mHalf = t.mScale == relive::reliveScale::eHalf;
                    f.mA = t.mDoorId;
                    f.mB = t.mTargetDoorId;
                    f.mSwitch = t.mSwitchId;
                    f.mStartClosed = t.mStartState == relive::Path_Door::eClosed;
                    if (isAe && (t.mDoorType == relive::Path_Door::eTasksDoor || t.mDoorType == relive::Path_Door::eTasksDoorWithSecretMusic))
                    {
                        // Opens once all its hubs' switches are on (the door sets its own switch then, but only
                        // while it is on the screen)
                        f.mSwitch = World::DoorSwitch(id);
                        f.mStartClosed = true;
                    }
                    setDest(f, t.mNextLevel, t.mNextPath, t.mNextCamera);
                    in.mFeatures.push_back(f);
                    break;
                }
                case ReliveTypes::eBirdPortal:
                {
                    const auto& t = static_cast<const relive::Path_BirdPortal&>(tlv);
                    if (t.mPortalType == relive::Path_BirdPortal::PortalType::eAbe)
                    {
                        NavFeature f = MakeFeature(FeatureKind::ePortal, tlv, camera, id);
                        f.mHalf = t.mScale == relive::reliveScale::eHalf;
                        f.mDir = t.mEnterSide == relive::Path_BirdPortal::PortalSide::eLeft ? 0 : 1;
                        f.mSwitch = t.mCreatePortalSwitchId;
                        f.mA = t.mDeletePortalSwitchId;
                        setDest(f, t.mExitLevel, t.mExitPath, t.mExitCamera);
                        in.mFeatures.push_back(f);
                    }
                    break;
                }
                case ReliveTypes::eBirdPortalExit:
                {
                    const auto& t = static_cast<const relive::Path_BirdPortalExit&>(tlv);
                    NavFeature f = MakeFeature(FeatureKind::ePortalExit, tlv, camera, id);
                    f.mHalf = t.mScale == relive::reliveScale::eHalf;
                    in.mFeatures.push_back(f);
                    break;
                }
                case ReliveTypes::eWellLocal:
                {
                    const auto& t = static_cast<const relive::Path_WellLocal&>(tlv);
                    NavFeature f = MakeFeature(FeatureKind::eWellLocal, tlv, camera, id);
                    f.mHalf = t.mScale == relive::reliveScale::eHalf;
                    f.mB = t.mOtherWellId;
                    // (switch 1 is always on: such a well only ever throws to its "on" place)
                    f.mC = t.mSwitchId == 1 ? t.mOnDestX : t.mOffDestX;
                    f.mD = t.mSwitchId == 1 ? t.mOnDestY : t.mOffDestY;
                    f.mOnC = t.mOnDestX;
                    f.mOnD = t.mOnDestY;
                    f.mSwitch = t.mSwitchId;
                    in.mFeatures.push_back(f);
                    break;
                }
                case ReliveTypes::eTeleporter:
                {
                    const auto& t = static_cast<const relive::Path_Teleporter&>(tlv);
                    NavFeature f = MakeFeature(FeatureKind::eTeleporter, tlv, camera, id);
                    f.mHalf = t.mScale == relive::reliveScale::eHalf;
                    f.mA = t.mTeleporterId;
                    f.mB = t.mOtherTeleporterId;
                    f.mSwitch = t.mSwitchId;
                    f.mDestCamera = t.mDestCamera;
                    in.mFeatures.push_back(f);
                    break;
                }
                case ReliveTypes::eWellExpress:
                {
                    const auto& t = static_cast<const relive::Path_WellExpress&>(tlv);
                    NavFeature f = MakeFeature(FeatureKind::eWellExpress, tlv, camera, id);
                    f.mHalf = t.mScale == relive::reliveScale::eHalf;
                    f.mA = t.mOffOtherWellId;
                    f.mB = t.mOtherWellId;
                    f.mSwitch = t.mSwitchId;
                    // (shot out of it, he is thrown at its own exit point)
                    f.mC = t.mExitX;
                    f.mD = t.mExitY;
                    setDest(f, t.mOffDestLevel, t.mOffDestPath, t.mOffDestCamera);
                    f.mOnA = t.mOnOtherWellId;
                    f.mOnDestCamera = t.mOnDestCamera;
                    if (static_cast<s32>(t.mOnDestLevel) != level || t.mOnDestPath != pathId)
                    {
                        f.mOnDestLevel = static_cast<s32>(t.mOnDestLevel);
                        f.mOnDestPath = t.mOnDestPath;
                    }
                    in.mFeatures.push_back(f);
                    break;
                }
                case ReliveTypes::eDeathDrop:
                    in.mFeatures.push_back(MakeFeature(FeatureKind::eDeathDrop, tlv, camera, id));
                    break;
                case ReliveTypes::eSoftLanding:
                {
                    NavFeature f = MakeFeature(FeatureKind::eSoftLanding, tlv, camera, id);
                    f.mAnyScale = true;
                    in.mFeatures.push_back(f);
                    break;
                }
                case ReliveTypes::eMine:
                {
                    NavFeature f = MakeFeature(FeatureKind::eHazard, tlv, camera, id);
                    f.mHalf = static_cast<const relive::Path_Mine&>(tlv).mScale == relive::reliveScale::eHalf;
                    // (a rock gets rid of it)
                    f.mSwitch = World::HazardSwitch(id);
                    in.mFeatures.push_back(f);
                    break;
                }
                case ReliveTypes::eDrill:
                {
                    NavFeature f = MakeFeature(FeatureKind::eSweep, tlv, camera, id);
                    f.mHalf = static_cast<const relive::Path_Drill&>(tlv).mScale == relive::reliveScale::eHalf;
                    in.mFeatures.push_back(f);
                    break;
                }
                case ReliveTypes::eUXB:
                {
                    NavFeature f = MakeFeature(FeatureKind::eHazard, tlv, camera, id);
                    f.mHalf = static_cast<const relive::Path_UXB&>(tlv).mScale == relive::reliveScale::eHalf;
                    in.mFeatures.push_back(f);
                    break;
                }
                case ReliveTypes::eFootSwitch:
                {
                    if (GetGameType() == GameType::eAe)
                    {
                        const auto& t = static_cast<const relive::Path_FootSwitch&>(tlv);
                        NavFeature f = MakeFeature(FeatureKind::ePlate, tlv, camera, id);
                        f.mHalf = t.mScale == relive::reliveScale::eHalf;
                        f.mSwitch = t.mSwitchId;
                        f.mA = OpOf(t.mAction);
                        in.mFeatures.push_back(f);
                    }
                    break;
                }
                case ReliveTypes::eSlamDoor:
                {
                    const auto& t = static_cast<const relive::Path_SlamDoor&>(tlv);
                    NavFeature f = MakeFeature(FeatureKind::eBarrier, tlv, camera, id);
                    f.mHalf = t.mScale == relive::reliveScale::eHalf;
                    f.mSwitch = t.mSwitchId;
                    f.mStartClosed = t.mStartClosed;
                    f.mSolid = true;
                    in.mFeatures.push_back(f);
                    break;
                }
                case ReliveTypes::eElectricWall:
                {
                    const auto& t = static_cast<const relive::Path_ElectricWall&>(tlv);
                    NavFeature f = MakeFeature(FeatureKind::eBarrier, tlv, camera, id);
                    f.mHalf = t.mScale == relive::reliveScale::eHalf;
                    f.mSwitch = t.mSwitchId;
                    f.mStartClosed = t.mStartState == relive::Path_ElectricWall::eOn;
                    in.mFeatures.push_back(f);
                    break;
                }
                case ReliveTypes::eInvisibleZone:
                {
                    NavFeature f = MakeFeature(FeatureKind::eInvisibleZone, tlv, camera, id);
                    f.mAnyScale = true;
                    in.mFeatures.push_back(f);
                    break;
                }
                case ReliveTypes::eLiftPoint:
                {
                    NavFeature f = MakeFeature(FeatureKind::eLiftStop, tlv, camera, id);
                    f.mHalf = static_cast<const relive::Path_LiftPoint&>(tlv).mScale == relive::reliveScale::eHalf;
                    // (where the lift is to start with)
                    f.mA = static_cast<const relive::Path_LiftPoint&>(tlv).mIsStartPoint ? 1 : 0;
                    f.mB = static_cast<const relive::Path_LiftPoint&>(tlv).mLiftPointId;
                    in.mFeatures.push_back(f);
                    break;
                }
                case ReliveTypes::eLiftMover:
                {
                    const auto& t = static_cast<const relive::Path_LiftMover&>(tlv);
                    movers.push_back({t.mLiftMoverSwitchId, t.mTargetLiftPointId, t.mMoveDirection == relive::Path_LiftMover::YDirection::eUp ? 1 : 0});
                    break;
                }
                case ReliveTypes::eTrapDoor:
                {
                    const auto& t = static_cast<const relive::Path_TrapDoor&>(tlv);
                    NavFeature f = MakeFeature(FeatureKind::eTrapDoor, tlv, camera, id);
                    f.mHalf = t.mScale == relive::reliveScale::eHalf;
                    f.mSwitch = t.mSwitchId;
                    f.mStartClosed = t.mStartState == relive::Path_TrapDoor::StartState::eClosed;
                    in.mFeatures.push_back(f);
                    break;
                }
                case ReliveTypes::eLevelLoader:
                {
                    const auto& t = static_cast<const relive::Path_LevelLoader&>(tlv);
                    NavFeature f = MakeFeature(FeatureKind::eLevelLoader, tlv, camera, id);
                    f.mAnyScale = true;
                    f.mSwitch = t.mSwitchId;
                    setDest(f, t.mDestLevel, t.mDestPath, t.mDestCamera);
                    in.mFeatures.push_back(f);
                    break;
                }
                case ReliveTypes::ePathTransition:
                {
                    const auto& t = static_cast<const relive::Path_PathTransition&>(tlv);
                    NavFeature f = MakeFeature(FeatureKind::ePathExit, tlv, camera, id);
                    f.mAnyScale = true;
                    setDest(f, t.mNextLevel, t.mNextPath, t.mNextCamera);
                    in.mFeatures.push_back(f);
                    break;
                }
                default:
                    break;
            }
        }
    }
    // A lift mover sends its lift all the way up or down once its switch is on: that stop then has the lift
    for (const std::array<s32, 3>& mover : movers)
    {
        NavFeature* end = nullptr;
        for (NavFeature& f : in.mFeatures)
        {
            if (f.mKind == FeatureKind::eLiftStop && f.mB == mover[1] && (!end || (mover[2] ? f.mY1 < end->mY1 : f.mY1 > end->mY1)))
            {
                end = &f;
            }
        }
        if (end)
        {
            end->mSwitch = mover[0];
        }
    }
    return in;
}


static void BuildEntities(BinaryPath& path, World& world)
{
    world.mEntities.clear();
    world.mTlvs.clear();
    world.mPath = &path;
    world.mInvisEver = false;
    world.mFleechKilled.clear();
    world.mScrabsKilled.clear();
    world.mLocksSlapped.clear();
    world.mScrabParked.clear();
    world.mScrabBoundsLeft.clear();
    world.mScrabBoundsRight.clear();
    world.mSlogSpawners.clear();
    for (auto& cam : path.GetCameras())
    {
        if (!cam)
        {
            continue;
        }
        const s32 camera = std::atoi(cam->mName.c_str());
        u32 idx = 0;
        for (auto& upTlv : cam->mTlvs.mTlvs)
        {
            const relive::Path_TLV& tlv = *upTlv;
            Entity e;
            e.mId = static_cast<u32>(camera) * 1000 + idx++;
            e.mCamera = camera;
            e.mX1 = tlv.mTopLeftX;
            e.mY1 = tlv.mTopLeftY;
            e.mX2 = tlv.mBottomRightX;
            e.mY2 = tlv.mBottomRightY;
            bool keep = true;
            switch (tlv.mTlvType)
            {
                case ReliveTypes::eLever:
                {
                    const auto& t = static_cast<const relive::Path_Lever&>(tlv);
                    e.mKind = EntityKind::eLever;
                    e.mHalf = t.mScale == relive::reliveScale::eHalf;
                    e.mSwitch = t.mSwitchId;
                    e.mOp = OpOf(t.mAction);
                    break;
                }
                case ReliveTypes::eMine:
                    e.mKind = EntityKind::eMine;
                    e.mHalf = static_cast<const relive::Path_Mine&>(tlv).mScale == relive::reliveScale::eHalf;
                    e.mSwitch = World::HazardSwitch(e.mId);
                    break;
                case ReliveTypes::eDrill:
                {
                    const auto& t = static_cast<const relive::Path_Drill&>(tlv);
                    e.mKind = EntityKind::eDrill;
                    e.mHalf = t.mScale == relive::reliveScale::eHalf;
                    e.mSwitch = t.mSwitchId;
                    e.mInputs[0] = t.mDrillBehavior == relive::Path_Drill::DrillBehavior::eToggle ? 1 : (t.mDrillBehavior == relive::Path_Drill::DrillBehavior::eUse ? 2 : 0);
                    e.mInputs[1] = t.mStartStateOn ? 1 : 0;
                    // (250 stands for a fifth of a pixel a frame, which the game takes as stopped)
                    e.mInputs[2] = t.mOffSpeed > 0 && t.mOffSpeed != 250 ? 1 : 0;
                    break;
                }
                case ReliveTypes::eMineCar:
                    e.mKind = EntityKind::eMineCar;
                    keep = GetGameType() == GameType::eAe;
                    e.mHalf = static_cast<const relive::Path_MineCar&>(tlv).mScale == relive::reliveScale::eHalf;
                    break;
                case ReliveTypes::eRockSack:
                    e.mKind = EntityKind::eRockSack;
                    // (AO's give rocks another way, not done yet)
                    keep = GetGameType() == GameType::eAe;
                    e.mHalf = static_cast<const relive::Path_RockSack&>(tlv).mScale == relive::reliveScale::eHalf;
                    // How many rocks it gives
                    e.mInputs[0] = static_cast<const relive::Path_RockSack&>(tlv).mRockAmount;
                    break;
                case ReliveTypes::eMeatSack:
                    // (a sack like the other: what it gives is meat)
                    e.mKind = EntityKind::eRockSack;
                    keep = GetGameType() == GameType::eAe;
                    e.mHalf = static_cast<const relive::Path_MeatSack&>(tlv).mScale == relive::reliveScale::eHalf;
                    e.mInputs[0] = static_cast<const relive::Path_MeatSack&>(tlv).mMeatAmount;
                    e.mInputs[1] = 1;
                    break;
                case ReliveTypes::ePullRingRope:
                {
                    const auto& t = static_cast<const relive::Path_PullRingRope&>(tlv);
                    e.mKind = EntityKind::ePullRing;
                    e.mHalf = t.mScale == relive::reliveScale::eHalf;
                    e.mSwitch = t.mSwitchId;
                    // (how far under its TLV's top the ring hangs: AE adds the rope's length)
                    e.mInputs[0] = 24 + t.mRopeLength;
                    e.mOp = OpOf(t.mAction);
                    break;
                }
                case ReliveTypes::eWorkWheel:
                {
                    const auto& t = static_cast<const relive::Path_WorkWheel&>(tlv);
                    e.mKind = EntityKind::eWorkWheel;
                    e.mHalf = t.mScale == relive::reliveScale::eHalf;
                    e.mSwitch = t.mSwitchId;
                    e.mHoldOnly = t.mTurnOffWhenStopped;
                    e.mInputs[0] = t.mActivationTime;
                    break;
                }
                case ReliveTypes::eMudokon:
                {
                    const auto& t = static_cast<const relive::Path_Mudokon&>(tlv);
                    e.mKind = EntityKind::eMudokon;
                    e.mHalf = t.mScale == relive::reliveScale::eHalf;
                    e.mSwitch = t.mRescueSwitchId;
                    e.mState = static_cast<s32>(t.mEmotion);
                    e.mBlind = t.mBlind;
                    break;
                }
                case ReliveTypes::eBirdPortal:
                {
                    const auto& t = static_cast<const relive::Path_BirdPortal&>(tlv);
                    e.mKind = EntityKind::eBirdPortal;
                    e.mHalf = t.mScale == relive::reliveScale::eHalf;
                    e.mRescuePortal = t.mPortalType == relive::Path_BirdPortal::PortalType::eWorker || t.mPortalType == relive::Path_BirdPortal::PortalType::eShrykull;
                    // It is only there once the first is on, and gone for good once the second is
                    e.mInputs[0] = t.mCreatePortalSwitchId;
                    e.mInputs[1] = t.mDeletePortalSwitchId;
                    break;
                }
                case ReliveTypes::eDoor:
                {
                    // A tasks door: "all my hubs are on" as a switch of its own
                    const auto& t = static_cast<const relive::Path_Door&>(tlv);
                    keep = GetGameType() == GameType::eAe && (t.mDoorType == relive::Path_Door::eTasksDoor || t.mDoorType == relive::Path_Door::eTasksDoorWithSecretMusic);
                    e.mKind = EntityKind::eSwitchLogic;
                    e.mSwitch = World::DoorSwitch(e.mId);
                    const s16 hubs[8] = {t.mHub1, t.mHub2, t.mHub3, t.mHub4, t.mHub5, t.mHub6, t.mHub7, t.mHub8};
                    s32 count = 0;
                    for (s32 i = 0; i < 8 && count < 6; i++)
                    {
                        if (hubs[i] > 1)
                        {
                            e.mInputs[count++] = hubs[i];
                        }
                    }
                    break;
                }
                case ReliveTypes::eMultiSwitchController:
                {
                    const auto& t = static_cast<const relive::Path_MultiSwitchController&>(tlv);
                    e.mKind = EntityKind::eSwitchLogic;
                    e.mSwitch = t.mOutputSwitchId;
                    e.mOp = OpOf(t.mAction);
                    const u16 inputs[6] = {t.mInputSwitchId1, t.mInputSwitchId2, t.mInputSwitchId3, t.mInputSwitchId4, t.mInputSwitchId5, t.mInputSwitchId6};
                    for (s32 i = 0; i < 6; i++)
                    {
                        e.mInputs[i] = inputs[i];
                    }
                    break;
                }
                case ReliveTypes::eWheelSyncer:
                {
                    const auto& t = static_cast<const relive::Path_WheelSyncer&>(tlv);
                    keep = t.mOutputRequirement == relive::Path_WheelSyncer::OutputRequirement::eAllOn;
                    e.mKind = EntityKind::eSwitchLogic;
                    e.mSwitch = t.mOutputSwitchId;
                    const s16 inputs[6] = {t.mInputSwitchId1, t.mInputSwitchId2, t.mInputSwitchId3, t.mInputSwitchId4, t.mInputSwitchId5, t.mInputSwitchId6};
                    for (s32 i = 0; i < 6; i++)
                    {
                        e.mInputs[i] = inputs[i];
                    }
                    break;
                }
                case ReliveTypes::eTimerTrigger:
                {
                    // Its outputs are toggled when its input changes (after a delay) and back when it changes
                    // back: each is a switch that comes on with the input (the Mudanchee vault's path 4, where
                    // one lever opens two gates through it)
                    const auto& t = static_cast<const relive::Path_TimerTrigger&>(tlv);
                    e.mKind = EntityKind::eSwitchLogic;
                    e.mInputs[0] = t.mInputSwitchId;
                    const s16 outs[4] = {t.mOutputSwitchId1, t.mOutputSwitchId2, t.mOutputSwitchId3, t.mOutputSwitchId4};
                    keep = false;
                    for (s32 i = 0; i < 4; i++)
                    {
                        if (outs[i] > 1 && t.mInputSwitchId > 1)
                        {
                            Entity out = e;
                            out.mId = e.mId ^ (static_cast<u32>(i) << 28);
                            out.mSwitch = outs[i];
                            // (it goes off again with its input: see Domain::Build)
                            out.mHoldOnly = true;
                            world.mEntities.push_back(out);
                            world.mTlvs.push_back(&tlv);
                        }
                    }
                    break;
                }
                case ReliveTypes::eLaughingGas:
                    e.mKind = EntityKind::eLaughingGas;
                    e.mSwitch = static_cast<const relive::Path_LaughingGas&>(tlv).mLaughingGasSwitchId;
                    break;
                case ReliveTypes::eFootSwitch:
                {
                    const auto& t = static_cast<const relive::Path_FootSwitch&>(tlv);
                    e.mKind = EntityKind::eFootSwitch;
                    keep = GetGameType() == GameType::eAe;
                    e.mHalf = t.mScale == relive::reliveScale::eHalf;
                    e.mSwitch = t.mSwitchId;
                    e.mOp = OpOf(t.mAction);
                    break;
                }
                case ReliveTypes::eSlogSpawner:
                    world.mSlogSpawners.push_back({(tlv.mTopLeftX + tlv.mBottomRightX) / 2, tlv.mBottomRightY, static_cast<const relive::Path_SlogSpawner&>(tlv).mSpawnerSwitchId, static_cast<const relive::Path_SlogSpawner&>(tlv).mMaxSlogs});
                    break;
                case ReliveTypes::eScrabBoundLeft:
                    world.mScrabBoundsLeft.push_back({(tlv.mTopLeftX + tlv.mBottomRightX) / 2, tlv.mBottomRightY});
                    break;
                case ReliveTypes::eScrabBoundRight:
                    world.mScrabBoundsRight.push_back({(tlv.mTopLeftX + tlv.mBottomRightX) / 2, tlv.mBottomRightY});
                    break;
                case ReliveTypes::eScrab:
                    e.mKind = EntityKind::eScrab;
                    keep = GetGameType() == GameType::eAe;
                    e.mHalf = static_cast<const relive::Path_Scrab&>(tlv).mScale == relive::reliveScale::eHalf;
                    // (how long it waits before it goes for what it has caught up with)
                    e.mInputs[0] = static_cast<const relive::Path_Scrab&>(tlv).mAttackDelay;
                    // (and how long at least it stands at an end of its beat, by the way it then faces: left, right)
                    e.mInputs[1] = static_cast<const relive::Path_Scrab&>(tlv).mPauseLeftMin;
                    e.mInputs[2] = static_cast<const relive::Path_Scrab&>(tlv).mPauseRightMin;
                    break;
                case ReliveTypes::eParamite:
                {
                    const auto& t = static_cast<const relive::Path_Paramite&>(tlv);
                    e.mKind = EntityKind::eParamite;
                    keep = GetGameType() == GameType::eAe;
                    e.mHalf = t.mScale == relive::reliveScale::eHalf;
                    e.mInputs[0] = static_cast<s32>(t.mEntranceType);
                    e.mInputs[1] = t.mSurpriseWebSwitchId;
                    e.mInputs[2] = t.mHissBeforeAttack ? 1 : 0;
                    break;
                }
                case ReliveTypes::eFleech:
                {
                    const auto& t = static_cast<const relive::Path_Fleech&>(tlv);
                    e.mKind = EntityKind::eFleech;
                    e.mHalf = t.mScale == relive::reliveScale::eHalf;
                    e.mSwitch = t.mWakeUpSwitchId;
                    e.mInputs[0] = t.mAsleep ? 1 : 0;
                    e.mInputs[1] = t.mHanging ? 1 : 0;
                    break;
                }
                case ReliveTypes::eSlapLock:
                {
                    const auto& t = static_cast<const relive::Path_SlapLock&>(tlv);
                    e.mKind = EntityKind::eSlapLock;
                    e.mHalf = t.mScale == relive::reliveScale::eHalf;
                    e.mSwitch = t.mHasGhost ? t.mTargetTombSwitchId2 : 0;
                    // (one whose ghost goes nowhere: the switch the slap sets is all it does. The Mudanchee
                    // trial's last, which turns off the electric walls.)
                    if (e.mSwitch <= 1 && t.mSlapOutputSwitchId > 1 && !t.mGiveInvisibilityPowerup)
                    {
                        e.mSwitch = t.mSlapOutputSwitchId;
                    }
                    e.mInputs[0] = t.mGiveInvisibilityPowerup ? t.mInvisibilityDuration : 0;
                    // (the switch the slap itself sets: a trap door under him, in the Mudanchee vault's path 5)
                    e.mInputs[1] = t.mSlapOutputSwitchId;
                    break;
                }
                case ReliveTypes::eHandStone:
                    e.mKind = EntityKind::eHandStone;
                    e.mHalf = static_cast<const relive::Path_HandStone&>(tlv).mScale == relive::reliveScale::eHalf;
                    e.mSwitch = static_cast<const relive::Path_HandStone&>(tlv).mTriggerSwitchId;
                    break;
                case ReliveTypes::eSlig:
                {
                    const auto& t = static_cast<const relive::Path_Slig&>(tlv);
                    e.mKind = EntityKind::eSlig;
                    e.mHalf = t.mScale == relive::reliveScale::eHalf;
                    // mState 1: starts asleep. mOp 1: starts facing left.
                    e.mState = t.mStartState == relive::Path_Slig::StartState::Sleeping ? 1 : 0;
                    e.mOp = t.mFacing == relive::reliveXDirection::eLeft ? 1 : 0;
                    break;
                }
                case ReliveTypes::eTeleporter:
                {
                    const auto& t = static_cast<const relive::Path_Teleporter&>(tlv);
                    e.mKind = EntityKind::eTeleporter;
                    e.mHalf = t.mScale == relive::reliveScale::eHalf;
                    e.mSwitch = t.mSwitchId;
                    e.mInputs[0] = t.mDestCamera;
                    e.mInputs[1] = t.mTeleporterId;
                    e.mInputs[2] = t.mOtherTeleporterId;
                    break;
                }
                case ReliveTypes::eSligSpawner:
                {
                    // Makes its Slig when Abe comes to its screen with the switch on, and again only once that
                    // one is dead and the switch is set anew (SligSpawner.cpp)
                    const auto& t = static_cast<const relive::Path_Slig&>(tlv);
                    e.mKind = EntityKind::eSlig;
                    e.mHalf = t.mScale == relive::reliveScale::eHalf;
                    e.mState = t.mStartState == relive::Path_Slig::StartState::Sleeping ? 1 : 0;
                    e.mOp = t.mFacing == relive::reliveXDirection::eLeft ? 1 : 0;
                    e.mSwitch = t.mSligSpawnerSwitchId;
                    break;
                }
                case ReliveTypes::eSecurityOrb:
                    e.mKind = EntityKind::eSecurityOrb;
                    break;
                default:
                    keep = false;
                    break;
            }
            if (keep)
            {
                world.mEntities.push_back(e);
                world.mTlvs.push_back(&tlv);
            }
        }
    }
}

bool GameView::ReadPath(BaseMap& map, s32 pathId, NavInput& input, World& world)
{
    BinaryPath* pPath = map.GetPathResourceBlockPtr(static_cast<u32>(pathId));
    if (!pPath)
    {
        return false;
    }
    input = BuildNavInput(*pPath, static_cast<s32>(map.mCurrentLevel), pathId);
    BuildEntities(*pPath, world);
    for (std::size_t i = 0; i < world.mEntities.size(); i++)
    {
        const relive::Path_TLV* pTlv = static_cast<const relive::Path_TLV*>(world.mTlvs[i]);
        world.mEntities[i].mGone = pTlv->mTlvFlags.Get(relive::eBit2_Destroyed) && !pTlv->mTlvFlags.Get(relive::eBit1_Created);
    }
    return true;
}

void GameView::ReadWorld(BaseMap& map, World& world)
{
    BinaryPath* pPath = map.GetPathResourceBlockPtr(static_cast<u32>(map.mCurrentPath));
    if (!pPath)
    {
        return;
    }
    if (world.mPath != pPath)
    {
        BuildEntities(*pPath, world);
    }

    const bool isAe = GetGameType() == GameType::eAe;
    for (s32 i = 0; i < 256; i++)
    {
        world.mSwitches[i] = gSwitchStates.mData[i];
    }
    world.mRescued = isAe ? gRescuedMudokons : AO::gRescuedMudokons;
    world.mKilled = isAe ? gKilledMudokons : AO::gKilledMudokons;

    for (std::size_t i = 0; i < world.mEntities.size(); i++)
    {
        Entity& e = world.mEntities[i];
        const relive::Path_TLV* pTlv = static_cast<const relive::Path_TLV*>(world.mTlvs[i]);
        // Both are set while its object is about: gone for good is destroyed and not there
        e.mGone = pTlv->mTlvFlags.Get(relive::eBit2_Destroyed) && !pTlv->mTlvFlags.Get(relive::eBit1_Created);
        e.mLive = false;
        e.mFollowing = false;
        e.mMood = MudMood::eNormal;
    }

    world.mLifts.clear();
    for (s32 i = 0; i < PlatformBase::Platforms().Size(); i++)
    {
        BaseGameObject* pObj = PlatformBase::Platforms().ItemAt(i);
        if (pObj && !pObj->GetDead() && pObj->Type() == ReliveTypes::eLiftPoint)
        {
            const PathLine* pLine = static_cast<PlatformBase*>(pObj)->GetPlatformLine();
            if (pLine)
            {
                world.mLifts.push_back({pLine->mRect.x, pLine->mRect.w, pLine->mRect.y});
                bool had = false;
                for (LiftAt& seen : world.mLiftsSeen)
                {
                    if (seen.mX1 == pLine->mRect.x)
                    {
                        seen = world.mLifts.back();
                        had = true;
                    }
                }
                if (!had)
                {
                    world.mLiftsSeen.push_back(world.mLifts.back());
                }
            }
        }
    }

    world.mSligs.clear();
    for (s32 i = 0; i < gBaseAliveGameObjects->Size(); i++)
    {
        BaseAliveGameObject* pObj = gBaseAliveGameObjects->ItemAt(i);
        if (pObj && !pObj->GetDead() && pObj->Type() == ReliveTypes::eSlig && pObj->mHealth > FP_FromInteger(0))
        {
            SligAt slig;
            slig.mX = FP_GetExponent(pObj->mXPos);
            slig.mY = FP_GetExponent(pObj->mYPos);
            slig.mFacingLeft = pObj->GetAnimation().GetFlipX();
            slig.mHalf = pObj->GetSpriteScale() != FP_FromInteger(1);
            slig.mPossessed = pObj->GetPossessed();
            const s16 motion = pObj->VGetMotion(eMotionType::eCurrentMotion);
            // Motion_32_Sleeping in AE, Motion_33_Sleeping in AO
            slig.mAsleep = motion == (isAe ? 32 : 33);
            slig.mMotion = motion;
            if (isAe)
            {
                s32 wakeCells = 0;
                static_cast<::Slig*>(pObj)->Beat(slig.mBeatMin, slig.mBeatMax, wakeCells, slig.mSightDelay);
                slig.mWake = wakeCells * (slig.mHalf ? 13 : 25);
            }
            world.mSligs.push_back(slig);
        }
    }

    world.mInvisCharge = isAe && gAbe && gAbe->mHaveInvisibility != 0;
    world.mInvisible = isAe && gAbe && gAbe->GetInvisible();
    {
        const s32 timer = world.mInvisible ? gAbe->InvisibilityTimer() - static_cast<s32>(sGnFrame) : 0;
        if (!world.mInvisible || timer > 0)
        {
            world.mInvisFadeAt = 0;
            world.mInvisLeft = world.mInvisible ? timer + 140 : 0;
        }
        else
        {
            if (world.mInvisFadeAt == 0)
            {
                world.mInvisFadeAt = sGnFrame;
            }
            world.mInvisLeft = std::max(0, 140 - static_cast<s32>(sGnFrame - world.mInvisFadeAt));
        }
    }
    world.mInvisEver = world.mInvisEver || world.mInvisCharge || world.mInvisible;
    world.mScrabs.clear();
    world.mParamites.clear();
    const std::vector<SlurgAt> slurgsBefore = world.mSlurgs;
    world.mSlurgs.clear();
    world.mFleechesAwake.clear();
    world.mFleechesAsleep.clear();
    world.mLocksIntact.clear();
    for (s32 i = 0; isAe && i < gBaseAliveGameObjects->Size(); i++)
    {
        BaseAliveGameObject* pObj = gBaseAliveGameObjects->ItemAt(i);
        if (pObj && !pObj->GetDead() && pObj->Type() == ReliveTypes::eSlapLock && static_cast<const SlapLock*>(pObj)->Intact())
        {
            // (which of the path's it is: by where it stands, in the middle of its TLV on its lower edge)
            for (const Entity& e : world.mEntities)
            {
                if (e.mKind == EntityKind::eSlapLock && std::abs(e.CentreX() - FP_GetExponent(pObj->mXPos)) <= 2 && std::abs(e.mY2 - FP_GetExponent(pObj->mYPos)) <= 2)
                {
                    world.mLocksIntact.insert(e.mId);
                }
            }
            continue;
        }
        // Which of the path's Scrabs it is: by its TLV, since one that is chased off its floor is no longer where
        // the map put it
        u32 scrabId = 0;
        if (pObj && pObj->Type() == ReliveTypes::eScrab)
        {
            for (std::size_t k = 0; k < world.mEntities.size(); k++)
            {
                if (world.mEntities[k].mKind == EntityKind::eScrab && static_cast<const relive::Path_TLV*>(world.mTlvs[k])->mId == pObj->mBaseGameObjectTlvInfo)
                {
                    scrabId = world.mEntities[k].mId;
                }
            }
            // (one that lost a fight lies there a while before its TLV says it is gone)
            if (scrabId != 0 && pObj->mHealth <= FP_FromInteger(0))
            {
                world.mScrabsKilled.insert(scrabId);
            }
        }
        if (!pObj || pObj->GetDead() || pObj->mHealth <= FP_FromInteger(0))
        {
            continue;
        }
        RockAt at;
        at.mX = FP_GetExponent(pObj->mXPos);
        at.mY = FP_GetExponent(pObj->mYPos);
        at.mHalf = pObj->GetSpriteScale() != FP_FromInteger(1);
        if (pObj->Type() == ReliveTypes::eScrab)
        {
            ScrabAt scrab;
            scrab.mId = scrabId;
            scrab.mX = at.mX;
            scrab.mY = at.mY;
            scrab.mHalf = at.mHalf;
            scrab.mPossessed = pObj->GetPossessed();
            scrab.mMotion = pObj->VGetMotion(eMotionType::eCurrentMotion);
            scrab.mFacesLeft = pObj->GetAnimation().GetFlipX();
            world.mScrabs.push_back(scrab);
        }
        else if (pObj->Type() == ReliveTypes::eParamite)
        {
            const ::Paramite* game = static_cast<const ::Paramite*>(pObj);
            ParamiteAt paramite;
            for (std::size_t k = 0; k < world.mEntities.size(); k++)
            {
                if (world.mEntities[k].mKind == EntityKind::eParamite && static_cast<const relive::Path_TLV*>(world.mTlvs[k])->mId == pObj->mBaseGameObjectTlvInfo)
                {
                    paramite.mId = world.mEntities[k].mId;
                }
            }
            paramite.mX = at.mX;
            paramite.mY = at.mY;
            paramite.mHalf = at.mHalf;
            paramite.mPossessed = pObj->GetPossessed();
            paramite.mMotion = pObj->VGetMotion(eMotionType::eCurrentMotion);
            paramite.mFacesLeft = pObj->GetAnimation().GetFlipX();
            paramite.mBrain = game->BrainIndex();
            paramite.mSub = game->BrainSubState();
            world.mParamites.push_back(paramite);
        }
        else if (pObj->Type() == ReliveTypes::eSlurg)
        {
            SlurgAt slurg;
            slurg.mX = at.mX;
            slurg.mY = at.mY;
            slurg.mHalf = at.mHalf;
            const PSX_RECT box = pObj->VGetBoundingRect();
            slurg.mX1 = box.x;
            slurg.mX2 = box.w;
            // Which way it goes: from where it was the frame before (it moves a pixel every other frame)
            for (const SlurgAt& was : slurgsBefore)
            {
                if (was.mHalf == slurg.mHalf && was.mY == slurg.mY && std::abs(was.mX - slurg.mX) <= 2)
                {
                    slurg.mDir = slurg.mX > was.mX ? 1 : (slurg.mX < was.mX ? -1 : was.mDir);
                    break;
                }
            }
            world.mSlurgs.push_back(slurg);
        }
        else if (pObj->Type() == ReliveTypes::eFleech)
        {
            // (Fleech.hpp: motion 0 is asleep on the floor, 17 asleep hanging by its tongue)
            const s16 motion = pObj->VGetMotion(eMotionType::eCurrentMotion);
            if (motion != 0 && motion != 17)
            {
                // (11 raises its head to the hoist, 12 climbs)
                at.mClimbing = motion == 11 || motion == 12;
                at.mChasing = isAe && static_cast<::Fleech*>(pObj)->IsChasingAbe();
                world.mFleechesAwake.push_back(at);
            }
            else
            {
                world.mFleechesAsleep.push_back(at);
            }
        }
    }

    // Bird portals that are open: one can be gone into
    world.mPortalsOpen.clear();
    for (s32 i = 0; isAe && i < gBaseGameObjects->Size(); i++)
    {
        BaseGameObject* pObj = gBaseGameObjects->ItemAt(i);
        if (pObj && !pObj->GetDead() && pObj->Type() == ReliveTypes::eBirdPortal)
        {
            IBirdPortal* pPortal = static_cast<IBirdPortal*>(pObj);
            if (pPortal->IsActivePortalState())
            {
                RockAt at;
                at.mX = FP_GetExponent(pPortal->mXPos);
                at.mY = FP_GetExponent(pPortal->mYPos);
                world.mPortalsOpen.push_back(at);
            }
        }
    }

    world.mFliers.clear();
    for (s32 i = 0; isAe && i < gBaseAliveGameObjects->Size(); i++)
    {
        BaseAliveGameObject* pObj = gBaseAliveGameObjects->ItemAt(i);
        if (pObj && !pObj->GetDead() && pObj->Type() == ReliveTypes::eFlyingSlig && pObj->mHealth > FP_FromInteger(0) && !pObj->GetPossessed())
        {
            FlierAt flier;
            flier.mX = FP_GetExponent(pObj->mXPos);
            flier.mY = FP_GetExponent(pObj->mYPos);
            flier.mVelX = pObj->mVelX.fpValue / 256;
            flier.mVelY = pObj->mVelY.fpValue / 256;
            flier.mHalf = pObj->GetSpriteScale() != FP_FromInteger(1);
            world.mFliers.push_back(flier);
        }
    }

    for (s32 i = 0; isAe && i < gBaseAliveGameObjects->Size(); i++)
    {
        BaseAliveGameObject* pObj = gBaseAliveGameObjects->ItemAt(i);
        if (!pObj || pObj->GetDead() || pObj->Type() != ReliveTypes::eMineCar)
        {
            continue;
        }
        for (Entity& e : world.mEntities)
        {
            if (e.mKind == EntityKind::eMineCar)
            {
                e.mLive = true;
                e.mLiveX = FP_GetExponent(pObj->mXPos);
                e.mLiveY = FP_GetExponent(pObj->mYPos);
                e.mState = sControlledCharacter == pObj ? 1 : 0;
            }
        }
    }

    world.mRocks = isAe && gAbe ? gAbe->mBaseThrowableCount : 0;
    world.mRocksDown.clear();
    world.mRocksFlying.clear();
    world.mFallers.clear();
    for (s32 i = 0; isAe && i < gBaseAliveGameObjects->Size(); i++)
    {
        BaseAliveGameObject* pObj = gBaseAliveGameObjects->ItemAt(i);
        if (pObj && !pObj->GetDead() && pObj->Type() == ReliveTypes::eRockSpawner)
        {
            RockAt faller;
            faller.mX = FP_GetExponent(pObj->mXPos);
            faller.mY = FP_GetExponent(pObj->mYPos);
            faller.mHalf = pObj->GetSpriteScale() != FP_FromInteger(1);
            faller.mChasing = pObj->mVelY > FP_FromInteger(0);
            world.mFallers.push_back(faller);
        }
        if (pObj && !pObj->GetDead() && (pObj->Type() == ReliveTypes::eRock || pObj->Type() == ReliveTypes::eMeat))
        {
            RockAt rock;
            rock.mX = FP_GetExponent(pObj->mXPos);
            rock.mY = FP_GetExponent(pObj->mYPos);
            rock.mHalf = pObj->GetSpriteScale() != FP_FromInteger(1);
            // (one that has come to rest can be picked up)
            if (pObj->GetInteractive())
            {
                world.mRocksDown.push_back(rock);
            }
            else if (pObj->GetAnimation().GetRender() && (pObj->mVelX != FP_FromInteger(0) || pObj->mVelY != FP_FromInteger(0)))
            {
                world.mRocksFlying.push_back(rock);
            }
        }
    }

    world.mUxbs.clear();
    for (s32 i = 0; isAe && i < gBaseAliveGameObjects->Size(); i++)
    {
        BaseAliveGameObject* pObj = gBaseAliveGameObjects->ItemAt(i);
        if (pObj && !pObj->GetDead() && pObj->Type() == ReliveTypes::eUXB)
        {
            const ::UXB* pUxb = static_cast<::UXB*>(pObj);
            UxbAt uxb;
            uxb.mX = FP_GetExponent(pObj->mXPos);
            uxb.mY = FP_GetExponent(pObj->mYPos);
            uxb.mHalf = pObj->GetSpriteScale() != FP_FromInteger(1);
            uxb.mGreen = pUxb->IsGreen();
            uxb.mOff = pUxb->IsOff();
            world.mUxbs.push_back(uxb);
        }
    }

    world.mBeams.clear();
    for (s32 i = 0; isAe && i < gBaseGameObjects->Size(); i++)
    {
        BaseGameObject* pObj = gBaseGameObjects->ItemAt(i);
        ::MotionDetector* pDetector = pObj && !pObj->GetDead() ? dynamic_cast<::MotionDetector*>(pObj) : nullptr;
        PSX_RECT laser = {};
        BeamAt beam;
        if (pDetector && pDetector->BeamNow(laser, beam.mMinX, beam.mMaxX, beam.mVel256, beam.mPause))
        {
            beam.mX1 = laser.x;
            beam.mY1 = laser.y;
            beam.mX2 = laser.w;
            beam.mY2 = laser.h;
            beam.mHalf = pDetector->GetSpriteScale() != FP_FromInteger(1);
            beam.mEndPause = ::MotionDetector::kEndPause + 1;
            world.mBeams.push_back(beam);
        }
    }

    world.mSweeps.clear();
    for (s32 i = 0; isAe && i < gBaseGameObjects->Size(); i++)
    {
        BaseGameObject* pObj = gBaseGameObjects->ItemAt(i);
        if (!pObj || pObj->GetDead() || pObj->Type() != ReliveTypes::eDrill)
        {
            continue;
        }
        ::Drill* pDrill = static_cast<::Drill*>(pObj);
        SweepAt sweep;
        for (std::size_t k = 0; k < world.mEntities.size(); k++)
        {
            if (world.mEntities[k].mKind == EntityKind::eDrill && pDrill->TlvId() == static_cast<const relive::Path_TLV*>(world.mTlvs[k])->mId)
            {
                sweep.mId = world.mEntities[k].mId;
                world.mEntities[k].mLive = true;
            }
        }
        const PSX_RECT rect = pDrill->KillRect();
        sweep.mX1 = rect.x;
        sweep.mY1 = rect.y;
        sweep.mX2 = rect.w;
        sweep.mY2 = rect.h;
        sweep.mHalf = pDrill->GetSpriteScale() != FP_FromInteger(1);
        sweep.mVertical = pDrill->Direction() == relive::Path_Drill::DrillDirection::eDown;
        sweep.mSign = pDrill->Direction() == relive::Path_Drill::DrillDirection::eRight ? 1 : -1;
        sweep.mOffset = pDrill->Offset().fpValue / 256;
        sweep.mTravel = pDrill->Travel() * 256;
        sweep.mSpeed = pDrill->StrokeSpeed().fpValue / 256;
        sweep.mPhase = pDrill->GetStroke() == ::Drill::Stroke::eParked ? 0 : (pDrill->GetStroke() == ::Drill::Stroke::eOut ? 1 : 2);
        sweep.mPause = pDrill->PauseLeft();
        sweep.mWillStart = pDrill->WillStart();
        world.mSweeps.push_back(sweep);
    }

    world.mSlogs.clear();
    // The switch that where Abe stands is about to set (an invisible switch sets its own a few frames after he
    // is in it): a Slog that this sets on him is as good as awake
    s32 soonOn = -1;
    if (isAe && gAbe)
    {
        const s16 ax = static_cast<s16>(FP_GetExponent(gAbe->mXPos));
        const s16 ay = static_cast<s16>(FP_GetExponent(gAbe->mYPos));
        if (relive::Path_TLV* pTlv = map.VTLV_Get_At_Of_Type(ax, ay, ax, ay, ReliveTypes::eInvisibleSwitch).GetTlv())
        {
            soonOn = static_cast<relive::Path_InvisibleSwitch*>(pTlv)->mSwitchId;
        }
    }
    world.mSoonOn = soonOn;
    for (s32 i = 0; i < gBaseAliveGameObjects->Size(); i++)
    {
        BaseAliveGameObject* pObj = gBaseAliveGameObjects->ItemAt(i);
        if (pObj && !pObj->GetDead() && pObj->Type() == ReliveTypes::eSlog && pObj->mHealth > FP_FromInteger(0))
        {
            SlogAt slog;
            slog.mX = FP_GetExponent(pObj->mXPos);
            slog.mY = FP_GetExponent(pObj->mYPos);
            // Motion_15_Sleeping in AE, Motion_16_Sleeping in AO
            slog.mAsleep = pObj->VGetMotion(eMotionType::eCurrentMotion) == (isAe ? 15 : 16);
            slog.mFacesLeft = pObj->GetAnimation().GetFlipX();
            slog.mMotion = pObj->VGetMotion(eMotionType::eCurrentMotion);
            const s32 anger = isAe ? static_cast<::Slog*>(pObj)->AngerSwitchId() : 0;
            slog.mWaking = slog.mAsleep && anger > 1 && (SwitchStates_Get(anger) != 0 || anger == soonOn);
            world.mSlogs.push_back(slog);
        }
    }

    // The Mudokons that are about, each given to its TLV's entity: by id, or failing that the nearest
    for (s32 i = 0; i < gBaseAliveGameObjects->Size(); i++)
    {
        BaseAliveGameObject* pObj = gBaseAliveGameObjects->ItemAt(i);
        if (!pObj || pObj->GetDead() || pObj->Type() != ReliveTypes::eMudokon)
        {
            continue;
        }
        const s32 x = FP_GetExponent(pObj->mXPos);
        const s32 y = FP_GetExponent(pObj->mYPos);
        Entity* match = nullptr;
        s32 bestDist = 0;
        for (std::size_t k = 0; k < world.mEntities.size(); k++)
        {
            Entity& e = world.mEntities[k];
            if (e.mKind != EntityKind::eMudokon || e.mLive)
            {
                continue;
            }
            const relive::Path_TLV* pTlv = static_cast<const relive::Path_TLV*>(world.mTlvs[k]);
            if (pObj->mBaseGameObjectTlvInfo == pTlv->mId)
            {
                match = &e;
                break;
            }
            const s32 dist = std::abs(e.CentreX() - x) + std::abs(e.mY2 - y);
            if (dist < 600 && (!match || dist < bestDist))
            {
                match = &e;
                bestDist = dist;
            }
        }
        if (!match)
        {
            continue;
        }
        match->mLive = true;
        match->mLiveX = x;
        match->mLiveY = y;
        match->mLiveTop = pObj->VGetBoundingRect().y;
        if (isAe)
        {
            match->mFollowing = static_cast<::Mudokon*>(pObj)->IsFollowingAbe();
            match->mState = static_cast<::Mudokon*>(pObj)->BrainState() * 100 + static_cast<::Mudokon*>(pObj)->BrainSubState();
            switch (static_cast<::Mudokon*>(pObj)->Emotion())
            {
                case Mud_Emotion::eNormal_0:
                    match->mMood = MudMood::eNormal;
                    break;
                case Mud_Emotion::eHappy_5:
                case Mud_Emotion::eWired_6:
                    match->mMood = MudMood::eWired;
                    break;
                case Mud_Emotion::eSick_7:
                    match->mMood = MudMood::eSick;
                    break;
                default:
                    match->mMood = MudMood::eUpset;
                    break;
            }
        }
        else
        {
            match->mFollowing = static_cast<AO::Mudokon*>(pObj)->field_1B8_brain_state == 10; // Brain_10_ListeningToAbe
        }
    }
}

std::vector<PathSummary> GameView::ReadLevel(BaseMap& map)
{
    std::vector<PathSummary> out;
    for (auto& path : map.GetLoadedPaths())
    {
        PathSummary summary;
        summary.mPath = static_cast<s32>(path->GetPathId());
        const NavInput in = BuildNavInput(*path, static_cast<s32>(map.mCurrentLevel), summary.mPath);
        for (const NavFeature& f : in.mFeatures)
        {
            if (f.mDestPath >= 0 && (f.mKind == FeatureKind::eDoor || f.mKind == FeatureKind::eWellExpress || f.mKind == FeatureKind::ePathExit || f.mKind == FeatureKind::eLevelLoader || f.mKind == FeatureKind::ePortal))
            {
                summary.mExits.push_back({f.mDestLevel, f.mDestPath});
            }
        }
        for (auto& cam : path->GetCameras())
        {
            if (!cam)
            {
                continue;
            }
            for (auto& tlv : cam->mTlvs.mTlvs)
            {
                summary.mMudokons += tlv->mTlvType == ReliveTypes::eMudokon ? 1 : 0;
            }
        }
        out.push_back(summary);
    }
    return out;
}

NavInput GameView::BuildNavInput(BaseMap& map)
{
    BinaryPath* pPath = map.GetPathResourceBlockPtr(static_cast<u32>(map.mCurrentPath));
    if (!pPath)
    {
        return {};
    }
    return BuildNavInput(*pPath, static_cast<s32>(map.mCurrentLevel), map.mCurrentPath);
}

} // namespace Bot
