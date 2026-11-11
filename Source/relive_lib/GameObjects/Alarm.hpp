#pragma once

#include "../relive_lib/GameObjects/EffectBase.hpp"
#include "../relive_lib/Layer.hpp"
#include "../relive_lib/WorldStatic.hpp"
#include "../relive_lib/SaveStateBase.hpp"

namespace relive
{
    struct Path_Alarm;
}

struct AlarmSaveState final : public SaveStateBase
{
    AlarmSaveState()
        : SaveStateBase(ReliveTypes::eAlarm, sizeof(*this))
    { }
    EffectBaseSaveState mEffect;
    // Made from a TLV by the camera, or by a script
    bool mIsTlvAlarm;
    Guid mTlvInfo;
    s16 mAlarmRed;
    s32 mAlarmPauseTimer;
    s32 mAlarmDurationTimer;
    s32 mAlarmState;
    u16 mAlarmSwitchId;
    u16 mAlarmDuration;
    bool mDrawable;
};

class Alarm final : public EffectBase
{
public:
    Alarm(relive::Path_Alarm* pTlv, const Guid& tlvId, ResourceManager& resMan, BaseMap& map);
    Alarm(s32 durationTimer, u16 switchId, s32 pauseTimer, Layer layer, ResourceManager& resMan, BaseMap& map);
    ~Alarm();

    virtual void VRender(OrderingTable& ot) override;
    virtual void VUpdate() override;
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    // The alarm a script makes is made again
    static void CreateFromSaveState(const AlarmSaveState& state, ResourceManager& resMan, BaseMap& map);
    // The alarm the camera makes (from a TLV) is put back once the camera has made it
    static void RestoreFromSaveState(const AlarmSaveState& state);

private:
    // For a save: made with nothing counted, the counts are put back with the rest
    struct RestoreTag final
    { };
    Alarm(RestoreTag, Layer layer, ResourceManager& resMan, BaseMap& map);
    void SetFromSaveState(const AlarmSaveState& state);

    s16 mAlarmRed = 0;
    s32 mAlarmPauseTimer = 0;
    s32 mAlarmDurationTimer = 0;
    Guid mAlarmTlvInfo;
    enum class States
    {
        eWaitForSwitchEnable,
        eAfterConstructed,
        eEnabling,
        eOnFlash,
        eDisabling,
        eDisabled
    };
    States mAlarmState;
    u16 mAlarmSwitchId = 0;
    u16 mAlarmDuration = 0;
};

extern WorldStatic<s16> gAlarmInstanceCount;
extern WorldStatic<Guid> gAlarmObjId;
