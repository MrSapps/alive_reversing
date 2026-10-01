#include "../stdafx.h"
#include "SoundGold.hpp"
#include "Midi.hpp"
#include "PsxSpuApi.hpp"
#include "Sound.hpp"
#include "SDLSoundBuffer.hpp"
#include "SDLSoundSystem.hpp"
#include "PsxSoundEngine.hpp"
#include "../BinaryPath.hpp"
#include "../FatalError.hpp"
#include "../../AliveLibAE/PathData.hpp"
#include "../../AliveLibAO/Midi.hpp"
#include <cstdarg>
#include <cstdio>

namespace AO {
extern OpenSeqHandle g_SeqTable_4C9E70[165];
}

extern u8 sRandomSeed;

static constexpr u32 kStepMs = 10;

static OpenSeqHandle* SeqTable(GameType game)
{
    return game == GameType::eAo ? AO::g_SeqTable_4C9E70 : gSeqData.mSeqs;
}

static std::string Format(const char_type* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char_type buf[512] = {};
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    return buf;
}

SoundGoldSession::SoundGoldSession(GameType game, bool ps1Sound, PsxSpu::Interpolation interpolation)
    : mGame(game)
{
    if (sDSound_BBC344)
    {
        ALIVE_FATAL("SoundGoldSession: the sound system is already running");
    }

    SetGameType(game);

    // Whatever an earlier session left behind would change which voices get picked
    memset(sSoundBuffers_BBBAB8, 0, sizeof(sSoundBuffers_BBBAB8));
    sLastNotePlayTime_BBC33C = 0;
    sRandomSeed = 0;

    mOldCreateDS = GetSoundAPI().mSND_CreateDS;
    GetSoundAPI().mSND_CreateDS = SND_CreateDS_Offline;
    SDLSoundSystem::SetPs1SoundOnCreate(ps1Sound, interpolation);

    if (game == GameType::eAo)
    {
        AO::SND_Init();
    }
    else
    {
        SND_Init();
    }

    // SND_Shutdown leaves the MIDI channel and SEQ state of an earlier session behind, and the
    // clock starts at 0 again, so the last sequencer update time must be forgotten too
    IPsxSpuApiVars* pVars = GetSpuApiVars();
    memset(&pVars->sMidi_Channels(), 0, sizeof(MidiChannels));
    for (s32 i = 0; i < 32; i++)
    {
        memset(&pVars->sMidiSeqSongs(i), 0, sizeof(MIDI_SeqSong));
    }
    pVars->sControllerValue() = 0;

    pVars->sLastTime() = 0xFFFFFFFF;
    pVars->sMidi_WaitUntil() = 0;

    // Only record channels/voices once they change from this
    mLastChannelState.resize(kNumChannels, "off");
    if (ps1Sound)
    {
        mLastVoiceState.resize(PsxSpu::kNumVoices, "off");
    }
    else
    {
        mLastVoiceState.resize(ALIVE_COUNTOF(sSoundBuffers_BBBAB8), "stopped");
    }
}

SoundGoldSession::~SoundGoldSession()
{
    SND_Shutdown();
    GetMidiVars()->sMonkVh_Vb().mSoundTheme.clear();

    GetSoundAPI().mSND_CreateDS = mOldCreateDS;
    SDLSoundSystem::SetPs1SoundOnCreate(false);
    SND_Restart_SetCallBack(nullptr);
    SND_StopAll_SetCallBack(nullptr);
    SetSpuApiVars(nullptr);
    SetMidiApiVars(nullptr);
}

void SoundGoldSession::LoadSoundBlock(ResourceManagerWrapper& resMan, const std::shared_ptr<PathSoundInfo>& info, s32 reverb, const std::string& monkTheme)
{
    mInfo = info;

    // The game only loads MONK.VH/VB when mVabId is -1, but it starts as 0 (sMonkVh_Vb's
    // initialiser) until SND_Shutdown sets it, so the game never loads it before then. Without a
    // monk theme do the same, as the load would fail.
    GetMidiVars()->sMonkVh_Vb().mSoundTheme = monkTheme;
    GetMidiVars()->sMonkVh_Vb().mVabId = monkTheme.empty() ? 0 : -1;
    SND_Pend_Sound_Files(*info, resMan);

    // Only what the engine does with the files is recorded, not their names or bytes, so the
    // trace stays the same when the files change format
    Note(Format("load seqs=%d reverb=%d", static_cast<s32>(info->mSeqFiles.size()), reverb));

    SND_Load_VABS(mInfo, reverb, resMan);
    SND_Load_Seqs(SeqTable(mGame), mInfo, resMan);

    if (!monkTheme.empty())
    {
        RecordLoadedVab(GetMidiVars()->sMonkVh_Vb().mVabId);
    }
    RecordLoadedVab(info->mVabId);

    for (const u16 seqIdx : LoadedSeqs())
    {
        Note(Format("seq %d %s", seqIdx, SeqTable(mGame)[seqIdx].field_0_mBsqName));
    }
}

void SoundGoldSession::Note(const std::string& text)
{
    mTrace += Format("t=%u ", SND_GetTicks());
    mTrace += text;
    mTrace += "\n";
}

void SoundGoldSession::Advance(u32 ms)
{
    const u32 end = SND_GetTicks() + ms;
    while (SND_GetTicks() < end)
    {
        SsSeqCalledTbyT();
        RecordChanges();
        sDSound_BBC344->RenderOfflineUntil(std::min(SND_GetTicks() + kStepMs, end));
    }
    RecordChanges();
}

s32 SoundGoldSession::VabId() const
{
    return mInfo ? mInfo->mVabId : -1;
}

std::vector<u16> SoundGoldSession::LoadedSeqs() const
{
    std::vector<u16> ret;
    for (s32 i = 0; i < GetMidiVars()->MidiTableSize(); i++)
    {
        if (!SeqTable(mGame)[i].field_C_ppSeq_Data.empty())
        {
            ret.push_back(static_cast<u16>(i));
        }
    }
    return ret;
}

std::vector<s32> SoundGoldSession::Programs() const
{
    std::vector<s32> ret;
    const s32 vabId = VabId();
    if (vabId < 0)
    {
        return ret;
    }

    for (s32 prog = 0; prog < 128; prog++)
    {
        for (s32 tone = 0; tone < 16; tone++)
        {
            if (GetSpuApiVars()->sConvertedVagTable().table[vabId][prog][tone].field_D_vol > 0)
            {
                ret.push_back(prog);
                break;
            }
        }
    }
    return ret;
}

s32 SoundGoldSession::RootKey(s32 program) const
{
    for (s32 tone = 0; tone < 16; tone++)
    {
        const Converted_Vag& vag = GetSpuApiVars()->sConvertedVagTable().table[VabId()][program][tone];
        if (vag.field_D_vol > 0)
        {
            // centre * 256 - 2 * shift, with shift 0-127
            return (vag.field_A_shift_cen + 255) >> 8;
        }
    }
    return 60;
}

const std::vector<StereoSample_S16>& SoundGoldSession::Audio() const
{
    return sDSound_BBC344->OfflineOutput();
}

void SoundGoldSession::RecordLoadedVab(s32 vabId)
{
    if (vabId < 0)
    {
        return;
    }

    IPsxSpuApiVars* pVars = GetSpuApiVars();
    Note(Format("vab %d progs=%d vags=%d", vabId, pVars->sProgCounts()[vabId], pVars->sVagCounts()[vabId]));

    for (s32 prog = 0; prog < 128; prog++)
    {
        for (s32 tone = 0; tone < 16; tone++)
        {
            const Converted_Vag& v = pVars->sConvertedVagTable().table[vabId][prog][tone];
            if (v.field_D_vol == 0)
            {
                continue;
            }
            Note(Format("tone vab=%d prog=%d tone=%d vag=%d keys=%d-%d shift_cen=%d vol=%d prio=%d flags=%d adsr=%d,%d,%d,%d pan=%d",
                vabId, prog, tone, v.field_10_vag, v.field_8_min, v.field_9_max, v.field_A_shift_cen, v.field_D_vol, v.field_E_priority, v.field_C,
                v.field_0_adsr_attack, v.field_2_adsr_sustain_level, v.field_4_adsr_decay, v.field_6_adsr_release, v.field_11_pad));
        }
    }

    for (s32 i = 0; i < pVars->sVagCounts()[vabId]; i++)
    {
        const SoundEntry& e = pVars->sSoundEntryTable16().table[vabId][i];
        if (!e.field_8_pSoundBuffer)
        {
            Note(Format("sample vab=%d idx=%d none", vabId, i));
            continue;
        }
        Note(Format("sample vab=%d idx=%d rate=%d bits=%d stereo=%d bytes=%d crc=%08x", vabId, i, e.field_18_sampleRate, e.field_1C_bitsPerSample, e.field_20_isStereo,
            e.field_C_buffer_size_bytes, Crc32(e.field_8_pSoundBuffer, e.field_C_buffer_size_bytes)));
    }
}

void SoundGoldSession::RecordChanges()
{
    IPsxSpuApiVars* pVars = GetSpuApiVars();
    for (s32 i = 0; i < kNumChannels; i++)
    {
        const MIDI_Channel& c = pVars->sMidi_Channels().channels[i];
        const MIDI_ADSR_State& a = c.field_1C_adsr;
        std::string state;
        if (a.field_3_state != 0)
        {
            state = Format("state=%d vol=%d cvol=%d prio=%d vab=%d prog=%d note=%d freq=%.4f adsr=%d,%d,%d,%d seqch=%d ref=%d loop=%d",
                a.field_3_state, c.field_8_left_vol, c.field_C_vol, c.field_4_priority, a.field_0_seq_idx, a.field_1_program, a.field_2_note_byte1, static_cast<f64>(c.field_10_freq),
                a.field_4_attack, a.field_6_sustain, a.field_8_decay, a.field_A_release, a.field_C, a.field_E_ref_count, c.field_18_rightVol);
        }
        else
        {
            state = "off";
        }

        if (state != mLastChannelState[i])
        {
            Note(Format("ch%d ", i) + state);
            mLastChannelState[i] = state;
        }
    }

    if (PsxSoundEngine* pPs1Sound = PsxSoundEngine::Get())
    {
        for (s32 i = 0; i < PsxSpu::kNumVoices; i++)
        {
            const PsxSoundEngine::VoiceState v = pPs1Sound->GetVoiceState(i);
            std::string state = "off";
            if (v.mPhase != PsxSpuEnvelope::Phase::Off)
            {
                state = Format("phase=%d pitch=%04x vol=%d,%d adsr=%04x,%04x reverb=%d seq=%d prog=%d note=%d", static_cast<s32>(v.mPhase), v.mPitch, v.mVolLeft, v.mVolRight, v.mAdsr1,
                    v.mAdsr2, v.mReverb ? 1 : 0, v.mLibsnd.mSeq, v.mLibsnd.mProgram, v.mLibsnd.mNote);
            }

            if (state != mLastVoiceState[i])
            {
                Note(Format("spu%d ", i) + state);
                mLastVoiceState[i] = state;
            }
        }
        return;
    }

    for (s32 i = 0; i < static_cast<s32>(ALIVE_COUNTOF(sSoundBuffers_BBBAB8)); i++)
    {
        const SDLSoundBuffer* pVoice = sSoundBuffers_BBBAB8[i].field_0_pDSoundBuffer;
        std::string state;
        if (pVoice && pVoice->mState.eStatus == SDLSoundBufferStatus::Playing)
        {
            state = Format("sample=%d vol=%d freq=%u pan=%d loop=%d", sSoundBuffers_BBBAB8[i].field_8_sample_idx, pVoice->mState.iVolumeTarget,
                static_cast<u32>(pVoice->mState.fFrequency * pVoice->mSoundSysFreq), pVoice->mState.iPan, pVoice->mState.bLoop ? 1 : 0);
        }
        else
        {
            state = "stopped";
        }

        if (state != mLastVoiceState[i])
        {
            Note(Format("voice%d ", i) + state);
            mLastVoiceState[i] = state;
        }
    }
}

std::vector<u8> SoundGoldSession::ToWav(const std::vector<StereoSample_S16>& audio)
{
    const u32 dataBytes = static_cast<u32>(audio.size() * sizeof(StereoSample_S16));
    std::vector<u8> wav;
    auto put = [&](const void* p, size_t n)
    {
        const u8* b = static_cast<const u8*>(p);
        wav.insert(wav.end(), b, b + n);
    };
    auto put32 = [&](u32 v)
    {
        const u8 b[4] = {static_cast<u8>(v), static_cast<u8>(v >> 8), static_cast<u8>(v >> 16), static_cast<u8>(v >> 24)};
        put(b, 4);
    };
    auto put16 = [&](u16 v)
    {
        const u8 b[2] = {static_cast<u8>(v), static_cast<u8>(v >> 8)};
        put(b, 2);
    };

    put("RIFF", 4);
    put32(36 + dataBytes);
    put("WAVE", 4);
    put("fmt ", 4);
    put32(16);
    put16(1);          // PCM
    put16(2);          // channels
    put32(44100);      // sample rate
    put32(44100 * 4);  // bytes per second
    put16(4);          // block align
    put16(16);         // bits per sample
    put("data", 4);
    put32(dataBytes);
    for (const StereoSample_S16& s : audio)
    {
        put16(static_cast<u16>(s.left));
        put16(static_cast<u16>(s.right));
    }
    return wav;
}

u32 SoundGoldSession::Crc32(const u8* pData, size_t len)
{
    u32 crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++)
    {
        crc ^= pData[i];
        for (s32 bit = 0; bit < 8; bit++)
        {
            crc = (crc >> 1) ^ (0xEDB88320 & (0u - (crc & 1)));
        }
    }
    return ~crc;
}
