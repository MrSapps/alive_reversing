#pragma once

#include "../Function.hpp"
#include "../GameType.hpp"
#include <string>
#include <vector>

// The layout of a VAB header (VH) file: the header, then 16 tones for each program
struct ProgAtr final
{
    u8 field_0_num_tones;
    s8 field_1_vol;
    s8 field_2_priority;
    s8 field_3_mode;
    s8 field_4_pan;
    s8 field_5_reserved0;
    s16 field_6_attr;
    s32 field_8_reserved1;
    s32 field_C_reserved2;
};
ALIVE_ASSERT_SIZEOF(ProgAtr, 0x10);

struct VabHeader final
{
    s32 field_0_form;
    s32 field_4_version;
    s32 field_8_id;
    s32 field_C_file_size;
    s16 field_10_reserved0;
    s16 field_12_num_progs;
    s16 field_14_num_tones;
    u16 field_16_num_vags;
    s8 field_18_master_vol;
    s8 field_19_master_pan;
    s8 field_1A_attr1;
    s8 field_1B_attr2;
    s32 field_1C_reserved1;
    ProgAtr field_20_progs[128];
};
ALIVE_ASSERT_SIZEOF(VabHeader, 0x820);

// A VAB tone, 16 per program after the VabHeader
struct VagAtr final
{
    s8 field_0_priority;
    s8 field_1_mode;
    s8 field_2_vol;
    s8 field_3_pan;
    u8 field_4_centre;
    u8 field_5_shift;
    s8 field_6_min;
    s8 field_7_max;
    s8 field_8_vibW;
    s8 field_9_vibT;
    s8 field_A_porW;
    s8 field_B_porT;
    s8 field_C_pitch_bend_min;
    s8 field_D_pitch_bend_max;
    s8 field_E_reserved1;
    s8 field_F_reserved2;
    s16 field_10_adsr1;
    s16 field_12_adsr2;
    s16 field_14_prog;
    s16 field_16_vag;
    s16 field_18_reserved[4];
};
ALIVE_ASSERT_SIZEOF(VagAtr, 0x20);

// One record of a VAB body (sound data) file, see Midi.cpp for how the games read their bodies
struct VabBodyRecord final
{
    s32 field_0_length_or_duration;
    s32 field_4_unused;
    u32 field_8_fileOffset;
};

class VabFile final
{
public:
    // The PC data lowered some of AO's tone priorities and volumes: the secret area jingle's from 127
    // to 70, so sound effects (an electric wall's buzz) took its voices. Puts the PS1 values back in
    // VH data, before it is opened. vhFile: the VH's name, e.g. RFSNDFX.VH. A tone only changes when
    // it is as the PC data has it.
    static void RestorePs1Tones(GameType game, const std::string& vhFile, std::vector<u8>& vhData);
};
