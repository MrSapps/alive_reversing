#include "AESaveSerialization.hpp"

// TODO: Not AE specific move out of here
static nlohmann::json WriteObjectBlyJson(const Quicksave& q)
{
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

static void WriteObjectStateFromJson(const nlohmann::json& j, SerializedObjectData& object_states)
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
        ALIVE_FATAL("Unknown object type %s", type.c_str());
    }
}

// TODO: Not AE specific move out of here
static void ReadObjectStateJson(const nlohmann::json& j, SerializedObjectData& object_data)
{
    for (const auto& state : j["object_states"]) 
    {
        WriteObjectStateFromJson(state, object_data);
    }
}

// TODO: Not AE specific move out of here
static void ReadObjectBlyJson(const nlohmann::json& j, Quicksave& q)
{
    // Remove the first element since it's the flags count and divide by 2 because 1 flag has 2 fields/elements
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
