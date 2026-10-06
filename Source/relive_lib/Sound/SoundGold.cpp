#include "../stdafx.h"
#include "SoundGold.hpp"
#include "Midi.hpp"
#include "GameSound.hpp"
#include "SDLSoundSystem.hpp"
#include "PsxSoundEngine.hpp"
#include "VabFile.hpp"
#include "../BinaryPath.hpp"
#include "../FatalError.hpp"
#include "../../AliveLibAE/PathData.hpp"
#include "../../AliveLibAO/Midi.hpp"
#include <algorithm>
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

// The tones of a VH file: the header, then 16 tones for each program. Unused slots have no volume.
static std::vector<VagAtr> ReadTones(const std::vector<u8>& vh, VabHeader& header)
{
    std::vector<VagAtr> tones;
    if (vh.size() < sizeof(VabHeader))
    {
        return tones;
    }
    memcpy(&header, vh.data(), sizeof(VabHeader));
    const size_t count = static_cast<size_t>(std::clamp<s32>(header.field_12_num_progs, 0, 128)) * 16;
    for (size_t i = 0; i < count && sizeof(VabHeader) + (i + 1) * sizeof(VagAtr) <= vh.size(); i++)
    {
        VagAtr tone = {};
        memcpy(&tone, vh.data() + sizeof(VabHeader) + i * sizeof(VagAtr), sizeof(VagAtr));
        tones.push_back(tone);
    }
    return tones;
}

SoundGoldSession::SoundGoldSession(GameType game, PsxSpu::Interpolation interpolation)
    : mGame(game)
{
    if (SDLSoundSystem::Get())
    {
        ALIVE_FATAL("SoundGoldSession: the sound system is already running");
    }

    SetGameType(game);
    sRandomSeed = 0;

    GameSoundConfig config;
    config.mGame = game;
    config.mOffline = true;
    config.mGaussianInterpolation = interpolation == PsxSpu::Interpolation::Gaussian;
    SND_Init(config);

    // The gold files are the hardware's output
    GameSound::Get()->Engine().SetDeclick(false);

    // Only record voices once they change from this
    mLastVoiceState.resize(PsxSpu::kNumVoices, "off");
}

SoundGoldSession::~SoundGoldSession()
{
    SND_Shutdown();
    SND_SsQuit();
}

void SoundGoldSession::LoadSoundBlock(ResourceManagerWrapper& resMan, const std::shared_ptr<PathSoundInfo>& info, s32 reverb)
{
    mInfo = info;

    SND_Pend_Sound_Files(*info, resMan);

    // Only what the engine does with the files is recorded, not their names or bytes, so the
    // trace stays the same when the files change format
    Note(Format("load seqs=%d reverb=%d", static_cast<s32>(info->mSeqFiles.size()), reverb));

    SND_Load_VABS(mInfo, reverb, resMan);
    SND_Load_Seqs(SeqTable(mGame), mInfo, resMan);

    RecordLoadedVab(*info);

    for (const u16 seqIdx : LoadedSeqs())
    {
        Note(Format("seq %d %s", seqIdx, SeqTable(mGame)[seqIdx].field_0_mBsqName));
    }
}

void SoundGoldSession::Note(const std::string& text)
{
    mTrace += Format("t=%u ", SDLSoundSystem::GetTicks());
    mTrace += text;
    mTrace += "\n";
}

void SoundGoldSession::Advance(u32 ms)
{
    SDLSoundSystem& sound = GameSound::Get()->System();
    const u32 end = SDLSoundSystem::GetTicks() + ms;
    while (SDLSoundSystem::GetTicks() < end)
    {
        RecordChanges();
        sound.RenderOfflineUntil(std::min(SDLSoundSystem::GetTicks() + kStepMs, end));
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
    for (s32 i = 0; i < GameSound::Get()->SeqTableSize(); i++)
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
    if (!mInfo)
    {
        return ret;
    }

    VabHeader header = {};
    for (const VagAtr& tone : ReadTones(mInfo->mVhFileData, header))
    {
        if (tone.field_2_vol > 0 && std::find(ret.begin(), ret.end(), tone.field_14_prog) == ret.end())
        {
            ret.push_back(tone.field_14_prog);
        }
    }
    std::sort(ret.begin(), ret.end());
    return ret;
}

s32 SoundGoldSession::RootKey(s32 program) const
{
    if (mInfo)
    {
        VabHeader header = {};
        for (const VagAtr& tone : ReadTones(mInfo->mVhFileData, header))
        {
            if (tone.field_2_vol > 0 && tone.field_14_prog == program)
            {
                return tone.field_4_centre;
            }
        }
    }
    return 60;
}

const std::vector<StereoSample_S16>& SoundGoldSession::Audio() const
{
    return GameSound::Get()->System().OfflineOutput();
}

void SoundGoldSession::RecordLoadedVab(const PathSoundInfo& info)
{
    const s32 vabId = info.mVabId;
    if (vabId < 0)
    {
        return;
    }

    VabHeader header = {};
    const std::vector<VagAtr> tones = ReadTones(info.mVhFileData, header);
    const s32 vagCount = std::min<s32>(header.field_16_num_vags, 255);
    Note(Format("vab %d progs=%d vags=%d", vabId, header.field_12_num_progs, vagCount));

    for (size_t i = 0; i < tones.size(); i++)
    {
        const VagAtr& t = tones[i];
        if (t.field_2_vol == 0)
        {
            continue;
        }
        Note(Format("tone vab=%d prog=%d tone=%d vag=%d keys=%d-%d centre=%d shift=%d vol=%d prio=%d mode=%d adsr=%04x,%04x pan=%d",
            vabId, t.field_14_prog, static_cast<s32>(i % 16), t.field_16_vag - 1, t.field_6_min, t.field_7_max, t.field_4_centre, t.field_5_shift, t.field_2_vol,
            t.field_0_priority, t.field_1_mode, static_cast<u16>(t.field_10_adsr1), static_cast<u16>(t.field_12_adsr2), t.field_3_pan));
    }

    for (s32 i = 0; i < vagCount; i++)
    {
        const PsxSoundEngine::SampleInfo sample = GameSound::Get()->Engine().GetSample(vabId, i);
        if (sample.mPcm.empty())
        {
            Note(Format("sample vab=%d idx=%d none", vabId, i));
            continue;
        }
        Note(Format("sample vab=%d idx=%d samples=%u loop=%d loopstart=%u crc=%08x", vabId, i, static_cast<u32>(sample.mPcm.size()), sample.mLoop ? 1 : 0, sample.mLoopStart,
            Crc32(reinterpret_cast<const u8*>(sample.mPcm.data()), sample.mPcm.size() * sizeof(s16))));
    }
}

void SoundGoldSession::RecordChanges()
{
    PsxSoundEngine& engine = GameSound::Get()->Engine();
    for (s32 i = 0; i < PsxSpu::kNumVoices; i++)
    {
        const PsxSoundEngine::VoiceState v = engine.GetVoiceState(i);
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
