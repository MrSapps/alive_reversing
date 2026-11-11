#pragma once

#include "../relive_lib/GameObjects/BaseAnimatedWithPhysicsGameObject.hpp"
#include "../relive_lib/SaveStateBase.hpp"

class AnimationUnknown;
namespace relive
{
    struct Path_ParamiteWebLine;
}

struct ParamiteWebLineSaveState final : public SaveStateBase
{
    ParamiteWebLineSaveState()
        : SaveStateBase(ReliveTypes::eParamiteWebLine, sizeof(*this))
    { }
    s16 field_104_wobble_idx;
    s16 field_106_wobble_pos;
    AnimationSaveState field_108_anim_flare;
    s16 field_1A0_pulse_position;
    s16 field_1A2_pulse_position_speed;
    s16 field_1A4_delay_counter;
};

class ParamiteWebLine final : public ::BaseAnimatedWithPhysicsGameObject
{
public:
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void RestoreFromSaveState(const ParamiteWebLineSaveState& state);

    ParamiteWebLine(relive::Path_ParamiteWebLine* pTlv, const Guid& tlvId, ResourceManager& resMan, BaseMap& map);
    ~ParamiteWebLine();

    void LoadAnimations();
    void Wobble(s16 ypos);

    virtual void VUpdate() override;
    virtual void VScreenChanged() override;
    virtual PSX_RECT VGetBoundingRect() override;
    virtual void VRender(OrderingTable& ot) override;

private:
    s16 field_F4_anim_segment_count = 0;
    s16 field_F6_piece_length = 0;
    s16 field_F8_top = 0;
    s16 field_FA_bottom = 0;
    AnimationUnknown* field_FC_pRes = nullptr;
    Guid field_100_tlv_info;
    s16 field_104_wobble_idx = 0;
    s16 field_106_wobble_pos = 0;
    Animation field_108_anim_flare = {};
    s16 field_1A0_pulse_position = 0;
    s16 field_1A2_pulse_position_speed = 0;
    s16 field_1A4_delay_counter = 0;
};
