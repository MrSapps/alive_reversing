#pragma once

#include "../../relive_lib/Function.hpp"
#include "Sound.hpp"

class AutoFILE;
class FileSystem;

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

s16 SsVabOpenHead(VabHeader* pVabHeader);

struct VabBodyRecord final
{
    s32 field_0_length_or_duration;
    s32 field_4_unused;
    u32 field_8_fileOffset;
};

struct Converted_Vag final
{
    u16 field_0_adsr_attack;
    u16 field_2_adsr_sustain_level;
    u16 field_4_adsr_decay;
    u16 field_6_adsr_release;
    u8 field_8_min;
    u8 field_9_max;
    s16 field_A_shift_cen;
    u8 field_C;
    u8 field_D_vol;
    u8 field_E_priority;
    u8 field_F_prog;
    u8 field_10_vag;
    s8 field_11_pad;
};
ALIVE_ASSERT_SIZEOF(Converted_Vag, 0x12);

constexpr s32 kMaxVabs = 4;

struct ConvertedVagTable final
{
    Converted_Vag table[kMaxVabs][128][16]; // 16 = max tones, 128 = max progs
};
ALIVE_ASSERT_SIZEOF(ConvertedVagTable, 147456);

struct SoundEntryTable final
{
    SoundEntry table[kMaxVabs][256];
};
ALIVE_ASSERT_SIZEOF(SoundEntryTable, 36864);

struct VabUnknown final
{
    s8 field_0[4][128];
};
ALIVE_ASSERT_SIZEOF(VabUnknown, 512);

const s32 kNumChannels = 24;

// Only exposed for SND_PlayEx!
struct MIDI_ADSR_State final
{
    u8 field_0_seq_idx;
    u8 field_1_program;
    u8 field_2_note_byte1;
    s8 field_3_state;
    u16 field_4_attack;
    u16 field_6_sustain;
    u16 field_8_decay;
    u16 field_A_release;
    u16 field_C;
    u8 field_E_ref_count;
    s8 field_F_pad;
};
ALIVE_ASSERT_SIZEOF(MIDI_ADSR_State, 0x10);

struct MIDI_Channel final
{
    s32 field_0_sound_buffer_field_4;
    s32 field_4_priority;
    s32 field_8_left_vol;
    s32 field_C_vol; // pan vol?
    f32 field_10_freq;
    s32 field_14_time;
    s32 field_18_rightVol;
    MIDI_ADSR_State field_1C_adsr;
};
ALIVE_ASSERT_SIZEOF(MIDI_Channel, 0x2C);

struct MidiChannels final
{
    MIDI_Channel channels[kNumChannels];
};
ALIVE_ASSERT_SIZEOF(MidiChannels, 1056);

struct MIDI_ProgramVolume final
{
    s8 field_0_program;
    s8 field_1_left_vol;
    s8 field_2_right_vol;
};
ALIVE_ASSERT_SIZEOF(MIDI_ProgramVolume, 3);

struct MIDI_SeqSong final
{
    u8* field_0_seq_data;
    u32 field_4_time;
    s32 field_8_playTimeStamp;
    s32 field_C_volume;
    s32 field_10_quaterNoteRes;
    s32 field_14_tempo;
    s32 field_18_repeatCount;
    u8* field_1C_pSeqData;
    void* field_20_fn_ptr; // read but never written
    u8* field_24_loop_start;
    s16 field_seq_idx;
    u8 field_2A_running_status;
    s8 field_2B_repeatMode; // TODO: Check
    s8 field_2C_loop_count;
    s8 field_2D_pad;
    s16 field_2E_seqAccessNum; // never written in a meaningful way
    s8 field_30_timeSignatureBars;
    s8 field_31_timeSignatureBars2; // bug: maybe they should have assigned beats instead? but never read anyway
    MIDI_ProgramVolume field_32_progVols[16];
    // Microseconds of field_4_time that are left over from the whole milliseconds (was padding)
    u16 mTimeRemainderUs;
};
ALIVE_ASSERT_SIZEOF(MIDI_SeqSong, 100);

struct MidiSeqSongsTable final
{
    MIDI_SeqSong table[32];
};

ALIVE_ASSERT_SIZEOF(MidiSeqSongsTable, 3200);

class IPsxSpuApiVars
{
public:
    virtual ~IPsxSpuApiVars()
    { }
    virtual s16& sGlobalVolumeLevel_right() = 0;
    virtual s16& sGlobalVolumeLevel_left() = 0;
    virtual VabUnknown& s512_byte() = 0;
    virtual u8* sVagCounts() = 0;
    virtual u8* sProgCounts() = 0;
    virtual VabHeader** spVabHeaders() = 0;
    virtual ConvertedVagTable& sConvertedVagTable() = 0;
    virtual SoundEntryTable& sSoundEntryTable16() = 0;
    virtual MidiChannels& sMidi_Channels() = 0;
    virtual MIDI_SeqSong& sMidiSeqSongs(s32 idx) = 0;
    virtual s32& sMidi_Inited_dword() = 0;
    virtual u32& sMidiTime() = 0;
    virtual bool& sSoundDatIsNull() = 0;
    virtual s8& sbDisableSeqs() = 0;
    virtual u32& sLastTime() = 0;
    virtual u32& sMidi_WaitUntil() = 0;
    virtual AutoFILE& sSoundDatFileHandle() = 0;
    virtual u8& sControllerValue() = 0;
    virtual void MIDI_ParseMidiMessage(s32 idx) = 0;
    virtual void SsUtKeyOffV(s32 idx) = 0;

    // Where the game's PS1 executable used libsnd differently, for -ps1_sound:
    // The master volume (SsSetMVol) the game sets on init and on each sound reset
    virtual s16 DefaultMasterVolume() = 0;
};

// nullptr sets the default (AE) vars back
void SetSpuApiVars(IPsxSpuApiVars* pVars);
IPsxSpuApiVars* GetSpuApiVars();

void SsVabTransBody_4FC840(FileSystem& fs, VabBodyRecord* pVabBody, s16 vabId);

#define SS_WAIT_COMPLETED 1
void SsVabTransCompleted(s32 immediateFlag);
void SsVabClose_4FC5B0(s32 vabId);


void SsSetMVol_4FC360(s16 left, s16 right);
void SSInit_4FC230();
void SpuInitHot_4FC320();
void SsEnd_4FC350();

void SsSetTableSize_4FE0B0(void* pTable, s32 sMax, s32 tMax);

void SsUtSetReverbDepth_4FE380(s32 leftDepth, s32 rightDepth);
void SsUtSetReverbType_4FE360(s32);
void SsUtReverbOn_4FE340();
void SsUtReverbOff_4FE350();
void SpuClearReverbWorkArea_4FA690(s32 reverbMode);

void SsSetTickMode_4FDC20(s32 tickMode);
s32 SsVoKeyOn_4FCF10(s32 vabIdAndProgram, s32 pitch, u16 leftVol, u16 rightVol);
void SsUtAllKeyOff(s32 mode);
s16 SsUtKeyOffV_4FE010(s16 idx);
s16 SsUtChangePitch_4FDF70(s16 voice, s32 vabId, s32 prog, s16 old_note, s16 old_fine, s16 new_note, s16 new_fine);

s16 SsSeqOpen_4FD6D0(u8* pSeqData, u32 seqSize, s16 seqIdx);
void SsSeqClose_4FD8D0(s16 idx);
void SsSeqStop(s16 idx);
u16 SsIsEos_4FDA80(s16 idx, s16 seqNum);
void SsSeqSetVol(s16 idx, s16 volLeft, s16 volRight);
void SsSeqPlay_4FD900(u16 idx, s8 playMode, s16 repeatCount);

s32 MIDI_ParseMidiMessage_4FD100(s32 idx);
s32 MIDI_Read_Var_Len_4FD0D0(MIDI_SeqSong* pMidiStru);
// Moves the song's next event time (field_4_time, ms) on by ticks at its tempo (field_14_tempo, us
// per tick), keeping the sub millisecond remainder. The PC code rounded every delta down to a whole
// ms, which made the SEQs play up to 2% fast.
void MIDI_AddDeltaTime(MIDI_SeqSong& song, u32 ticks);
u8 MIDI_ReadByte_4FD6B0(MIDI_SeqSong* pData);
void MIDI_SkipBytes_4FD6C0(MIDI_SeqSong* pData, s32 length);
void MIDI_SetTempo(s16 idx, s16 kZero, s16 tempo);
s32 MIDI_PlayerPlayMidiNote_4FCE80(s32 vabId, s32 program, s32 note, s32 leftVol, s32 rightVol, s32 volume);
s32 MIDI_Allocate_Channel(s32 not_used, s32 priority);

using TVSyncCallBackFn = void(CC*)();
void VSyncCallback_4F8C40(TVSyncCallBackFn callBack);
void SND_CallBack_4020A4(); // TODO: Naming??
void SsSeqCalledTbyT();

// Most likely PC specific extensions that have been inlined
void SsExt_CloseAllVabs();

void SsExt_StopPlayingSamples();

// Sets field_C of every converted vag that uses vagIdx, shared by the AE + AO SsVabTransBody
void SsExt_SetVagFlags(s16 vabId, s32 vagIdx, u8 flags);
