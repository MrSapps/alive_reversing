#include "stdafx.h"
#include "MusicController.hpp"
#include "../relive_lib/Function.hpp"
#include "stdlib.hpp"
#include "../relive_lib/Sound/Midi.hpp"
#include "../relive_lib/Sound/PsxSoundEngine.hpp"
#include "../relive_lib/SoundDebug.hpp"
#include "../relive_lib/Events.hpp"
#include "Abe.hpp"
#include "Map.hpp"
#include "../relive_lib/ObjectIds.hpp"
#include "PathData.hpp"
#include "../relive_lib/BaseGameAutoPlayer.hpp"
#include "Math.hpp"

static MusicController* sMusicController = nullptr;
static u32 sMusicControllerBaseTimeStamp = 0;
static u32 sMusicTime = 0;

// Logs the result, for replaying a run on the PS1's MusicController with the same choices
static s32 MusicRandom(s32 min, s32 max)
{
    const s32 value = Math_RandomRange(min, max);
    SoundDebug::LogSeqCall("RAND %d %d %d", min, max, value);
    return value;
}

struct MusicController_Record3_Sub final
{
    s16 field_0;
    s8 field_2_min;
    s8 field_3_max;
};
ALIVE_ASSERT_SIZEOF(MusicController_Record3_Sub, 4);

struct MusicController_Record3 final
{
    MusicController_Record3_Sub field_0[3];
    u8 field_C_repeat_count;
    u8 field_D_padding;
};
ALIVE_ASSERT_SIZEOF(MusicController_Record3, 0xE);

// TODO: 17 = max levels?

const MusicController_Record3 stru_55D1E0[17] = {
    {{{2048, 107, 108}, {-1, 109, 109}, {-1, 0, 0}}, 1u, 0u},
    {{{320, 103, 103}, {1280, 102, 102}, {-1, 101, 101}}, 1u, 0u},
    {{{512, 8, 8}, {512, 7, 7}, {-1, 6, 6}}, 1u, 0u},
    {{{512, 17, 17}, {512, 16, 16}, {-1, 15, 15}}, 1u, 0u},
    {{{1024, 33, 33}, {512, 32, 32}, {-1, 31, 31}}, 1u, 0u},
    {{{640, 49, 49}, {640, 48, 48}, {-1, 47, 47}}, 1u, 0u},
    {{{640, 62, 62}, {640, 61, 61}, {-1, 60, 60}}, 1u, 0u},
    {{{-1, 40, 40}, {-1, -1, -1}, {-1, -1, -1}}, 1u, 0u},
    {{{320, 72, 72}, {320, 71, 71}, {-1, 70, 70}}, 1u, 0u},
    {{{320, 88, 88}, {320, 87, 87}, {-1, 86, 86}}, 1u, 0u},
    {{{512, 97, 97}, {-1, 96, 97}, {-1, -1, -1}}, 1u, 0u},
    {{{-1, 24, 24}, {-1, -1, -1}, {-1, -1, -1}}, 1u, 0u},
    {{{-1, 56, 56}, {-1, -1, -1}, {-1, -1, -1}}, 1u, 0u},
    {{{-1, 66, 66}, {-1, -1, -1}, {-1, -1, -1}}, 1u, 0u},
    {{{-1, 79, 79}, {-1, -1, -1}, {-1, -1, -1}}, 1u, 0u},
    {{{320, 103, 103}, {1280, 102, 102}, {-1, 101, 101}}, 1u, 0u},
    {{{-1, 111, 111}, {-1, 0, 0}, {-1, 0, 0}}, 1u, 0u}};

enum class AmbientMusic : s8
{
    eOff = 0,
    eOn = 1
};

struct MusicController_Record final
{
    s8 field_0_seq_id_idx;
    s8 field_1_unused;
    AmbientMusic field_2_bAmbient_music_enabled;
    s8 field_3_padding;
};
ALIVE_ASSERT_SIZEOF(MusicController_Record, 0x4);

const MusicController_Record tension_55D2D0[17] = {
    {SeqId::BonewerkzEnderChase2_111, 16u, AmbientMusic::eOff, 0u},
    {SeqId::BonewerkzChase2_104, 20u, AmbientMusic::eOff, 0u},
    {SeqId::HitBottomOfDeathPit_9, 20u, AmbientMusic::eOff, 0u},
    {SeqId::AbeStopIt_18, 16u, AmbientMusic::eOff, 0u},
    {SeqId::NecrumCalm2_34, 16u, AmbientMusic::eOff, 0u},
    {SeqId::MudomoSafeTension_50, 20u, AmbientMusic::eOff, 0u},
    {SeqId::MudancheePossess_63, 20u, AmbientMusic::eOff, 0u},
    {SeqId::NecrumSafeTension_41, 28u, AmbientMusic::eOff, 0u},
    {SeqId::MudancheeEnderSafeTension_73, 20u, AmbientMusic::eOff, 0u},
    {SeqId::BarracksCalm1_89, 20u, AmbientMusic::eOff, 0u},
    {SeqId::BonewerkzCalm2_98, 20u, AmbientMusic::eOff, 0u},
    {SeqId::ParamiteNearby_25, 28u, AmbientMusic::eOff, 0u},
    {SeqId::MudomoEnderSafeTension_57, 28u, AmbientMusic::eOff, 0u},
    {SeqId::MudancheeEnderCalm_67, 28u, AmbientMusic::eOff, 0u},
    {SeqId::FeecoTension2_80, 28u, AmbientMusic::eOff, 0u},
    {SeqId::BonewerkzChase2_104, 20u, AmbientMusic::eOff, 0u},
    {-1, 1u, AmbientMusic::eOn, 0u}};

const MusicController_Record chase_55D314[17] = {
    {SeqId::BonewerkzEnderSafeTension_112, 16u, AmbientMusic::eOff, 0u},
    {SeqId::BonewerkzSafeTension_105, 20u, AmbientMusic::eOff, 0u},
    {SeqId::MudokonChant1_10, 20u, AmbientMusic::eOff, 0u},
    {SeqId::WheelSqueak_19, 16u, AmbientMusic::eOff, 0u},
    {SeqId::NecrumCalm1_35, 16u, AmbientMusic::eOff, 0u},
    {SeqId::MudomoEnderCalm_51, 20u, AmbientMusic::eOff, 0u},
    {SeqId::MudancheeTension2_64, 20u, AmbientMusic::eOff, 0u},
    {SeqId::MudomoCalm3_42, 28u, AmbientMusic::eOff, 0u},
    {SeqId::FeecoCalm3_74, 20u, AmbientMusic::eOff, 0u},
    {SeqId::BarracksTension_90, 20u, AmbientMusic::eOff, 0u},
    {SeqId::BonewerkzCalm1_99, 20u, AmbientMusic::eOff, 0u},
    {SeqId::SleepingFleechOffscreen_26, 28u, AmbientMusic::eOff, 0u},
    {SeqId::MudancheeCalm3_58, 28u, AmbientMusic::eOff, 0u},
    {SeqId::MudancheeEnderTension1_68, 28u, AmbientMusic::eOff, 0u},
    {SeqId::FeecoChase2_81, 28u, AmbientMusic::eOff, 0u},
    {SeqId::BonewerkzSafeTension_105, 20u, AmbientMusic::eOff, 0u},
    {-1, 1u, AmbientMusic::eOn, 0u}};

const MusicController_Record possessed_55D358[17] = {
    {-1, 1u, AmbientMusic::eOn, 0u},
    {SeqId::BonewerkzEnderCalm_106, 20u, AmbientMusic::eOff, 0u},
    {SeqId::MudokonChant_11, 20u, AmbientMusic::eOff, 0u},
    {SeqId::MainMenuParamiteAttack_20, 16u, AmbientMusic::eOff, 0u},
    {SeqId::NecrumTension1_36, 16u, AmbientMusic::eOff, 0u},
    {SeqId::MudomoEnderTension1_52, 20u, AmbientMusic::eOff, 0u},
    {SeqId::MudancheeChase2_65, 20u, AmbientMusic::eOff, 0u},
    {SeqId::MudomoCalm2_43, 28u, AmbientMusic::eOff, 0u},
    {SeqId::FeecoCalm2_75, 20u, AmbientMusic::eOff, 0u},
    {SeqId::BarracksPossess_91, 20u, AmbientMusic::eOff, 0u},
    {SeqId::BonewerkzTension1_100, 20u, AmbientMusic::eOff, 0u},
    {SeqId::ToCalm1_27, 28u, AmbientMusic::eOff, 0u},
    {SeqId::MudancheeCalm2_59, 28u, AmbientMusic::eOff, 0u},
    {SeqId::MudancheePossess_69, 28u, AmbientMusic::eOff, 0u},
    {SeqId::FeecoSafeTension_82, 28u, AmbientMusic::eOff, 0u},
    {SeqId::BonewerkzEnderCalm_106, 20u, AmbientMusic::eOff, 0u},
    {-1, 1u, AmbientMusic::eOn, 0u}};

const MusicController_Record slogTension_55D39C[17] = {
    {-1, 1u, AmbientMusic::eOn, 0u},
    {SeqId::BreweryCalm3_113, 20u, AmbientMusic::eOff, 0u},
    {SeqId::Scratching_12, 20u, AmbientMusic::eOff, 0u},
    {SeqId::WalkingSligOffscreen_21, 20u, AmbientMusic::eOff, 0u},
    {SeqId::NecrumPossess_37, 20u, AmbientMusic::eOff, 0u},
    {SeqId::MudomoEnderPossess_53, 20u, AmbientMusic::eOff, 0u},
    {-1, 1u, AmbientMusic::eOff, 0u},
    {SeqId::MudomoCalm1_44, 20u, AmbientMusic::eOff, 0u},
    {SeqId::FeecoCalm1_76, 20u, AmbientMusic::eOff, 0u},
    {SeqId::BarracksChase_92, 20u, AmbientMusic::eOff, 0u},
    {-1, 1u, AmbientMusic::eOff, 0u},
    {SeqId::ToCalm2_28, 20u, AmbientMusic::eOff, 0u},
    {-1, 1u, AmbientMusic::eOff, 0u},
    {-1, 1u, AmbientMusic::eOff, 0u},
    {SeqId::FeecoEnderCalm_83, 20u, AmbientMusic::eOff, 0u},
    {SeqId::BreweryCalm3_113, 20u, AmbientMusic::eOff, 0u},
    {-1, 1u, AmbientMusic::eOn, 0u}};

const MusicController_Record slogChase_55D3E0[17] = {
    {-1, 1u, AmbientMusic::eOn, 0u},
    {SeqId::BreweryCalm2_114, 20u, AmbientMusic::eOff, 0u},
    {SeqId::Empty_13, 20u, AmbientMusic::eOff, 0u},
    {SeqId::SleepingSligOffscreen_22, 20u, AmbientMusic::eOff, 0u},
    {SeqId::NecrumChase1_38, 20u, AmbientMusic::eOff, 0u},
    {SeqId::MudomoEnderChase1_54, 20u, AmbientMusic::eOff, 0u},
    {-1, 1u, AmbientMusic::eOff, 0u},
    {SeqId::MudomoTension1_45, 20u, AmbientMusic::eOff, 0u},
    {SeqId::FeecoTension1_77, 20u, AmbientMusic::eOff, 0u},
    {SeqId::BarracksEnderCalm_93, 20u, AmbientMusic::eOff, 0u},
    {-1, 1u, AmbientMusic::eOff, 0u},
    {SeqId::DeathDrums_29, 20u, AmbientMusic::eOff, 0u},
    {-1, 1u, AmbientMusic::eOff, 0u},
    {-1, 1u, AmbientMusic::eOff, 0u},
    {SeqId::FeecoEnderTension_84, 20u, AmbientMusic::eOff, 0u},
    {SeqId::BreweryCalm2_114, 20u, AmbientMusic::eOff, 0u},
    {-1, 1u, AmbientMusic::eOn, 0u}};

const MusicController_Record slogChaseTension_55D424[17] = {
    {-1, 1u, AmbientMusic::eOn, 0u},
    {SeqId::BreweryCalm1_115, 20u, AmbientMusic::eOff, 0u},
    {SeqId::Explosion1_14, 20u, AmbientMusic::eOff, 0u},
    {SeqId::AwakeSlogOffscreen_23, 20u, AmbientMusic::eOff, 0u},
    {SeqId::NecrumTension2_39, 20u, AmbientMusic::eOff, 0u},
    {SeqId::MudomoEnderTension2_55, 20u, AmbientMusic::eOff, 0u},
    {-1, 1u, AmbientMusic::eOff, 0u},
    {SeqId::MudomoChase1_46, 20u, AmbientMusic::eOff, 0u},
    {SeqId::FeecoPossess_78, 20u, AmbientMusic::eOff, 0u},
    {SeqId::BarracksEnderTension_94, 20u, AmbientMusic::eOff, 0u},
    {-1, 1u, AmbientMusic::eOff, 0u},
    {SeqId::AbeDeathDrums_30, 20u, AmbientMusic::eOff, 0u},
    {-1, 1u, AmbientMusic::eOff, 0u},
    {-1, 1u, AmbientMusic::eOff, 0u},
    {SeqId::FeecoEnderPossess_85, 20u, AmbientMusic::eOff, 0u},
    {SeqId::BreweryCalm1_115, 20u, AmbientMusic::eOff, 0u},
    {-1, 1u, AmbientMusic::eOn, 0u}};

// TODO: 118 = max SEQs?
struct MusicController_Record2 final
{
    u16 field_0_idx;
    u16 field_2_duration;
};
ALIVE_ASSERT_SIZEOF(MusicController_Record2, 4);

const MusicController_Record2 stru_55D008[118] = {
    {SeqId::ToCalm1_27, 320u},
    {SeqId::ToCalm2_28, 320u},
    {SeqId::DeathDrums_29, 320u},
    {SeqId::AbeDeathDrums_30, 320u},
    {SeqId::SaveTriggerMusic_31, 320u},
    {SeqId::SecretMusic_32, 320u},
    {SeqId::NecrumCalm3_33, 512u},
    {SeqId::NecrumCalm2_34, 512u},
    {SeqId::NecrumCalm1_35, 256u},
    {SeqId::NecrumTension1_36, 320u},
    {SeqId::NecrumChase1_38, 320u},
    {SeqId::NecrumPossess_37, 320u},
    {SeqId::NecrumTension2_39, 160u},
    {SeqId::NecrumChase2_40, 640u},
    {SeqId::NecrumSafeTension_41, 160u},
    {SeqId::MudomoCalm3_42, 512u},
    {SeqId::MudomoCalm2_43, 512u},
    {SeqId::MudomoCalm1_44, 512u},
    {SeqId::MudomoTension1_45, 256u},
    {SeqId::MudomoChase1_46, 512u},
    {SeqId::MudomoPossess_47, 256u},
    {SeqId::MudomoTension2_48, 160u},
    {SeqId::MudomoChase2_49, 640u},
    {SeqId::MudomoSafeTension_50, 160u},
    {SeqId::MudomoEnderCalm_51, 448u},
    {SeqId::MudomoEnderTension1_52, 224u},
    {SeqId::MudomoEnderChase1_54, 112u},
    {SeqId::MudomoEnderPossess_53, 448u},
    {SeqId::MudomoEnderTension2_55, 160u},
    {SeqId::MudomoEnderChase2_56, 640u},
    {SeqId::MudomoEnderSafeTension_57, 160u},
    {SeqId::MudancheeCalm3_58, 512u},
    {SeqId::MudancheeCalm2_59, 512u},
    {SeqId::MudancheeCalm1_60, 1024u},
    {SeqId::MudancheeTension1_61, 256u},
    {SeqId::MudancheeChase1_62, 1024u},
    {SeqId::MudancheePossess_63, 256u},
    {SeqId::MudancheeTension2_64, 160u},
    {SeqId::MudancheeChase2_65, 640u},
    {SeqId::MudancheeSafeTension_66, 160u},
    {SeqId::MudancheeEnderCalm_67, 448u},
    {SeqId::MudancheeEnderTension1_68, 224u},
    {SeqId::MudancheeEnderChase1_70, 112u},
    {SeqId::MudancheePossess_69, 448u},
    {SeqId::MudancheeTension2_71, 160u},
    {SeqId::MudancheeEnderChase2_72, 640u},
    {SeqId::MudancheeEnderSafeTension_73, 160u},
    {SeqId::FeecoCalm3_74, 320u},
    {SeqId::FeecoCalm2_75, 640u},
    {SeqId::FeecoCalm1_76, 640u},
    {SeqId::FeecoTension1_77, 320u},
    {SeqId::FeecoChase1_79, 320u},
    {SeqId::FeecoPossess_78, 320u},
    {SeqId::FeecoTension2_80, 160u},
    {SeqId::FeecoChase2_81, 640u},
    {SeqId::FeecoSafeTension_82, 160u},
    {SeqId::FeecoEnderCalm_83, 448u},
    {SeqId::FeecoEnderTension_84, 112u},
    {SeqId::FeecoEnderChase_86, 448u},
    {SeqId::FeecoEnderPossess_85, 224u},
    {SeqId::BarracksCalm3_87, 640u},
    {SeqId::BarracksCalm2_88, 640u},
    {SeqId::BarracksCalm1_89, 640u},
    {SeqId::BarracksTension_90, 320u},
    {SeqId::BarracksChase_92, 320u},
    {SeqId::BarracksPossess_91, 320u},
    {SeqId::BarracksEnderCalm_93, 448u},
    {SeqId::BarracksEnderTension_94, 112u},
    {SeqId::BarracksEnderChase_96, 448u},
    {SeqId::BarracksEnderPossess_95, 224u},
    {SeqId::BonewerkzCalm3_97, 320u},
    {SeqId::BonewerkzCalm2_98, 320u},
    {SeqId::BonewerkzCalm1_99, 320u},
    {SeqId::BonewerkzTension1_100, 320u},
    {SeqId::BonewerkzChase1_102, 320u},
    {SeqId::BonewerkzPossess_101, 320u},
    {SeqId::BonewerkzTension2_103, 160u},
    {SeqId::BonewerkzChase2_104, 640u},
    {SeqId::BonewerkzSafeTension_105, 160u},
    {SeqId::BonewerkzEnderCalm_106, 448u},
    {SeqId::BonewerkzEnderTension1_107, 112u},
    {SeqId::BonewerkzEnderChase1_109, 448u},
    {SeqId::BonewerkzEnderPossess_108, 224u},
    {SeqId::BonewerkzEnderTension2_110, 160u},
    {SeqId::BonewerkzEnderChase2_111, 640u},
    {SeqId::BonewerkzEnderSafeTension_112, 160u},
    {SeqId::BreweryCalm3_113, 320u},
    {SeqId::BreweryCalm2_114, 320u},
    {SeqId::BreweryCalm1_115, 320u},
    {SeqId::BreweryTension1_116, 320u},
    {SeqId::BreweryChase1_118, 320u},
    {SeqId::BreweryPossess_117, 320u},
    {SeqId::BreweryTension2_119, 160u},
    {SeqId::BreweryChase2_120, 640u},
    {SeqId::BrewerySafeTension_121, 160u},
    {SeqId::BoilerCalm3_122, 512u},
    {SeqId::BoilerCalm2_123, 512u},
    {SeqId::BoilerCalm1_124, 512u},
    {SeqId::BoilerTension_125, 320u},
    {SeqId::BoilerChase_127, 320u},
    {SeqId::BoilerPossess_126, 320u},
    {SeqId::MinesCalm3_128, 320u},
    {SeqId::MinesCalm2_129, 1280u},
    {SeqId::MinesCalm1_130, 320u},
    {SeqId::MinesTension1_131, 320u},
    {SeqId::MinesChase1_133, 320u},
    {SeqId::MinesPossess_132, 320u},
    {SeqId::MenuMainTheme1_134, 256u},
    {SeqId::MenuMainTheme2_135, 2048u},
    {SeqId::MenuMainTheme3_136, 2048u},
    {SeqId::MenuBeginTheme2_137, 256u},
    {SeqId::MenuSubTheme_138, 2048u},
    {SeqId::MenuBeginTheme1_139, 256u},
    {SeqId::MinesTension2_140, 160u},
    {SeqId::MinesChase2_141, 640u},
    {SeqId::MinesSafeTension_142, 160u},
    {SeqId::MinesChase2_141, 640u},
    {SeqId::MainMenuAmbient_0, 0u}};


s32 MusicController::Create(ResourceManagerWrapper& resMan, BaseMap& map)
{
    if (sMusicController)
    {
        return 0;
    }

    sMusicController = relive_new MusicController(resMan, map);
    if (sMusicController)
    {
        MusicController::SetBaseTimeStamp();
        SoundDebug::LogSeqCall("CREATE");
        SoundDebug::MusicScope scope;
        sMusicController->EnableMusic(false);
    }
    return 1;
}

void MusicController::SetBaseTimeStamp()
{
    sMusicControllerBaseTimeStamp = SoundDebug::MusicTicks(GetGameAutoPlayer().SysGetTicks());
}

void MusicController::UpdateMusicTime()
{
    const u32 elapsedMs = SoundDebug::MusicTicks(GetGameAutoPlayer().SysGetTicks()) - sMusicControllerBaseTimeStamp;
    // Half a unit per vblank (the PS1 reads its vblank counter shifted right by one)
    sMusicTime = (PsxSoundEngine::kVblankHz * elapsedMs) / 2000;
}

void MusicController::Shutdown()
{
    SoundDebug::MusicScope scope;
    SoundDebug::LogSeqCall("SHUTDOWN");
    if (sMusicController)
    {
        sMusicController->SetDead(true);
        sMusicController = nullptr;
        //nullsub_8(); // TODO: Check if PSX stub
        //sub_4FBAD0(dword_5C3028); // Some other likely strange PSX specific thing that does nothing
        //nullsub_27(); // TODO: Check if PSX stub
    }
}

MusicController::MusicTypes MusicController::GetMusicType(u16* seq, u16* seq2, u32* seqTime)
{
    MusicController::UpdateMusicTime();
    if (seq)
    {
        *seq = field_2C_flags_and_seq_idx;
    }

    if (seq2)
    {
        *seq2 = field_40_flags_and_idx;
    }

    if (seqTime)
    {
        if (field_42_type != MusicTypes::eNone_0)
        {
            *seqTime = field_30_music_time - sMusicTime;
            return field_42_type;
        }
        *seqTime = field_44 - sMusicTime;
    }

    return field_42_type;
}

MusicController::MusicTypes MusicController::static_GetMusicType(u16* seq1, u16* seq2, u32* seqTime)
{
    if (sMusicController)
    {
        return sMusicController->GetMusicType(seq1, seq2, seqTime);
    }
    return MusicTypes::eTypeNull;
}

MusicController::MusicController(ResourceManagerWrapper& resMan, BaseMap& map)
    : BaseGameObject(true, 0, resMan, map)
{
    SetSurviveDeathReset(true);
    field_40_flags_and_idx = -1;
    field_24_currentLevelID = EReliveLevelIds::eNone;
    field_28_object_id = Guid{};
    field_2C_flags_and_seq_idx = -1;
    field_42_type = MusicTypes::eNone_0;
    field_30_music_time = 0;

    mMusicDead = false;
    mMusicEnabled = true;
    mScreenChanged = true;
    mUnknown4 = true;
    mAmbientMusicEnabled = true;

    field_34_music_start_time = 0;
    field_48_last_music_frame = 0;
    field_4C_state = 0;

    field_4E_starting_volume = 0;
    field_50_current_vol = 0;
    field_52_target_volume = 0;
    field_54_music_volume_change_time = 0;

    SetType(ReliveTypes::eMusicController);

    field_20_vol = 100;
    field_22_vol = 127;
}


MusicController::~MusicController()
{
    // Whatever is destroyed after this one (a movie stopping all sound as it goes) finds no controller
    if (sMusicController == this)
    {
        sMusicController = nullptr;
    }
    if (field_40_flags_and_idx > 0)
    {
        SND_SEQ_Stop(field_40_flags_and_idx);
    }
}

void MusicController::EnableMusic(s16 bEnable)
{
    bool enableMusic = bEnable != 0 ? true : false;
    MusicController::UpdateMusicTime();

    // If enable flag has changed
    if (mMusicEnabled != enableMusic)
    {
        mMusicEnabled = !mMusicEnabled;

        // Is it enabled?
        if (mMusicEnabled)
        {
            // Yes enable volume
            SetMusicVolumeDelayed(field_20_vol, 0);

            field_44 = 0;
            field_30_music_time = 0;
            field_48_last_music_frame = sMusicTime;

            if (field_42_type == MusicTypes::eTension_4 || field_42_type == MusicTypes::eIntenseChase_7 || field_42_type == MusicTypes::ePossessed_9)
            {
                mUnPause = true;
            }
        }
        else
        {
            // No so turn off volume to disable music
            SetMusicVolumeDelayed(0, 0);
        }
        UpdateVolumeState();
    }
}

void MusicController::SetMusicVolumeDelayed(s16 vol, s16 delay)
{
    if (vol != field_50_current_vol)
    {
        field_4E_starting_volume = field_50_current_vol;
        field_52_target_volume = (vol <= 0) ? 0 : vol; // Clamp to 0
        field_34_music_start_time = sMusicTime;
        field_54_music_volume_change_time = sMusicTime + delay;

        if (vol)
        {
            // vol state on?
            field_4C_state = 3;
        }
        else
        {
            // vol state off ?
            field_4C_state = 2;
        }
    }
}

void MusicController::UpdateVolumeState()
{
    switch (field_4C_state)
    {
        case 0:
            field_30_music_time = 0;
            field_34_music_start_time = sMusicTime;
            field_48_last_music_frame = sMusicTime;
            break;

        case 1:
            // volume off ?
            break;

        case 2:
            field_4C_state = 0;
            field_50_current_vol = 0;

            if (field_2C_flags_and_seq_idx > 0)
            {
                SND_SEQ_Stop(field_2C_flags_and_seq_idx);
                field_2C_flags_and_seq_idx = -1;
            }

            if (field_40_flags_and_idx > 0)
            {
                SND_SEQ_Stop(field_40_flags_and_idx);
                field_40_flags_and_idx = -1;
            }
            break;

        case 3:
            // change volume ?
            field_50_current_vol = field_52_target_volume;
            field_4C_state = 1; // Playing ?

            if (field_2C_flags_and_seq_idx > 0)
            {
                SND_SEQ_SetVol(field_2C_flags_and_seq_idx, field_50_current_vol, field_50_current_vol);
            }

            if (field_40_flags_and_idx > 0)
            {
                SND_SEQ_SetVol(field_40_flags_and_idx, field_50_current_vol, field_50_current_vol);
            }
            break;

        default:
            break;
    }
}

void MusicController::VScreenChanged()
{
    mScreenChanged = true;
}

void MusicController::VUpdate()
{
    MusicController::UpdateMusicTime();
    SoundDebug::MusicScope scope;
    SoundDebug::LogSeqCall("VUPDATE %d %d %d %d", static_cast<s32>(MapWrapper::ToAE(mMap.mCurrentLevel)), mScreenChanged ? 1 : 0, EventGet(Event::kEventDeathReset) ? 1 : 0, SoundDebug::ObjectOrdinal(gAbe));

    if (EventGet(Event::kEventDeathReset))
    {
        mMusicDead = true;
        field_28_object_id = gAbe->mBaseGameObjectId;
    }

    if (mScreenChanged)
    {
        mScreenChanged = false;

        if (mMap.mCurrentLevel != field_24_currentLevelID)
        {
            mBeatPeriod = 1; // The PS1 changes the music on the next update
            field_44 = 0;
            field_30_music_time = 0;
            mMusicDead = false;
            mUnknown4 = true;
            field_48_last_music_frame = sMusicTime;
            field_28_object_id = Guid{};

            if (field_2C_flags_and_seq_idx > 0)
            {
                SND_SEQ_Stop(field_2C_flags_and_seq_idx);
                field_2C_flags_and_seq_idx = -1;
            }

            if (field_40_flags_and_idx > 0)
            {
                SND_SEQ_Stop(field_40_flags_and_idx);
                field_40_flags_and_idx = -1;
            }

            field_24_currentLevelID = mMap.mCurrentLevel;

            if (mMusicEnabled)
            {
                SetMusicVolumeDelayed(field_20_vol, 0);
                PlayMusic(MusicTypes::eNone_0, 0, 1, 0);
            }
        }
    }

    if (field_42_type > MusicTypes::eType1 && sMusicTime - field_48_last_music_frame >= 160)
    {
        PlayMusic(MusicTypes::eNone_0, 0, 1, 0);
    }

    UpdateVolumeState();

    if (mMusicEnabled)
    {
        UpdateMusic();
        UpdateAmbiance();
    }
}

void MusicController::PlayMusic(MusicTypes typeToSet, const BaseGameObject* pObj, s16 bFlag4, s8 bFlag0x20)
{
    MusicController::UpdateMusicTime();

    if (typeToSet <= MusicTypes::eType1 || pObj)
    {
        const bool ownerAlive = sObjectIds.Find_Impl(field_28_object_id) != nullptr;
        SoundDebug::LogSeqCall("FIND %d", ownerAlive ? 1 : 0);
        if (!ownerAlive)
        {
            field_28_object_id = Guid{};
        }

        if (field_42_type == typeToSet)
        {
            if (typeToSet != MusicTypes::eNone_0)
            {
                field_48_last_music_frame = sMusicTime;
            }

            if (pObj && field_28_object_id != Guid{} && field_28_object_id == pObj->mBaseGameObjectId)
            {
                mMusicDead = false;
                mMusicDead = (bFlag4 & 1) ? true : false;
            }

            if (!(mUnPause))
            {
                mUnPause = false;
                mUnPause = (bFlag0x20 & 1) ? true : false;
            }
            return;
        }

        if (!pObj)
        {
            if (mMusicDead)
            {
                return;
            }

            if (bFlag4)
            {
                field_28_object_id = Guid{};
            }

            mUnPause = true;
            field_48_last_music_frame = sMusicTime;
            field_42_type = typeToSet;
            field_44 = 0;
        }
        else if (pObj->mBaseGameObjectId == field_28_object_id || field_28_object_id == Guid{} || (!mMusicDead && (bFlag4 || typeToSet >= field_42_type)))
        {
            field_28_object_id = pObj->mBaseGameObjectId;
            mMusicDead = false;
            mMusicDead = (bFlag4 & 1) ? true : false;
            mUnPause = true;
            field_48_last_music_frame = sMusicTime;
            field_42_type = typeToSet;
            field_44 = 0;
        }
    }
}

// The beat period when no music type picks one, per level (from the PS1 executable)
static const s8 kPs1DefaultBeat[17] = {16, 20, 16, 16, 16, 20, 20, 28, 20, 20, 16, 28, 28, 28, 28, 20, 16};

// The PS1's UpdateMusic (checked against SLES_014.80 0x80046284). It changes the music only when
// the time since the last change is a multiple of the beat period (set by each music type, from
// the tables), so a change waits for the next beat; a music type that was just asked for (mUnPause)
// can also go ahead when it's one of the ones that don't wait.
void MusicController::UpdateMusic()
{
    const s32 now = static_cast<s32>(sMusicTime);
    const s32 level = static_cast<s32>(MapWrapper::ToAE(field_24_currentLevelID));

    SoundDebug::LogSeqCall("DBG %d %d %d %d %d %d", now, mBeatOrigin, mBeatPeriod, static_cast<s32>(field_44), static_cast<s32>(field_42_type), mUnPause ? 1 : 0);
    bool go = false;
    if (now >= static_cast<s32>(field_44) && mBeatPeriod != 0)
    {
        go = ((now - mBeatOrigin) % mBeatPeriod) == 0;
    }

    if (!go)
    {
        if (!mUnPause)
        {
            return;
        }
        switch (field_42_type)
        {
            case MusicTypes::eChime_2:
            case MusicTypes::eDrumAmbience_3:
            case MusicTypes::eIntenseChase_7:
            case MusicTypes::eSoftChase_8:
            case MusicTypes::eDeathDrumShort_10:
            case MusicTypes::eDeathLong_11:
            case MusicTypes::eSecretAreaShort_12:
            case MusicTypes::eSecretAreaLong_13:
                break;
            default:
                return;
        }
    }

    if (field_40_flags_and_idx >= 0)
    {
        SND_SEQ_Stop(field_40_flags_and_idx);
    }

    s32 idx = -1;
    const MusicController_Record* pTable = nullptr;
    switch (field_42_type)
    {
        case MusicTypes::eChime_2:
            mBeatPeriod = 1;
            mAmbientMusicEnabled = false;
            SetMusicVolumeDelayed(field_22_vol, 0);
            break;
        case MusicTypes::eDrumAmbience_3:
            idx = mUnPause ? MusicRandom(0, 1) : -1;
            mBeatPeriod = 1;
            mAmbientMusicEnabled = false;
            SetMusicVolumeDelayed(field_22_vol, 0);
            break;
        case MusicTypes::eDeathDrumShort_10:
            idx = mUnPause ? 2 : -1;
            mBeatPeriod = 1;
            mAmbientMusicEnabled = false;
            SetMusicVolumeDelayed(field_22_vol, 0);
            break;
        case MusicTypes::eDeathLong_11:
            idx = mUnPause ? 3 : -1;
            mBeatPeriod = 1;
            mAmbientMusicEnabled = false;
            SetMusicVolumeDelayed(field_22_vol, 0);
            break;
        case MusicTypes::eSecretAreaShort_12:
        case MusicTypes::eSecretAreaLong_13:
            idx = mUnPause ? (field_42_type == MusicTypes::eSecretAreaShort_12 ? 4 : 5) : -1;
            mBeatPeriod = 120;
            mAmbientMusicEnabled = false;
            SetMusicVolumeDelayed(127, 0);
            break;
        case MusicTypes::eTension_4:
            pTable = tension_55D2D0;
            break;
        case MusicTypes::eSlogTension_5:
            pTable = slogTension_55D39C;
            break;
        case MusicTypes::eSlogChaseTension_6:
            pTable = slogChaseTension_55D424;
            break;
        case MusicTypes::eIntenseChase_7:
            pTable = slogChase_55D3E0;
            break;
        case MusicTypes::eSoftChase_8:
            pTable = chase_55D314;
            break;
        case MusicTypes::ePossessed_9:
            if (mUnPause)
            {
                pTable = possessed_55D358;
            }
            else
            {
                SetMusicVolumeDelayed(field_20_vol, 30);
                mBeatPeriod = 20;
                mAmbientMusicEnabled = true;
                mUnknown7 = true;
            }
            break;
        default:
            SetMusicVolumeDelayed(field_20_vol, 30);
            mBeatPeriod = kPs1DefaultBeat[level];
            mAmbientMusicEnabled = true;
            break;
    }

    if (pTable)
    {
        const MusicController_Record& rec = pTable[level];
        idx = rec.field_0_seq_id_idx;
        mBeatPeriod = rec.field_1_unused;
        mAmbientMusicEnabled = rec.field_2_bAmbient_music_enabled == AmbientMusic::eOn;
        SetMusicVolumeDelayed(gSeqData.mSeqs[stru_55D008[idx < 0 ? 0 : idx].field_0_idx].field_9_volume, 0);
    }

    if (idx >= 0)
    {
        field_40_flags_and_idx = stru_55D008[idx].field_0_idx;
        field_44 = sMusicTime + stru_55D008[idx].field_2_duration;
        SND_SEQ_Play(field_40_flags_and_idx, 1, field_50_current_vol, field_50_current_vol);
    }
    else
    {
        field_40_flags_and_idx = -1;
        field_44 = 0;
    }

    mBeatOrigin = now;

    if (mUnPause)
    {
        if (mUnknown7)
        {
            mUnknown7 = false;
        }
        else
        {
            mUnknown4 = true;
        }
        mUnPause = false;
    }
}

void MusicController::UpdateAmbiance()
{
    if (mAmbientMusicEnabled || (field_2C_flags_and_seq_idx < 0))
    {
        if (mUnknown4)
        {
            field_30_music_time = 0;
            mUnknown4 = false;
            field_34_music_start_time = sMusicTime;
        }

        s32 musicTime = sMusicTime;
        if (sMusicTime >= field_30_music_time && mAmbientMusicEnabled)
        {
            if (field_2C_flags_and_seq_idx > 0)
            {
                SND_SEQ_Stop(field_2C_flags_and_seq_idx);
                musicTime = sMusicTime;
            }

            s16 random = 0; // TODO: minus 1 ??
            // TODO: Check this logic, seems to have changed a lot since AO so might
            // be bugged
            if (field_42_type != MusicTypes::eNone_0)
            {
                random = MusicRandom(
                    stru_55D1E0[static_cast<s32>(MapWrapper::ToAE(field_24_currentLevelID))].field_0[0].field_2_min,
                    stru_55D1E0[static_cast<s32>(MapWrapper::ToAE(field_24_currentLevelID))].field_0[0].field_3_max);
            }
            else
            {
                const s32 calculatedTime = musicTime - field_34_music_start_time;
                s32 total = 0;
                bool found = false;
                for (const MusicController_Record3_Sub& rec : stru_55D1E0[static_cast<s32>(MapWrapper::ToAE(field_24_currentLevelID))].field_0)
                {
                    total += rec.field_0;
                    if (rec.field_0 < 0 || calculatedTime < total)
                    {
                        random = MusicRandom(rec.field_2_min, rec.field_3_max);
                        found = true;
                        break;
                    }
                }

                if (!found)
                {
                    field_2C_flags_and_seq_idx = -1;
                    field_30_music_time = sMusicTime;
                    return;
                }
            }

            if (random < 0)
            {
                field_2C_flags_and_seq_idx = -1;
                field_30_music_time = sMusicTime;
            }
            else
            {
                field_2C_flags_and_seq_idx = stru_55D008[random].field_0_idx;
                SND_SEQ_Play(field_2C_flags_and_seq_idx, stru_55D1E0[static_cast<s32>(MapWrapper::ToAE(field_24_currentLevelID))].field_C_repeat_count, field_50_current_vol, field_50_current_vol);
                field_30_music_time = sMusicTime + stru_55D008[random].field_2_duration;
            }
        }
    }
    else
    {
        SND_SEQ_Stop(field_2C_flags_and_seq_idx);
        field_2C_flags_and_seq_idx = -1;
    }
}

void MusicController::static_PlayMusic(MusicTypes typeToSet, const BaseGameObject* pObj, s16 bFlag4, s8 bFlag0x20)
{
    SoundDebug::MusicScope scope;
    SoundDebug::LogSeqCall("MUSIC %d %d %d %d", static_cast<s32>(typeToSet), SoundDebug::ObjectOrdinal(pObj), bFlag4, bFlag0x20);
    if (sMusicController)
    {
        sMusicController->PlayMusic(typeToSet, pObj, bFlag4, bFlag0x20);
    }
}

void MusicController::static_EnableMusic(s16 bOn)
{
    SoundDebug::MusicScope scope;
    SoundDebug::LogSeqCall("ENABLE %d", bOn);
    if (sMusicController)
    {
        sMusicController->EnableMusic(bOn);
    }
}
