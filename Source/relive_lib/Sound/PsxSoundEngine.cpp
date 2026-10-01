#include "../stdafx.h"
#include "PsxSoundEngine.hpp"
#include "PsxSpuApi.hpp"
#include "SDLSoundSystem.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>

// Frames rendered per SPU Render call in Mix
static constexpr u32 kMixChunkFrames = 512;

// A libsnd pan (0-127, 64 = centre) applied to a left/right volume pair
static void ApplyPan(s32 pan, s32& left, s32& right)
{
    if (pan < 64)
    {
        right = right * pan / 63;
    }
    else
    {
        left = left * (127 - pan) / 63;
    }
}

PsxSoundEngine::VoiceVolume PsxSoundEngine::LibsndVoiceVolume(bool seqNote, bool squared, s32 velocity, s32 vabVol, s32 progVol, s32 toneVol, s32 tonePan, s32 progPan, s32 channelPan,
    s32 seqVolLeft, s32 seqVolRight)
{
    // Checked against the real code run in a MIPS interpreter: the same result for all of 3000
    // random inputs. The divisions round down (the inputs are never negative).
    s32 vol = velocity * vabVol * 0x3FFF / (127 * 127);
    vol = vol * progVol * toneVol / (127 * 127);

    VoiceVolume v;
    v.mLeft = seqNote ? vol * seqVolLeft / 127 : vol;
    v.mRight = seqNote ? vol * seqVolRight / 127 : vol;
    ApplyPan(tonePan, v.mLeft, v.mRight);
    ApplyPan(progPan, v.mLeft, v.mRight);
    ApplyPan(channelPan, v.mLeft, v.mRight);

    if (squared)
    {
        v.mLeft = v.mLeft * v.mLeft / 0x3FFF;
        v.mRight = v.mRight * v.mRight / 0x3FFF;
    }
    return v;
}

void PsxSoundEngine::SfxVelocityAndPan(s32 volLeft, s32 volRight, s32& velocity, s32& pan)
{
    if (volLeft == volRight)
    {
        velocity = volLeft;
        pan = 64;
    }
    else if (volRight < volLeft)
    {
        velocity = volLeft;
        pan = volRight * 64 / volLeft;
    }
    else
    {
        velocity = volRight;
        pan = 127 - volLeft * 64 / volRight;
    }
}

// 0-127 to a fixed SPU volume
static s16 VolumeRegister(s32 vol)
{
    return static_cast<s16>(std::clamp(vol, 0, 127) * 0x7FFE / 127);
}

// A tone whose priority and volume the PC data changed: the PC values (and the tone's keys and
// sample, to be sure it's the same tone), then the PS1 ones. Made by comparing every VH of the PS1
// and PC versions; AE's are all the same.
struct Ps1ToneValues final
{
    GameType mGame;
    const char_type* mVhFile;
    u8 mProgram;
    u8 mTone; // Slot 0-15 in the program
    u8 mMinKey;
    u8 mMaxKey;
    u8 mVag;
    u8 mPcPriority;
    u8 mPcVol;
    u8 mPs1Priority;
    u8 mPs1Vol;
};

static const Ps1ToneValues kPs1ToneValues[] = {
    {GameType::eAo, "D1SNDFX.VH", 81, 0, 0, 127, 41, 70, 70, 127, 127},
    {GameType::eAo, "D1SNDFX.VH", 82, 0, 0, 126, 69, 95, 95, 127, 127},
    {GameType::eAo, "D1SNDFX.VH", 82, 1, 0, 126, 69, 95, 95, 91, 127},
    {GameType::eAo, "D1SNDFX.VH", 85, 0, 0, 127, 69, 95, 95, 101, 127},
    {GameType::eAo, "D2ENDER.VH", 81, 0, 0, 127, 18, 70, 70, 127, 127},
    {GameType::eAo, "D2ENDER.VH", 82, 0, 0, 127, 42, 95, 95, 101, 127},
    {GameType::eAo, "D2ENDER.VH", 85, 0, 0, 127, 42, 95, 95, 101, 127},
    {GameType::eAo, "D2SNDFX.VH", 81, 0, 0, 127, 51, 70, 70, 127, 127},
    {GameType::eAo, "D2SNDFX.VH", 82, 0, 0, 127, 85, 95, 95, 101, 127},
    {GameType::eAo, "D2SNDFX.VH", 85, 0, 0, 127, 85, 95, 95, 101, 127},
    {GameType::eAo, "E1SNDFX.VH", 74, 0, 0, 127, 51, 100, 100, 0, 127},
    {GameType::eAo, "E1SNDFX.VH", 76, 0, 0, 127, 51, 100, 100, 79, 127},
    {GameType::eAo, "E1SNDFX.VH", 81, 0, 0, 127, 51, 70, 70, 127, 127},
    {GameType::eAo, "E2SNDFX.VH", 76, 0, 0, 127, 52, 100, 100, 79, 127},
    {GameType::eAo, "E2SNDFX.VH", 81, 0, 0, 127, 52, 70, 70, 127, 127},
    {GameType::eAo, "F1SNDFX.VH", 81, 0, 0, 127, 92, 70, 70, 127, 127},
    {GameType::eAo, "F2ENDER.VH", 81, 0, 0, 127, 27, 70, 70, 127, 127},
    {GameType::eAo, "F2SNDFX.VH", 7, 3, 36, 43, 6, 40, 40, 2, 127},
    {GameType::eAo, "F2SNDFX.VH", 7, 4, 44, 45, 10, 40, 40, 2, 127},
    {GameType::eAo, "F2SNDFX.VH", 7, 5, 46, 46, 11, 40, 40, 2, 127},
    {GameType::eAo, "F2SNDFX.VH", 7, 6, 47, 47, 26, 60, 60, 2, 127},
    {GameType::eAo, "F2SNDFX.VH", 7, 7, 48, 48, 27, 60, 60, 2, 127},
    {GameType::eAo, "F2SNDFX.VH", 7, 8, 49, 55, 40, 60, 60, 2, 127},
    {GameType::eAo, "F2SNDFX.VH", 7, 9, 56, 57, 49, 40, 40, 2, 127},
    {GameType::eAo, "F2SNDFX.VH", 7, 10, 65, 67, 53, 40, 40, 2, 127},
    {GameType::eAo, "F2SNDFX.VH", 7, 12, 69, 75, 59, 40, 40, 2, 127},
    {GameType::eAo, "F2SNDFX.VH", 81, 0, 0, 127, 46, 70, 70, 127, 127},
    {GameType::eAo, "MLSNDFX.VH", 81, 0, 0, 127, 130, 70, 70, 127, 127},
    {GameType::eAo, "OPTSNDFX.VH", 2, 0, 0, 127, 9, 70, 70, 79, 127},
    {GameType::eAo, "RFENDER.VH", 49, 1, 61, 61, 67, 127, 127, 64, 127},
    {GameType::eAo, "RFENDER.VH", 67, 0, 60, 60, 103, 127, 127, 127, 100},
    {GameType::eAo, "RFENDER.VH", 67, 1, 61, 61, 104, 127, 127, 127, 100},
    {GameType::eAo, "RFENDER.VH", 67, 2, 62, 62, 105, 127, 127, 127, 100},
    {GameType::eAo, "RFENDER.VH", 67, 3, 63, 63, 106, 127, 127, 127, 100},
    {GameType::eAo, "RFENDER.VH", 81, 0, 0, 127, 36, 70, 70, 127, 127},
    {GameType::eAo, "RFSNDFX.VH", 7, 1, 36, 48, 95, 60, 60, 2, 127},
    {GameType::eAo, "RFSNDFX.VH", 7, 2, 35, 35, 1, 40, 40, 2, 127},
    {GameType::eAo, "RFSNDFX.VH", 7, 3, 34, 34, 2, 40, 40, 2, 127},
    {GameType::eAo, "RFSNDFX.VH", 7, 4, 33, 33, 3, 40, 40, 2, 127},
    {GameType::eAo, "RFSNDFX.VH", 7, 5, 32, 32, 7, 40, 40, 2, 127},
    {GameType::eAo, "RFSNDFX.VH", 7, 6, 31, 31, 9, 40, 40, 2, 127},
    {GameType::eAo, "RFSNDFX.VH", 7, 7, 30, 30, 12, 60, 60, 2, 127},
    {GameType::eAo, "RFSNDFX.VH", 7, 8, 49, 51, 16, 40, 40, 2, 127},
    {GameType::eAo, "RFSNDFX.VH", 7, 9, 52, 63, 17, 40, 40, 2, 127},
    {GameType::eAo, "RFSNDFX.VH", 49, 1, 61, 61, 97, 127, 127, 64, 127},
    {GameType::eAo, "RFSNDFX.VH", 81, 0, 0, 127, 44, 70, 70, 127, 127},
};

static bool CaseInsensitiveEquals(const std::string& a, const char_type* pB)
{
    const std::string b = pB;
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y)
    {
        return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
    });
}

void PsxSoundEngine::RestorePs1Tones(GameType game, const std::string& vhFile, std::vector<u8>& vhData)
{
    if (vhData.size() < sizeof(VabHeader))
    {
        return;
    }

    VabHeader header = {};
    memcpy(&header, vhData.data(), sizeof(VabHeader));
    const size_t numTones = static_cast<size_t>(std::clamp<s32>(header.field_12_num_progs, 0, kMaxPrograms)) * kTonesPerProgram;
    if (vhData.size() < sizeof(VabHeader) + numTones * sizeof(VagAtr))
    {
        return;
    }

    for (const Ps1ToneValues& entry : kPs1ToneValues)
    {
        if (entry.mGame != game || !CaseInsensitiveEquals(vhFile, entry.mVhFile))
        {
            continue;
        }

        for (size_t i = 0; i < numTones; i++)
        {
            VagAtr tone = {};
            u8* pTone = vhData.data() + sizeof(VabHeader) + i * sizeof(VagAtr);
            memcpy(&tone, pTone, sizeof(VagAtr));
            if (tone.field_14_prog == entry.mProgram && i % kTonesPerProgram == entry.mTone && static_cast<u8>(tone.field_6_min) == entry.mMinKey
                && static_cast<u8>(tone.field_7_max) == entry.mMaxKey && (tone.field_16_vag & 0xFF) == entry.mVag
                && static_cast<u8>(tone.field_0_priority) == entry.mPcPriority && static_cast<u8>(tone.field_2_vol) == entry.mPcVol)
            {
                tone.field_0_priority = static_cast<s8>(entry.mPs1Priority);
                tone.field_2_vol = static_cast<s8>(entry.mPs1Vol);
                memcpy(pTone, &tone, sizeof(VagAtr));
            }
        }
    }
}

PsxSoundEngine::PsxSoundEngine()
{
    for (std::array<u8, 2>& v : mSeqVolumes)
    {
        v = {127, 127};
    }
    for (std::array<u8, kMidiChannels>& v : mChannelVolumes)
    {
        v.fill(127);
    }

    // Sized here so Mix never allocates on the audio thread
    mRenderBuffer.resize(kMixChunkFrames * 2);
}

PsxSoundEngine* PsxSoundEngine::Get()
{
    return sDSound_BBC344 ? sDSound_BBC344->Ps1Sound() : nullptr;
}

void PsxSoundEngine::OpenVab(s32 vabId, const VabHeader& header)
{
    if (!ValidVab(vabId))
    {
        return;
    }

    std::lock_guard<std::mutex> lock(mMutex);
    StopVabVoices(vabId);

    Vab& vab = mVabs[vabId];
    vab.mMasterVol = static_cast<u8>(header.field_18_master_vol);
    vab.mMasterPan = static_cast<u8>(header.field_19_master_pan);
    for (s32 i = 0; i < kMaxPrograms; i++)
    {
        vab.mPrograms[i].mVol = static_cast<u8>(header.field_20_progs[i].field_1_vol);
        vab.mPrograms[i].mPan = static_cast<u8>(header.field_20_progs[i].field_4_pan);
    }

    // Laid out like SsVabOpenHead's table: each program's tones are its tone slots 0-15
    vab.mTones.assign(kMaxPrograms * kTonesPerProgram, Tone{});
    const VagAtr* pVagAtr = reinterpret_cast<const VagAtr*>(&header + 1);
    const s32 numProgs = std::clamp<s32>(header.field_12_num_progs, 0, kMaxPrograms);
    for (s32 i = 0; i < numProgs; i++)
    {
        for (s32 toneIdx = 0; toneIdx < kTonesPerProgram; toneIdx++, pVagAtr++)
        {
            const s32 program = pVagAtr->field_14_prog;
            if (pVagAtr->field_2_vol <= 0 || program < 0 || program >= kMaxPrograms)
            {
                continue;
            }

            Tone& tone = vab.mTones[program * kTonesPerProgram + toneIdx];
            tone.mVol = static_cast<u8>(pVagAtr->field_2_vol);
            tone.mPan = static_cast<u8>(pVagAtr->field_3_pan);
            tone.mMode = static_cast<u8>(pVagAtr->field_1_mode);
            tone.mCentre = pVagAtr->field_4_centre;
            tone.mShift = pVagAtr->field_5_shift;
            tone.mBendMin = static_cast<u8>(pVagAtr->field_C_pitch_bend_min);
            tone.mBendMax = static_cast<u8>(pVagAtr->field_D_pitch_bend_max);
            tone.mAdsr1 = static_cast<u16>(pVagAtr->field_10_adsr1);
            tone.mAdsr2 = static_cast<u16>(pVagAtr->field_12_adsr2);
            // VAG numbers start at 1, as in SsVabOpenHead
            tone.mVag = static_cast<s16>((pVagAtr->field_16_vag & 0xFF) - 1);
        }
    }

    for (s32 i = 0; i < kMaxSamples; i++)
    {
        vab.mSamples[i].clear();
        vab.mLoops[i] = false;
    }
}

void PsxSoundEngine::SetSample(s32 vabId, s32 vagIdx, const s16* pPcm, u32 sampleCount, bool loop)
{
    if (!ValidVab(vabId) || vagIdx < 0 || vagIdx >= kMaxSamples)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(mMutex);
    // A voice playing the old sample would read freed memory
    StopVabVoices(vabId);
    mVabs[vabId].mSamples[vagIdx].assign(pPcm, pPcm + sampleCount);
    mVabs[vabId].mLoops[vagIdx] = loop;
}

void PsxSoundEngine::CloseVab(s32 vabId)
{
    if (!ValidVab(vabId))
    {
        return;
    }

    std::lock_guard<std::mutex> lock(mMutex);
    StopVabVoices(vabId);
    Vab& vab = mVabs[vabId];
    vab.mTones.clear();
    for (s32 i = 0; i < kMaxSamples; i++)
    {
        vab.mSamples[i] = {};
        vab.mLoops[i] = false;
    }
}

void PsxSoundEngine::StopVabVoices(s32 vabId)
{
    for (s32 i = 0; i < PsxSpu::kNumVoices; i++)
    {
        if (mVoices[i].mVabId == vabId)
        {
            mSpu.StopVoice(i);
            mSpu.SetVoiceSample(i, {});
            mVoices[i] = {};
            mAllocs[i].mKeyOn = false;
            mAllocs[i].mEnvelope = 0;
            mAllocs[i].mUnrendered = false;
        }
    }
}

void PsxSoundEngine::NoteOnSeq(s32 voice, s32 vabId, s32 program, s32 tone, s32 note, s32 velocity, s32 seq, s32 channel)
{
    if (seq < 0 || seq >= kMaxSeqs || channel < 0 || channel >= kMidiChannels)
    {
        return;
    }
    // libsnd scales the velocity by the channel volume (CC 7), and pans by the channel pan (CC 10),
    // default 64. The games' SEQs never change the pan.
    constexpr s32 kChannelPan = 64;
    NoteOn(voice, vabId, program, tone, note, seq, true, velocity, mChannelVolumes[seq][channel], kChannelPan);
}

void PsxSoundEngine::NoteOnSfx(s32 voice, s32 vabId, s32 program, s32 tone, s32 note, s32 volLeft, s32 volRight, bool squared)
{
    s32 velocity = 0;
    s32 pan = 64;
    SfxVelocityAndPan(std::clamp(volLeft, 0, 127), std::clamp(volRight, 0, 127), velocity, pan);
    NoteOn(voice, vabId, program, tone, note, -1, squared, velocity, 127, pan);
}

PsxSoundEngine::VoiceVolume PsxSoundEngine::CurrentVolume(const VoiceInfo& info) const
{
    const bool seqNote = info.mSeq >= 0;
    const s32 seqLeft = seqNote ? mSeqVolumes[info.mSeq][0] : 127;
    const s32 seqRight = seqNote ? mSeqVolumes[info.mSeq][1] : 127;
    // libsnd scales the velocity by the channel volume first (_SsVmKeyOn, 0x8007b56c in the AO PS1
    // executable)
    const s32 velocity = seqNote ? info.mVelocity * info.mChannelVolume / 127 : info.mVelocity;
    return LibsndVoiceVolume(seqNote, info.mSquared, velocity, info.mVabVol, info.mProgVol, info.mToneVol, info.mTonePan, info.mProgPan, info.mChannelPan, seqLeft, seqRight);
}

void PsxSoundEngine::OpenSeq(s32 seq)
{
    ResetChannelVolumes(seq);
    SetSeqVolume(seq, 127, 127);
}

void PsxSoundEngine::ResetChannelVolumes(s32 seq)
{
    if (seq < 0 || seq >= kMaxSeqs)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(mMutex);
    mChannelVolumes[seq].fill(127);
}

void PsxSoundEngine::SetChannelVolume(s32 seq, s32 channel, s32 vabId, s32 program, s32 volume)
{
    if (seq < 0 || seq >= kMaxSeqs || channel < 0 || channel >= kMidiChannels)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(mMutex);
    volume = std::clamp(volume, 0, 127);
    mChannelVolumes[seq][channel] = static_cast<u8>(volume);
    for (s32 i = 0; i < PsxSpu::kNumVoices; i++)
    {
        VoiceInfo& info = mVoices[i];
        if (info.mSeq == seq && info.mVabId == vabId && info.mProgram == program && mSpu.IsVoiceActive(i))
        {
            // _SsVmSetVol (0x8007d6ac) turns 0 into 1 for the playing notes only
            info.mChannelVolume = std::max(volume, 1);
            UpdateVoiceVolume(i);
        }
    }
}

void PsxSoundEngine::UpdateVoiceVolume(s32 voice)
{
    VoiceInfo& info = mVoices[voice];
    const VoiceVolume v = CurrentVolume(info);
    info.mState.mVolLeft = static_cast<s16>(v.mLeft);
    info.mState.mVolRight = static_cast<s16>(v.mRight);
    mSpu.SetVoiceVolumeRegisters(voice, static_cast<u16>(v.mLeft), static_cast<u16>(v.mRight));
}

void PsxSoundEngine::SetSeqVolume(s32 seq, s32 left, s32 right)
{
    if (seq < 0 || seq >= kMaxSeqs)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(mMutex);
    mSeqVolumes[seq] = {static_cast<u8>(std::clamp(left, 0, 127)), static_cast<u8>(std::clamp(right, 0, 127))};
    for (s32 i = 0; i < PsxSpu::kNumVoices; i++)
    {
        VoiceInfo& info = mVoices[i];
        if (info.mSeq == seq && info.mVabId >= 0 && mSpu.IsVoiceActive(i))
        {
            UpdateVoiceVolume(i);
        }
    }
}

void PsxSoundEngine::NoteOn(s32 voice, s32 vabId, s32 program, s32 tone, s32 note, s32 seq, bool squared, s32 velocity, s32 channelVolume, s32 channelPan)
{
    if (!ValidVoice(voice) || !ValidVab(vabId) || program < 0 || program >= kMaxPrograms || tone < 0 || tone >= kTonesPerProgram)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(mMutex);
    const Vab& vab = mVabs[vabId];
    if (vab.mTones.empty())
    {
        return;
    }

    const Tone& t = vab.mTones[program * kTonesPerProgram + tone];
    if (t.mVol == 0 || t.mVag < 0 || vab.mSamples[t.mVag].empty())
    {
        return;
    }

    const Program& prog = vab.mPrograms[program];

    const std::vector<s16>& pcm = vab.mSamples[t.mVag];
    PsxSpuSample sample;
    sample.mPcm = pcm.data();
    sample.mLength = static_cast<u32>(pcm.size());
    sample.mLoop = vab.mLoops[t.mVag];

    VoiceInfo& info = mVoices[voice];
    info.mVabId = vabId;
    info.mProgram = program;
    // note is in 1/256 semitones
    info.mNote128 = note >> 1;
    info.mCentre = t.mCentre;
    info.mShift = t.mShift;
    info.mBendMin = t.mBendMin;
    info.mBendMax = t.mBendMax;

    info.mSeq = seq;
    info.mSquared = squared;
    info.mVelocity = std::clamp(velocity, 0, 127);
    info.mChannelVolume = std::clamp(channelVolume, 0, 127);
    info.mChannelPan = channelPan;
    info.mVabVol = vab.mMasterVol;
    info.mProgVol = prog.mVol;
    info.mToneVol = t.mVol;
    info.mTonePan = t.mPan;
    info.mProgPan = prog.mPan;
    const VoiceVolume volume = CurrentVolume(info);

    info.mState.mVolLeft = static_cast<s16>(volume.mLeft);
    info.mState.mVolRight = static_cast<s16>(volume.mRight);
    info.mState.mAdsr1 = t.mAdsr1;
    info.mState.mAdsr2 = t.mAdsr2;
    // VagAtr mode 4: the tone goes through the reverb
    info.mState.mReverb = (t.mMode & 4) != 0;

    mSpu.SetVoiceSample(voice, sample);
    SetVoicePitch(voice, info.mNote128);
    // The VOLL/VOLR registers, fixed volume
    mSpu.SetVoiceVolumeRegisters(voice, static_cast<u16>(volume.mLeft), static_cast<u16>(volume.mRight));
    mSpu.SetVoiceAdsr(voice, t.mAdsr1, t.mAdsr2);
    mSpu.SetVoiceReverb(voice, info.mState.mReverb);
    mSpu.KeyOn(voice);

    // libsnd's key on (0x8007a9ac) takes the envelope as full until the next tick reads it
    VoiceAlloc& alloc = mAllocs[voice];
    alloc.mKeyOn = true;
    alloc.mEnvelope = 0x7FFF;
    alloc.mSilentTicks = 0;
    alloc.mUnrendered = true;
}

s32 PsxSoundEngine::AllocateVoice(s32 priority)
{
    std::lock_guard<std::mutex> lock(mMutex);
    s32 voice = -1;
    s32 victim = -1;
    s32 lowestPriority = priority;
    u16 lowestEnvelope = 0xFFFF;
    u16 oldest = 0;
    for (s32 i = 0; i < PsxSpu::kNumVoices; i++)
    {
        const VoiceAlloc& alloc = mAllocs[i];
        if (!alloc.mKeyOn && alloc.mEnvelope == 0)
        {
            voice = i;
            break;
        }

        if (alloc.mPriority < lowestPriority)
        {
            victim = i;
            lowestPriority = alloc.mPriority;
            lowestEnvelope = alloc.mEnvelope;
            oldest = alloc.mAge;
        }
        else if (alloc.mPriority == lowestPriority)
        {
            if (alloc.mEnvelope < lowestEnvelope)
            {
                victim = i;
                lowestEnvelope = alloc.mEnvelope;
                oldest = alloc.mAge;
            }
            else if (alloc.mEnvelope == lowestEnvelope && alloc.mAge > oldest)
            {
                victim = i;
                oldest = alloc.mAge;
            }
        }
    }

    if (voice < 0)
    {
        voice = victim;
    }
    if (voice < 0)
    {
        return -1;
    }

    for (VoiceAlloc& alloc : mAllocs)
    {
        alloc.mAge++;
    }
    mAllocs[voice].mAge = 0;
    mAllocs[voice].mPriority = priority;
    return voice;
}

void PsxSoundEngine::Tick()
{
    std::lock_guard<std::mutex> lock(mMutex);
    for (s32 i = 0; i < PsxSpu::kNumVoices; i++)
    {
        VoiceAlloc& alloc = mAllocs[i];
        if (alloc.mUnrendered)
        {
            continue;
        }

        alloc.mEnvelope = static_cast<u16>(std::max<s16>(mSpu.VoiceEnvelope(i), 0));
        if (alloc.mEnvelope != 0)
        {
            alloc.mSilentTicks = 0;
        }
        else if (alloc.mSilentTicks < 15 && ++alloc.mSilentTicks == 15)
        {
            alloc.mKeyOn = false;
        }
    }
}

void PsxSoundEngine::SetVoicePitch(s32 voice, s32 note128)
{
    VoiceInfo& info = mVoices[voice];
    // A note and a fine tune of 0-127, as libsnd gets them. Pitch variation and bends can make
    // note128 negative, hence rounding down rather than towards zero.
    const s32 note = note128 >= 0 ? note128 / 128 : -((-note128 + 127) / 128);
    info.mState.mPitch = PsxSpu::PitchFromNote(note, note128 - note * 128, info.mCentre, info.mShift);
    mSpu.SetVoicePitch(voice, info.mState.mPitch);
}

void PsxSoundEngine::KeyOff(s32 voice)
{
    if (!ValidVoice(voice))
    {
        return;
    }

    std::lock_guard<std::mutex> lock(mMutex);
    mSpu.KeyOff(voice);
    mAllocs[voice].mKeyOn = false;
}

void PsxSoundEngine::Stop(s32 voice)
{
    if (!ValidVoice(voice))
    {
        return;
    }

    std::lock_guard<std::mutex> lock(mMutex);
    mSpu.StopVoice(voice);
    mAllocs[voice].mKeyOn = false;
    mAllocs[voice].mEnvelope = 0;
    mAllocs[voice].mUnrendered = false;
}

bool PsxSoundEngine::IsVoiceActive(s32 voice)
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mSpu.IsVoiceActive(voice);
}

void PsxSoundEngine::SetPitchOffset(s32 voice, s32 offset)
{
    if (!ValidVoice(voice))
    {
        return;
    }

    std::lock_guard<std::mutex> lock(mMutex);
    if (mVoices[voice].mVabId >= 0)
    {
        SetVoicePitch(voice, mVoices[voice].mNote128 + offset);
    }
}

void PsxSoundEngine::PitchBend(s32 program, s32 bend)
{
    const s32 delta = std::clamp(bend, 0, 127) - 64;

    std::lock_guard<std::mutex> lock(mMutex);
    for (s32 i = 0; i < PsxSpu::kNumVoices; i++)
    {
        VoiceInfo& info = mVoices[i];
        if (info.mVabId < 0 || info.mProgram != program || !mSpu.IsVoiceActive(i))
        {
            continue;
        }

        // libsnd's SsUtPitchBend (AE PS1 executable 0x80077264): the bend moves the voice's key by
        // up to pbmax semitones up (in 63 steps) or pbmin down (in 64 steps). The key's own fine
        // tune is dropped, the tone's shift is added as usual. Divisions round towards zero.
        s32 note = info.mNote128 / 128;
        s32 fine = 0;
        if (delta > 0)
        {
            const s32 x = delta * info.mBendMax;
            note += x / 63;
            fine = (x % 63) * 2;
        }
        else if (delta < 0)
        {
            const s32 x = delta * info.mBendMin;
            note += x / 64 - 1;
            fine = (x % 64) * 2 + 127;
        }
        info.mState.mPitch = PsxSpu::PitchFromNote(note, fine, info.mCentre, info.mShift);
        mSpu.SetVoicePitch(i, info.mState.mPitch);
    }
}

PsxSoundEngine::VoiceState PsxSoundEngine::GetVoiceState(s32 voice)
{
    if (!ValidVoice(voice))
    {
        return {};
    }

    std::lock_guard<std::mutex> lock(mMutex);
    VoiceState state = mVoices[voice].mState;
    state.mPhase = mSpu.VoicePhase(voice);
    return state;
}

void PsxSoundEngine::SetInterpolation(PsxSpu::Interpolation interpolation)
{
    std::lock_guard<std::mutex> lock(mMutex);
    mSpu.SetInterpolation(interpolation);
}

void PsxSoundEngine::SetMasterVolume(s32 left, s32 right)
{
    std::lock_guard<std::mutex> lock(mMutex);
    mSpu.SetMasterVolume(VolumeRegister(left), VolumeRegister(right));
}

void PsxSoundEngine::SetReverbType(s32 type)
{
    if (type < 0 || type >= PsxSpuReverb::kNumPresets)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(mMutex);
    // libsnd's reverb types are in the same order as the presets
    mSpu.SetReverbPreset(static_cast<PsxSpuReverb::Preset>(type));
}

void PsxSoundEngine::SetReverbDepth(s32 left, s32 right)
{
    std::lock_guard<std::mutex> lock(mMutex);
    mSpu.SetReverbDepth(PsxSpuReverb::DepthToVolume(left), PsxSpuReverb::DepthToVolume(right));
}

void PsxSoundEngine::SetReverbEnabled(bool enable)
{
    std::lock_guard<std::mutex> lock(mMutex);
    mSpu.SetReverbEnabled(enable);
}

void PsxSoundEngine::ClearReverb()
{
    std::lock_guard<std::mutex> lock(mMutex);
    mSpu.Reverb().Clear();
}

void PsxSoundEngine::Mix(StereoSample_S16* pOut, u32 frames)
{
    std::lock_guard<std::mutex> lock(mMutex);
    while (frames > 0)
    {
        const u32 chunk = std::min(frames, kMixChunkFrames);
        mSpu.Render(mRenderBuffer.data(), chunk);
        for (VoiceAlloc& alloc : mAllocs)
        {
            alloc.mUnrendered = false;
        }
        for (u32 i = 0; i < chunk; i++)
        {
            pOut[i].left = static_cast<s16>(std::clamp(pOut[i].left + mRenderBuffer[i * 2], -0x8000, 0x7FFF));
            pOut[i].right = static_cast<s16>(std::clamp(pOut[i].right + mRenderBuffer[i * 2 + 1], -0x8000, 0x7FFF));
        }
        pOut += chunk;
        frames -= chunk;
    }
}
