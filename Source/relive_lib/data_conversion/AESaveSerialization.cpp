#include "AESaveSerialization.hpp"

// TODO: Not AE specific move out of here
static nlohmann::json WriteObjectBlyJson(const Quicksave& q)
{
    // Where the last read of it (a restore) left off doesn't matter
    q.mObjectBlyData.ReadRewind();
    if (!q.mObjectBlyData.CanRead())
    {
        return {};
    }

    // TODO: This func is likely very slow
    const u32 flagsCount = q.mObjectBlyData.ReadU32();

    nlohmann::json j = nlohmann::json::array();

    j.push_back(flagsCount);
    // Flags and TLV specific meaning byte
    for (u32 i = 0; i < flagsCount * 2; i++)
    {
        j.push_back(q.mObjectBlyData.ReadU8());
    }

    return j;
}

void to_json(nlohmann::json& j, const Quicksave& p)
{
    j = nlohmann::json{
        {"world_info", p.mWorldInfo},
        {"restart_path_world_info", p.mRestartPathWorldInfo},
        {"restart_path_abe_state", p.mRestartPathAbeState},
        {"restart_path_switch_states", p.mRestartPathSwitchStates},
        {"switch_states", p.mSwitchStates}, 
        {"object_states", WriteObjectStateJson(p.mObjectsStateData)}, 
        {"object_bly_data", WriteObjectBlyJson(p)}
    };
}

template<typename T>
static void write_object_state(const nlohmann::json& j, SerializedObjectData& object_states)
{
    T data = j.get<T>();
    object_states.Write(data);
}

static void WriteObjectStateFromJsonImpl(const nlohmann::json& j, SerializedObjectData& object_states)
{
    const std::string type = j["type"].template get<std::string>();
    if (type == "slig_spawner")
    {
        write_object_state<SligSpawnerSaveState>(j, object_states);
    }
    else if (type == "lift_mover")
    {
        write_object_state<LiftMoverSaveState>(j, object_states);
    }
    else if (type == "bone")
    {
        write_object_state<BoneSaveState>(j, object_states);
    }
    else if (type == "lcd_screen")
    {
        write_object_state<LCDScreenSaveState>(j, object_states);
    }
    else if (type == relive::Path_Hoist::kClassName)
    {
        write_object_state<HoistRocksEffectSaveState>(j, object_states);
    }
    else if (type == relive::Path_InvisibleSwitch::kClassName)
    {
        write_object_state<InvisibleSwitchSaveState>(j, object_states);
    }
    else if (type == relive::Path_MusicTrigger::kClassName)
    {
        write_object_state<MusicTriggerSaveState>(j, object_states);
    }
    else if (type == relive::Path_SecurityOrb::kClassName)
    {
        write_object_state<SecurityOrbSaveState>(j, object_states);
    }
    else if (type == relive::Path_GasEmitter::kClassName)
    {
        write_object_state<GasEmitterSaveState>(j, object_states);
    }
    else if (type == "leaf")
    {
        write_object_state<LeafSaveState>(j, object_states);
    }
    else if (type == "snooze_particle")
    {
        write_object_state<SnoozeParticleSaveState>(j, object_states);
    }
    else if (type == relive::Path_Water::kClassName)
    {
        write_object_state<WaterSaveState>(j, object_states);
    }
    else if (type == "events_state")
    {
        write_object_state<EventsSaveState>(j, object_states);
    }
    else if (type == "list_order")
    {
        write_object_state<ListOrderSaveState>(j, object_states);
    }
    else if (type == "alive_object_state")
    {
        write_object_state<AliveObjectStateSaveState>(j, object_states);
    }
    else if (type == "gamespeak")
    {
        write_object_state<GameSpeakSaveState>(j, object_states);
    }
    else if (type == "game_speak_listener")
    {
        write_object_state<GameSpeakListenerSaveState>(j, object_states);
    }
    else if (type == "animation_state")
    {
        write_object_state<AnimationStateSaveState>(j, object_states);
    }
    else if (type == "dove")
    {
        write_object_state<DoveSaveState>(j, object_states);
    }
    else if (type == "mines_alarm")
    {
        write_object_state<MinesAlarmSaveState>(j, object_states);
    }
    else if (type == "crawling_slig")
    {
        write_object_state<CrawlingSligSaveState>(j, object_states);
    }
    else if (type == "drill")
    {
        write_object_state<DrillSaveState>(j, object_states);
    }
    else if (type == "evil_fart")
    {
        write_object_state<EvilFartSaveState>(j, object_states);
    }
    else if (type == "fleech")
    {
        write_object_state<FleechSaveState>(j, object_states);
    }
    else if (type == "flying_slig")
    {
        write_object_state<FlyingSligSaveState>(j, object_states);
    }
    else if (type == "flying_slig_spawner")
    {
        write_object_state<FlyingSligSpawnerSaveState>(j, object_states);
    }
    else if (type == "game_ender_controller")
    {
        write_object_state<GameEnderControllerSaveState>(j, object_states);
    }
    else if (type == "slap_lock_orb_whirlwind")
    {
        write_object_state<SlapLockWhirlWindSaveState>(j, object_states);
    }
    else if (type == "slap_lock")
    {
        write_object_state<SlapLockSaveState>(j, object_states);
    }
    else if (type == "greeter")
    {
        write_object_state<GreeterSaveState>(j, object_states);
    }
    else if (type == "grenade")
    {
        write_object_state<GrenadeSaveState>(j, object_states);
    }
    else if (type == "glukkon")
    {
        write_object_state<GlukkonSaveState>(j, object_states);
    }
    else if (type == "abe")
    {
        write_object_state<AbeSaveState>(j, object_states);
    }
    else if (type == "lift_point")
    {
        write_object_state<LiftPointSaveState>(j, object_states);
    }
    else if (type == "mudokon" || type == "ring_or_lift_mud")
    {
        write_object_state<MudokonSaveState>(j, object_states);
    }
    else if (type == "meat")
    {
        write_object_state<MeatSaveState>(j, object_states);
    }
    else if (type == "mine_car")
    {
        write_object_state<MineCarSaveState>(j, object_states);
    }
    else if (type == "paramite")
    {
        write_object_state<ParamiteSaveState>(j, object_states);
    }
    else if (type == "bird_portal")
    {
        write_object_state<BirdPortalSaveState>(j, object_states);
    }
    else if (type == "throwable_array")
    {
        write_object_state<ThrowableArraySaveState>(j, object_states);
    }
    else if (type == "ability_ring")
    {
        write_object_state<AbilityRingSaveState>(j, object_states);
    }
    else if (type == "rock")
    {
        write_object_state<RockSaveState>(j, object_states);
    }
    else if (type == "scrab")
    {
        write_object_state<ScrabSaveState>(j, object_states);
    }
    else if (type == "scrab_spawner")
    {
        write_object_state<ScrabSpawnerSaveState>(j, object_states);
    }
    else if (type == "slam_door")
    {
        write_object_state<SlamDoorSaveState>(j, object_states);
    }
    else if (type == "slig")
    {
        write_object_state<SligSaveState>(j, object_states);
    }
    else if (type == "slog")
    {
        write_object_state<SlogSaveState>(j, object_states);
    }
    else if (type == "slurg")
    {
        write_object_state<SlurgSaveState>(j, object_states);
    }
    else if (type == "timer_trigger")
    {
        write_object_state<TimerTriggerSaveState>(j, object_states);
    }
    else if (type == "trap_door")
    {
        write_object_state<TrapDoorSaveState>(j, object_states);
    }
    else if (type == "uxb")
    {
        write_object_state<UXBSaveState>(j, object_states);
    }
    else if (type == "work_wheel")
    {
        write_object_state<WorkWheelSaveState>(j, object_states);
    }
    else
    {
        // The rest are named for what they are
        switch (j["type"].template get<ReliveTypes>())
        {
            case ReliveTypes::eSpark:
                write_object_state<SparkSaveState>(j, object_states);
                break;

            case ReliveTypes::eParticle:
                write_object_state<ParticleSaveState>(j, object_states);
                break;

            case ReliveTypes::eGibs:
                write_object_state<GibsSaveState>(j, object_states);
                break;

            case ReliveTypes::eParticleBurst:
                write_object_state<ParticleBurstSaveState>(j, object_states);
                break;

            case ReliveTypes::eBlood:
                write_object_state<BloodSaveState>(j, object_states);
                break;

            case ReliveTypes::eDeathBird:
                write_object_state<DeathBirdParticleSaveState>(j, object_states);
                break;

            case ReliveTypes::eZapSpark:
                write_object_state<ZapSparkSaveState>(j, object_states);
                break;

            case ReliveTypes::eFlash:
                write_object_state<FlashSaveState>(j, object_states);
                break;

            case ReliveTypes::eFade:
            case ReliveTypes::eMainMenuTransistion:
                write_object_state<FadeSaveState>(j, object_states);
                break;

            case ReliveTypes::eAlarm:
                write_object_state<AlarmSaveState>(j, object_states);
                break;

            case ReliveTypes::eScreenShake:
                write_object_state<ScreenShakeSaveState>(j, object_states);
                break;

            case ReliveTypes::eAirExplosion:
                write_object_state<AirExplosionSaveState>(j, object_states);
                break;

            case ReliveTypes::eLaughingGas:
                write_object_state<LaughingGasSaveState>(j, object_states);
                break;

            case ReliveTypes::eFallingItem:
                write_object_state<FallingItemSaveState>(j, object_states);
                break;

            case ReliveTypes::eTeleporter:
                write_object_state<TeleporterSaveState>(j, object_states);
                break;

            case ReliveTypes::eMultiSwitchController:
                write_object_state<MultiSwitchControllerSaveState>(j, object_states);
                break;

            case ReliveTypes::eSecurityDoor:
                write_object_state<SecurityDoorSaveState>(j, object_states);
                break;

            case ReliveTypes::eMotionDetector:
                write_object_state<MotionDetectorSaveState>(j, object_states);
                break;

            case ReliveTypes::eLever:
                write_object_state<LeverSaveState>(j, object_states);
                break;

            case ReliveTypes::eFootSwitch:
                write_object_state<FootSwitchSaveState>(j, object_states);
                break;

            case ReliveTypes::eRollingBall:
                write_object_state<RollingBallSaveState>(j, object_states);
                break;

            case ReliveTypes::eRollingBallStopper:
                write_object_state<RollingBallStopperSaveState>(j, object_states);
                break;

            case ReliveTypes::eSlogSpawner:
                write_object_state<SlogSpawnerSaveState>(j, object_states);
                break;

            case ReliveTypes::eSlurgSpawner:
                write_object_state<SlurgSpawnerSaveState>(j, object_states);
                break;

            case ReliveTypes::eZzzSpawner:
                write_object_state<ZzzSpawnerSaveState>(j, object_states);
                break;

            case ReliveTypes::eBoomMachine:
                write_object_state<BoomMachineSaveState>(j, object_states);
                break;

            case ReliveTypes::eMeatSack:
                write_object_state<MeatSackSaveState>(j, object_states);
                break;

            case ReliveTypes::eRockSack:
                write_object_state<RockSackSaveState>(j, object_states);
                break;

            case ReliveTypes::eBoneBag:
                write_object_state<BoneBagSaveState>(j, object_states);
                break;

            case ReliveTypes::eMine:
                write_object_state<MineSaveState>(j, object_states);
                break;

            case ReliveTypes::eMovingBomb:
                write_object_state<MovingBombSaveState>(j, object_states);
                break;

            case ReliveTypes::eTimedMine:
                write_object_state<TimedMineSaveState>(j, object_states);
                break;

            case ReliveTypes::eDoorFlame:
                write_object_state<DoorFlameSaveState>(j, object_states);
                break;

            case ReliveTypes::eStatusLight:
                write_object_state<StatusLightSaveState>(j, object_states);
                break;

            case ReliveTypes::eDoorBlocker:
                write_object_state<DoorBlockerSaveState>(j, object_states);
                break;

            case ReliveTypes::eTorturedMud:
                write_object_state<TorturedMudokonSaveState>(j, object_states);
                break;

            case ReliveTypes::eGlukkonSwitch:
                write_object_state<GlukkonSwitchSaveState>(j, object_states);
                break;

            case ReliveTypes::eBackgroundAnimation:
                write_object_state<BackgroundAnimationSaveState>(j, object_states);
                break;

            case ReliveTypes::eLevelLoader:
                write_object_state<LevelLoaderSaveState>(j, object_states);
                break;

            case ReliveTypes::eBrewMachine:
                write_object_state<BrewMachineSaveState>(j, object_states);
                break;

            case ReliveTypes::eCrawlingSligButton:
                write_object_state<CrawlingSligButtonSaveState>(j, object_states);
                break;

            case ReliveTypes::eGasCountDown:
                write_object_state<GasCountDownSaveState>(j, object_states);
                break;

            case ReliveTypes::eSligGetPantsOrWings:
                write_object_state<SligGetPantsAndWingsSaveState>(j, object_states);
                break;

            case ReliveTypes::ePullRingRope:
                write_object_state<PullRingRopeSaveState>(j, object_states);
                break;

            case ReliveTypes::eParamiteWebLine:
                write_object_state<ParamiteWebLineSaveState>(j, object_states);
                break;

            case ReliveTypes::eElectricWall:
                write_object_state<ElectricWallSaveState>(j, object_states);
                break;

            case ReliveTypes::eDoor:
                write_object_state<DoorSaveState>(j, object_states);
                break;

            case ReliveTypes::eBulletShell:
                write_object_state<BulletShellSaveState>(j, object_states);
                break;

            case ReliveTypes::eBullet:
                write_object_state<BulletSaveState>(j, object_states);
                break;

            case ReliveTypes::eColourfulMeter:
                write_object_state<ColourfulMeterSaveState>(j, object_states);
                break;

            case ReliveTypes::ePalOverwriter:
                write_object_state<PalleteOverwriterSaveState>(j, object_states);
                break;

            case ReliveTypes::eElectrocute:
                write_object_state<ElectrocuteSaveState>(j, object_states);
                break;

            case ReliveTypes::eGroundExplosion:
                write_object_state<GroundExplosionSaveState>(j, object_states);
                break;

            case ReliveTypes::eExplosionSet:
                write_object_state<ExplosionSetSaveState>(j, object_states);
                break;

            case ReliveTypes::ePossessionFlicker:
                write_object_state<PossessionFlickerSaveState>(j, object_states);
                break;

            case ReliveTypes::eDeathGas:
                write_object_state<DeathGasSaveState>(j, object_states);
                break;

            case ReliveTypes::eThrowableTotalIndicator:
                write_object_state<ThrowableTotalIndicatorSaveState>(j, object_states);
                break;

            case ReliveTypes::eOrbWhirlWind:
                write_object_state<OrbWhirlWindSaveState>(j, object_states);
                break;

            case ReliveTypes::eBirdPortalTerminator:
                write_object_state<BirdPortalTerminatorSaveState>(j, object_states);
                break;

            case ReliveTypes::eScreenClipper:
                write_object_state<ScreenClipperSaveState>(j, object_states);
                break;

            case ReliveTypes::eShrykull:
                write_object_state<ShrykullSaveState>(j, object_states);
                break;

            case ReliveTypes::eZapLine:
                write_object_state<ZapLineSaveState>(j, object_states);
                break;

            case ReliveTypes::eCircularFade:
                write_object_state<CircularFadeSaveState>(j, object_states);
                break;

            case ReliveTypes::eInvisibleEffect:
                write_object_state<InvisibleEffectSaveState>(j, object_states);
                break;

            case ReliveTypes::eParamiteWeb:
                write_object_state<ParamiteWebSaveState>(j, object_states);
                break;

            case ReliveTypes::eRollingBallShaker:
                write_object_state<RollingBallShakerSaveState>(j, object_states);
                break;

            case ReliveTypes::eBoomMachinePipe:
                write_object_state<BoomMachinePipeSaveState>(j, object_states);
                break;

            case ReliveTypes::eWorldStatics:
                write_object_state<WorldStaticsSaveState>(j, object_states);
                break;

            default:
                ALIVE_FATAL("Unknown object type %s", type.c_str());
        }
    }
}

// TODO: Not AE specific move out of here
// The state's own fields come from its type's from_json, its id is on every state (SaveStateBase::mObjectId).
// Saves from before the id was kept don't have it.
static void WriteObjectStateFromJson(const nlohmann::json& j, SerializedObjectData& object_states)
{
    const std::size_t startOffset = object_states.Size();
    WriteObjectStateFromJsonImpl(j, object_states);
    if (j.contains("base_object_id") && object_states.Size() - startOffset >= sizeof(SaveStateBase))
    {
        const Guid objectId = j.at("base_object_id").get<Guid>();
        object_states.OverwriteAt(startOffset + offsetof(SaveStateBase, mObjectId), &objectId, sizeof(Guid));
    }
    if (j.contains("base_last_line_y_pos") && object_states.Size() - startOffset >= sizeof(SaveStateBase))
    {
        const FP lastLineYPos = j.at("base_last_line_y_pos").get<FP>();
        object_states.OverwriteAt(startOffset + offsetof(SaveStateBase, mLastLineYPos), &lastLineYPos, sizeof(FP));
    }
    if (j.contains("base_path_tlv_id") && object_states.Size() - startOffset >= sizeof(SaveStateBase))
    {
        const Guid pathTlvId = j.at("base_path_tlv_id").get<Guid>();
        object_states.OverwriteAt(startOffset + offsetof(SaveStateBase, mPathTlvId), &pathTlvId, sizeof(Guid));
    }
}

static void ReadObjectStateJson(const nlohmann::json& j, SerializedObjectData& object_data)
{
    for (const auto& state : j["object_states"]) 
    {
        try
        {
            WriteObjectStateFromJson(state, object_data);
        }
        catch (const nlohmann::json::exception& e)
        {
            ALIVE_FATAL("A saved object of type %s isn't what its json should be: %s", state.value("type", std::string("?")).c_str(), e.what());
        }
    }
}

// TODO: Not AE specific move out of here
static void ReadObjectBlyJson(const nlohmann::json& j, Quicksave& q)
{
    // Remove the first element since it's the flags count and divide by 2 because 1 flag has 2 fields/elements
    if (j.at("object_bly_data").empty())
    {
        return;
    }
    const u32 flagsCount = static_cast<u32>((j.at("object_bly_data").size() - 1) / 2);

    q.mObjectBlyData.WriteRewind();
    q.mObjectBlyData.WriteU32(flagsCount);
    for (size_t i = 1; i < j["object_bly_data"].size(); i++)
    {
        const auto& state = j["object_bly_data"].at(i);
        q.mObjectBlyData.WriteU8(state);
    }
}

void from_json(const nlohmann::json& j, Quicksave& p)
{
    j.at("world_info").get_to(p.mWorldInfo);
    j.at("restart_path_world_info").get_to(p.mRestartPathWorldInfo);
    j.at("restart_path_abe_state").get_to(p.mRestartPathAbeState);
    j.at("restart_path_switch_states").get_to(p.mRestartPathSwitchStates);
    j.at("switch_states").get_to(p.mSwitchStates);
    ReadObjectStateJson(j, p.mObjectsStateData);
    ReadObjectBlyJson(j, p);
}
