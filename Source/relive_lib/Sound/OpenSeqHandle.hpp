#pragma once

#include "../Function.hpp"
#include <vector>

// A SEQ of the game's SEQ table (both games have one): its name, and the SEQ's data and the number
// libsnd gave it while the SEQ is open (see GameSound)
struct OpenSeqHandle final
{
    const char_type* field_0_mBsqName;
    s32 field_4_generated_res_id; // A hash of the named which matches the resource Id
    s8 field_8_sound_block_idx;
    s8 field_9_volume;
    s16 field_A_id_seqOpenId;
    std::vector<u8> field_C_ppSeq_Data;
};
ALIVE_ASSERT_SIZEOF(OpenSeqHandle, 0x10);
