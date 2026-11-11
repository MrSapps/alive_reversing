#pragma once

#include "../relive_lib/GameObjects/BaseAnimatedWithPhysicsGameObject.hpp"
#include "../relive_lib/Function.hpp"
#include "../relive_lib/data_conversion/relive_tlvs.hpp"
#include "../relive_lib/SaveStateBase.hpp"

struct Water_Res final
{
    FP field_0_xpos;
    FP field_4_ypos;
    FP field_8_zpos;
    FP field_C_delta_x;
    FP field_10_delta_y;
    FP field_14_delta_z;
    s16 field_18_enabled;
    s16 field_1A_splash_time;
    s16 field_1C_state;
    s16 field_1E_padding;
    Poly_FT4 field_20_poly;
};
ALIVE_ASSERT_SIZEOF(Water_Res, 0x78);

// The water the TLV makes again when a save is restored: this puts back where it was, see Water::RestoreSaveState.
struct WaterSaveState final : public SaveStateBase
{
    WaterSaveState()
        : SaveStateBase(ReliveTypes::eWater, sizeof(*this))
    { }
    s16 mState;
    s16 mParticleCount;
    s16 mCurrentParticleIdx;
    s16 mCurrentDrops;
    s16 mSplashTime;
    s32 mNotInCameraCount;
    s32 mWaterDuration;
    s16 mHitTimeout;
    // Where it is on the screen, worked out as it is made from where the screen was then
    s16 mScreenX;
    s16 mScreenY;
    s16 mTopLeftX;
    s16 mTopLeftY;
    s16 mBottomRightX;
    s16 mBottomRightY;
    FP mRadius;
    FP mCentre;
    struct Drop final
    {
        FP mXPos;
        FP mYPos;
        FP mZPos;
        FP mDeltaX;
        FP mDeltaY;
        FP mDeltaZ;
        s16 mEnabled;
        s16 mSplashTime;
        s16 mState;
    };
    // The most a Water has, see its constructor
    Drop mDrops[128];
};

class Water final : public ::BaseAnimatedWithPhysicsGameObject
{
public:
    Water(relive::Path_Water* pTlv, const Guid& tlvId, ResourceManager& resMan, BaseMap& map);
    ~Water();

    virtual void VUpdate() override;
    virtual void VRender(OrderingTable& ot) override;
    virtual void VScreenChanged() override;
    virtual void VStopAudio() override;
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    virtual std::vector<NotStateMemory> VNotStateMemory() override;
    static void RestoreSaveState(const WaterSaveState& state);

private:
    void Disable_Water_Particle(s16 idx);
    void Add_Water_Particle();

private:
    Water_Res* field_F8_pWaterRes = nullptr;

    enum class WaterState : s16
    {
        eInit_0 = 0,
        eStarting_1 = 1,
        eFlowing_2 = 2,
        eStopping_3 = 3,
        eInactive_4 = 4,
    };

    WaterState field_FC_state = WaterState::eInit_0;
    s16 field_100_screen_x = 0;
    s16 field_102_screen_y = 0;
    PSX_Point field_104_top_left = {};
    PSX_Point field_108_bottom_right = {};
    s16 field_10C_particle_count = 0;
    s16 field_10E_current_particle_idx = 0;
    s16 field_110_current_drops = 0;
    Guid field_114_tlvInfo;
    FP field_118_radius = {};
    FP field_11C_centre = {};
    s16 field_120_frame_width = 0;
    s16 field_122_frame_height = 0;
    relive::Path_Water field_124_tlv_data = {};
    FP field_130_splash_x_vel = {};
    FP field_134_emit_x_vel = {};
    s16 field_138_splash_time = 0;
    s32 field_13C_not_in_camera_count = 0;
    s32 field_140_water_duration = 0;
    s32 field_144_sound_channels = 0;
    s16 field_148_bHitTimeout = 0;
};
