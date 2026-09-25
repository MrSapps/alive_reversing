#pragma once

#include "relive_tlvs.hpp"
#include "../Collisions.hpp"
#include <nlohmann/json_fwd.hpp>
#include <functional>
#include <map>
#include <memory>
#include <string>

// The concrete editor map object types (one per TLV) and their json (de)serialization live in
// EditorMapObjects.cpp - they are only ever used through MapObjectBase and the factory registry, and
// instantiating them in every editor TU dominated the editor's build time.

class IReflector
{
public:
    virtual ~IReflector() { }

    virtual void Visit(const char* fieldName, ReliveTypes& field) = 0;
    virtual void Visit(const char* fieldName, relive::reliveScale& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_Mudokon::Mud_TLV_Emotion& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_BirdPortal::PortalSide& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_FlyingSlig::SpawnDelayStates& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_Slig::DeathMode& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_Slig::StartState& field) = 0;
    virtual void Visit(const char* fieldName, relive::reliveXDirection& field) = 0;
    virtual void Visit(const char* fieldName, relive::reliveSwitchOp& field) = 0;
    virtual void Visit(const char* fieldName, eLineTypes& field) = 0;
    virtual void Visit(const char* fieldName, EReliveLevelIds& field) = 0;
    virtual void Visit(const char* fieldName, relive::reliveScreenChangeEffects& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_ElectricWall::ElectricWallStartState& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_Mudokon::MudJobs& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_BirdPortal::PortalType& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_Door::DoorStates& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_Door::DoorTypes& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_Lever::LeverSoundType& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_Lever::LeverSoundDirection& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_Hoist::Type& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_Hoist::GrabDirection& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_BoomMachine::PipeSide& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_EnemyStopper::StopDirection& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_UXB::StartState& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_Edge::GrabDirection& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_ShadowZone::Scale& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_MusicTrigger::MusicTriggerMusicType& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_MusicTrigger::TriggeredBy& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_InvisibleSwitch::InvisibleSwitchScale& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_GasEmitter::GasColour& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_BackgroundAnimation::BgAnimSounds& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_BackgroundAnimation::Layer& field) = 0;
    virtual void Visit(const char* fieldName, relive::TBlendModes& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_LiftPoint::LiftPointStopType& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_PullRingRope::PullRingSwitchSound& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_PullRingRope::PullRingSoundDirection& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_ContinuePoint::Scale& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_ContinuePoint::spawnDirection& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_WheelSyncer::OutputRequirement& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_Drill::DrillBehavior& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_Drill::DrillDirection& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_TrapDoor::StartState& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_LiftMover::YDirection& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_MotionDetector::InitialMoveDirection& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_DoorFlame::Colour& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_CrawlingSlig::StartState& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_CrawlingSlig::CrawlDirection& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_CrawlingSligButton::ButtonSounds& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_Glukkon::GlukkonTypes& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_Glukkon::Facing& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_Glukkon::SpawnType& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_Glukkon::Behavior& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_FootSwitch::FootSwitchTriggerBy& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_SlogSpawner::StartDirection& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_Scrab::ScrabPatrolType& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_ScrabSpawner::SpawnDirection& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_Paramite::EntranceType& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_BellsongStone::BellsongTypes& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_LightEffect::Type& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_RingMudokon::MustFaceMud& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_MeatSaw::Type& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_MeatSaw::StartState& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_LiftMudokon::Direction& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_BeeSwarmHole::MovementType& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_ZBall::StartPos& field) = 0;
    virtual void Visit(const char* fieldName, relive::Path_ZBall::Speed& field) = 0;

    virtual void Visit(const char* fieldName, RGB16& field) = 0;

    virtual void Visit(const char* fieldName, bool& field) = 0;

    virtual void Visit(const char* fieldName, u16& field) = 0;
    virtual void Visit(const char* fieldName, s16& field) = 0;

    virtual void Visit(const char* fieldName, u32& field) = 0;
    virtual void Visit(const char* fieldName, s32& field) = 0;

    virtual void Visit(const char* fieldName, std::string& field) = 0;
};

// TODO: Move back to the editor src
class MapObjectBase
{
public:
    // Given some json make a derived MapObjectBase type
    using TEditorDeserializeFunc = std::function<std::unique_ptr<MapObjectBase>(const nlohmann::json&)>;
    using TEditorNewFunc = std::function<std::unique_ptr<MapObjectBase>()>;

    struct FactoryFuncs final
    {
        TEditorDeserializeFunc mDeserializeFunc = nullptr;
        TEditorNewFunc mNewFunc = nullptr;
    };

    static std::map<ReliveTypes, FactoryFuncs>& GetEditorFactoryRegistry();

    virtual void DoNotInheritFromSMapObjectBaseDirectlyButFromMapObjectBaseInterfaceInstead() = 0;

    explicit MapObjectBase(relive::Path_TLV* pTlv)
     : mBaseTlv(pTlv)
    {

    }

    virtual ~MapObjectBase() { }

    relive::Path_TLV* mBaseTlv = nullptr;

    void SetXPos(s32 xpos)
    {
        mBaseTlv->mTopLeftX = xpos;
    }

    void SetYPos(s32 ypos)
    {
        mBaseTlv->mTopLeftY = ypos;
    }

    void SetWidth(s32 width)
    {
        mBaseTlv->mBottomRightX = width;
    }

    void SetHeight(s32 height)
    {
        mBaseTlv->mBottomRightY = height;
    }

    s32 XPos() const
    {
        return mBaseTlv->mTopLeftX;
    }

    s32 YPos() const
    {
        return mBaseTlv->mTopLeftY;
    }

    s32 Width() const
    {
        return mBaseTlv->Width();
    }

    s32 Height() const
    {
        return mBaseTlv->Height();
    }

    virtual void Visit(IReflector& r)
    {
        r.Visit("Type", mBaseTlv->mTlvType);
        r.Visit("xpos", mBaseTlv->mTopLeftX);
        r.Visit("ypos", mBaseTlv->mTopLeftY);
        r.Visit("width", mBaseTlv->mBottomRightX);
        r.Visit("height", mBaseTlv->mBottomRightY);
    }

    virtual std::string GetIconPath() const
    {
        return "";
    }

    virtual std::unique_ptr<MapObjectBase> Clone() const = 0;
    virtual nlohmann::json SerializeObject() = 0;
};
