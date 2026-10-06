#include "../stdafx.h"
#include "PsxSoundEngine.hpp"
#include "SDLSoundSystem.hpp"
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// Frames rendered per SPU Render call in Mix, at most
static constexpr u32 kMixChunkFrames = 512;

// 0-127 to a fixed SPU volume, as SsSetMVol does (0x81 * vol, 0x3FFF = full)
static s16 VolumeRegister(s32 vol)
{
    return static_cast<s16>(std::clamp(vol, 0, 127) * 0x7FFE / 127);
}

static Libsnd::Config LibsndConfig(GameType game)
{
    Libsnd::Config config;
    // AO's libsnd (0x8007c73c in its PS1 executable) squares every voice's volume, AE's (0x80076d94)
    // only a SEQ note's
    config.mSquareSfxVolume = game == GameType::eAo;
    if (game == GameType::eAe)
    {
        config.mSsSeqSetVol = Libsnd::Config::SsSeqSetVolMode::eWhilePlaying;
        config.mSetSeqVolAlwaysUpdates = true;
        config.mSeqVolZeroIsOne = false;
        config.mSeqVolNoteChannel = true;
        config.mNoteOnSkippedAtSeqVolZero = false;
    }
    return config;
}

PsxSoundEngine::PsxSoundEngine()
    : mLibsnd(*this, LibsndConfig(GetGameType()))
{
    // Sized here so Mix never allocates on the audio thread
    mRenderBuffer.resize(kMixChunkFrames * 2);

    // Voices that stop while they are loud fade out over 3 ms instead of clicking. The hardware
    // clicks; RELIVE_NO_DECLICK=1 gives its output for comparing (see build-ps1re/ref)
    SetDeclick(getenv("RELIVE_NO_DECLICK") == nullptr);

    if (const char* pRecord = getenv("RELIVE_SOUND_RECORD"))
    {
        mRecordFile = fopen(pRecord, "w");
    }
}

void PsxSoundEngine::FadeOutNextKeyOffs(u32 releaseMs, u32 windowMs)
{
    std::lock_guard<std::mutex> lock(mMutex);
    mSpu.SetMinimumReleaseTime(releaseMs * PsxSpu::kSampleRate / 1000);
    mLongReleaseSamplesLeft = windowMs * PsxSpu::kSampleRate / 1000;
}

void PsxSoundEngine::SetDeclick(bool enable)
{
    std::lock_guard<std::mutex> lock(mMutex);
    mSpu.SetDeclick(enable);
}

PsxSoundEngine::~PsxSoundEngine()
{
    if (mRecordFile)
    {
        fclose(mRecordFile);
    }
}

void PsxSoundEngine::Record(const char* fmt, ...)
{
    if (!mRecordFile)
    {
        return;
    }
    fprintf(mRecordFile, "%u ", SDLSoundSystem::GetTicks());
    va_list args;
    va_start(args, fmt);
    vfprintf(mRecordFile, fmt, args);
    va_end(args);
    fputc('\n', mRecordFile);
    fflush(mRecordFile);
}

PsxSoundEngine* PsxSoundEngine::Get()
{
    SDLSoundSystem* pSystem = SDLSoundSystem::Get();
    return pSystem ? &pSystem->Engine() : nullptr;
}

// ---- VABs ----

void PsxSoundEngine::OpenVab(s32 vabId, const u8* pVh, u32 size)
{
    if (!ValidVab(vabId) || !pVh)
    {
        return;
    }

    // The old samples are freed after the lock is released
    Vab old;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        StopVabVoices(vabId);
        mLibsnd.VabClose(static_cast<s16>(vabId));
        std::swap(old, mVabs[vabId]);
        mLibsnd.VabOpenHead(pVh, size, static_cast<s16>(vabId));
    }
}

void PsxSoundEngine::SetSample(s32 vabId, s32 vagIdx, const s16* pPcm, u32 sampleCount, bool loop, u32 loopStart)
{
    if (!ValidVab(vabId) || vagIdx < 0 || vagIdx >= kMaxSamples)
    {
        return;
    }

    // The copy is made, and the old data freed, without the lock: a VAB load sets every sample
    // in a loop and the audio thread must keep getting the lock in between. Only the voices on
    // this VAG are stopped (they would read freed memory), the rest keep playing through the load.
    std::vector<s16> pcm(pPcm, pPcm + sampleCount);
    std::array<std::vector<s16>, kPsxSpuFilterLevels> oldFiltered;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        StopVagVoices(vabId, vagIdx + 1);
        std::swap(mVabs[vabId].mSamples[vagIdx], pcm);
        mVabs[vabId].mLoops[vagIdx] = loop;
        mVabs[vabId].mLoopStarts[vagIdx] = loopStart;
        std::swap(mVabs[vabId].mFiltered[vagIdx], oldFiltered);
    }
}

void PsxSoundEngine::CloseVab(s32 vabId)
{
    if (!ValidVab(vabId))
    {
        return;
    }

    Vab old;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        StopVabVoices(vabId);
        mLibsnd.VabClose(static_cast<s16>(vabId));
        std::swap(old, mVabs[vabId]);
    }
}

void PsxSoundEngine::StopVabVoices(s32 vabId)
{
    // Not libsnd: the PS1 voice would play whatever was in SPU RAM, here the sample is freed
    for (s32 i = 0; i < PsxSpu::kNumVoices; i++)
    {
        VoiceRegs& r = mRegs[i];
        if (r.mPlayingVabId == vabId)
        {
            mSpu.StopVoice(i);
            r.mPlayingVabId = -1;
            r.mPlayingVag = 0;
        }
        if (r.mNextVabId == vabId)
        {
            mSpu.SetVoiceSample(i, PsxSpuSample{});
            r.mNextVabId = -1;
            r.mNextVag = 0;
        }
    }
}

void PsxSoundEngine::StopVagVoices(s32 vabId, s32 vag)
{
    for (s32 i = 0; i < PsxSpu::kNumVoices; i++)
    {
        VoiceRegs& r = mRegs[i];
        if (r.mPlayingVabId == vabId && r.mPlayingVag == vag)
        {
            mSpu.StopVoice(i);
            r.mPlayingVabId = -1;
            r.mPlayingVag = 0;
        }
        if (r.mNextVabId == vabId && r.mNextVag == vag)
        {
            mSpu.SetVoiceSample(i, PsxSpuSample{});
            r.mNextVabId = -1;
            r.mNextVag = 0;
        }
    }
}

// ---- libsnd ----

s16 PsxSoundEngine::SeqOpen(const u8* pSeq, u32 size, s16 vabId)
{
    std::array<u32, 256> maxPitch = {};
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mLibsnd.SeqPitches(pSeq, size, vabId, maxPitch);
    }
    FilterSamples(vabId, maxPitch);

    std::lock_guard<std::mutex> lock(mMutex);
    return mLibsnd.SeqOpen(pSeq, size, vabId);
}

void PsxSoundEngine::FilterSamples(s32 vabId, const std::array<u32, 256>& maxPitch)
{
    if (!ValidVab(vabId))
    {
        return;
    }
    for (s32 vag = 1; vag < 256; vag++)
    {
        const s32 levels = PsxSpu::FilterLevelsFor(maxPitch[vag]);
        if (levels == 0 || vag > kMaxSamples)
        {
            continue;
        }

        // A copy of the sample, filtered without the lock, then put in if it's still the same
        std::vector<s16> pcm;
        bool loop = false;
        u32 loopStart = 0;
        s32 have = 0;
        {
            std::lock_guard<std::mutex> lock(mMutex);
            const Vab& vab = mVabs[vabId];
            while (have < levels && vab.mFiltered[vag - 1][have].size() == vab.mSamples[vag - 1].size() && !vab.mSamples[vag - 1].empty())
            {
                have++;
            }
            if (have == levels || vab.mSamples[vag - 1].empty())
            {
                continue;
            }
            pcm = vab.mSamples[vag - 1];
            loop = vab.mLoops[vag - 1];
            loopStart = vab.mLoopStarts[vag - 1];
        }

        PsxSpuSample sample;
        sample.mPcm = pcm.data();
        sample.mLength = static_cast<u32>(pcm.size());
        sample.mLoop = loop;
        sample.mLoopStart = loopStart;
        std::array<std::vector<s16>, kPsxSpuFilterLevels> filtered;
        for (s32 i = have; i < levels; i++)
        {
            filtered[i] = PsxSpu::FilterSample(sample, i + 1);
        }

        std::lock_guard<std::mutex> lock(mMutex);
        Vab& vab = mVabs[vabId];
        if (vab.mSamples[vag - 1] != pcm)
        {
            continue;
        }
        // The voices playing the sample keep the copies they had until their next key on
        for (s32 i = have; i < levels; i++)
        {
            vab.mFiltered[vag - 1][i] = std::move(filtered[i]);
        }
    }
}

void PsxSoundEngine::SeqClose(s16 seq)
{
    Record("SEQCLOSE %d", seq);
    std::lock_guard<std::mutex> lock(mMutex);
    mLibsnd.SeqClose(seq);
}

void PsxSoundEngine::SeqPlay(s16 seq, s8 mode, s16 count)
{
    Record("SEQPLAY %d %d %d", seq, mode, count);
    std::lock_guard<std::mutex> lock(mMutex);
    mLibsnd.SeqPlay(seq, mode, count);
}

void PsxSoundEngine::SeqStop(s16 seq)
{
    Record("SEQSTOP %d", seq);
    std::lock_guard<std::mutex> lock(mMutex);
    mLibsnd.SeqStop(seq);
}

void PsxSoundEngine::SeqSetVol(s16 seq, s16 left, s16 right)
{
    Record("SEQVOL %d %d %d", seq, left, right);
    std::lock_guard<std::mutex> lock(mMutex);
    mLibsnd.SeqSetVol(seq, left, right);
}

s16 PsxSoundEngine::IsEos(s16 seq)
{
    std::lock_guard<std::mutex> lock(mMutex);
    return mLibsnd.IsEos(seq, 0);
}

s32 PsxSoundEngine::VoKeyOn(s32 vabProg, s32 pitch, u16 left, u16 right)
{
    // A sound effect played fast for the first time gets its sample filtered first (rare)
    std::array<u32, 256> maxPitch = {};
    bool filter = false;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mLibsnd.SfxPitches(vabProg, pitch, maxPitch);
        filter = std::any_of(maxPitch.begin(), maxPitch.end(), [](u32 p) { return PsxSpu::FilterLevelsFor(p) > 0; });
    }
    if (filter)
    {
        FilterSamples((vabProg >> 8) & 0xFF, maxPitch);
    }

    std::lock_guard<std::mutex> lock(mMutex);
    const s32 result = mLibsnd.VoKeyOn(vabProg, pitch, left, right);
    const s32 voices = result == -1 ? 0 : result;
    Record("KEYON %d %d %d %d %d", voices, vabProg, pitch, left, right);
    return voices;
}

void PsxSoundEngine::UtKeyOffV(s16 voice)
{
    Record("KEYOFFV %d", voice);
    std::lock_guard<std::mutex> lock(mMutex);
    mLibsnd.UtKeyOffV(voice);
}

void PsxSoundEngine::UtChangePitch(s16 voice, s16 vabId, s16 prog, s16 oldNote, s16 oldFine, s16 newNote, s16 newFine)
{
    Record("PITCH %d %d %d %d %d %d %d", voice, vabId, prog, oldNote, oldFine, newNote, newFine);
    std::lock_guard<std::mutex> lock(mMutex);
    mLibsnd.UtChangePitch(voice, vabId, prog, oldNote, oldFine, newNote, newFine);
}

void PsxSoundEngine::UtAllKeyOff()
{
    Record("ALLKEYOFF");
    std::lock_guard<std::mutex> lock(mMutex);
    mLibsnd.UtAllKeyOff();
}

void PsxSoundEngine::StopAll()
{
    std::lock_guard<std::mutex> lock(mMutex);
    for (s32 i = 0; i < PsxSpu::kNumVoices; i++)
    {
        mSpu.StopVoice(i);
        mRegs[i].mPlayingVabId = -1;
        mRegs[i].mPlayingVag = 0;
    }
}

// ---- ILibsndSpu ----

u16 PsxSoundEngine::VoiceEnvelope(s32 voice)
{
    return static_cast<u16>(mSpu.VoiceEnvelope(voice));
}

void PsxSoundEngine::SetVoiceVolume(s32 voice, u16 left, u16 right)
{
    mSpu.SetVoiceVolumeRegisters(voice, left, right);
    mRegs[voice].mVolLeft = left;
    mRegs[voice].mVolRight = right;
}

void PsxSoundEngine::SetVoicePitch(s32 voice, u32 pitch)
{
    mSpu.SetVoicePitch(voice, pitch);
    mRegs[voice].mPitch = pitch;
}

void PsxSoundEngine::SetVoiceSample(s32 voice, s32 vabId, s32 vag)
{
    // VAG numbers start at 1. Anything else (SsUtAllKeyOff's) is silence.
    PsxSpuSample sample;
    VoiceRegs& r = mRegs[voice];
    r.mNextVabId = -1;
    r.mNextVag = 0;
    if (ValidVab(vabId) && vag >= 1 && vag <= kMaxSamples)
    {
        const Vab& vab = mVabs[vabId];
        const std::vector<s16>& pcm = vab.mSamples[vag - 1];
        if (!pcm.empty())
        {
            sample.mPcm = pcm.data();
            sample.mLength = static_cast<u32>(pcm.size());
            sample.mLoop = vab.mLoops[vag - 1];
            sample.mLoopStart = vab.mLoopStarts[vag - 1];
            for (s32 i = 0; i < kPsxSpuFilterLevels; i++)
            {
                const std::vector<s16>& f = vab.mFiltered[vag - 1][i];
                sample.mFiltered[i] = f.size() == pcm.size() ? f.data() : nullptr;
            }
            r.mNextVabId = vabId;
            r.mNextVag = vag;
        }
    }
    mSpu.SetVoiceSample(voice, sample);
}

void PsxSoundEngine::SetVoiceAdsr(s32 voice, u16 adsr1, u16 adsr2)
{
    mSpu.SetVoiceAdsr(voice, adsr1, adsr2);
    mRegs[voice].mAdsr1 = adsr1;
    mRegs[voice].mAdsr2 = adsr2;
}

void PsxSoundEngine::KeyOff(u32 voices)
{
    mSpu.KeyOffMask(voices);
}

void PsxSoundEngine::KeyOn(u32 voices)
{
    for (s32 i = 0; i < PsxSpu::kNumVoices; i++)
    {
        if (voices & (1u << i))
        {
            mRegs[i].mPlayingVabId = mRegs[i].mNextVabId;
            mRegs[i].mPlayingVag = mRegs[i].mNextVag;
        }
    }
    mSpu.KeyOnMask(voices);
}

void PsxSoundEngine::SetReverbVoices(u32 voices)
{
    for (s32 i = 0; i < PsxSpu::kNumVoices; i++)
    {
        const bool on = (voices & (1u << i)) != 0;
        mSpu.SetVoiceReverb(i, on);
        mRegs[i].mReverb = on;
    }
}

// ---- the rest ----

PsxSoundEngine::VoiceState PsxSoundEngine::GetVoiceState(s32 voice)
{
    if (voice < 0 || voice >= PsxSpu::kNumVoices)
    {
        return {};
    }

    std::lock_guard<std::mutex> lock(mMutex);
    const VoiceRegs& r = mRegs[voice];
    VoiceState state;
    state.mPhase = mSpu.VoicePhase(voice);
    state.mPitch = r.mPitch;
    state.mVolLeft = static_cast<s16>(r.mVolLeft);
    state.mVolRight = static_cast<s16>(r.mVolRight);
    state.mAdsr1 = r.mAdsr1;
    state.mAdsr2 = r.mAdsr2;
    state.mReverb = r.mReverb;
    state.mLibsnd = mLibsnd.GetVoiceInfo(voice);
    return state;
}

PsxSoundEngine::SampleInfo PsxSoundEngine::GetSample(s32 vabId, s32 vagIdx)
{
    SampleInfo info;
    if (!ValidVab(vabId) || vagIdx < 0 || vagIdx >= kMaxSamples)
    {
        return info;
    }

    std::lock_guard<std::mutex> lock(mMutex);
    info.mPcm = mVabs[vabId].mSamples[vagIdx];
    info.mLoop = mVabs[vabId].mLoops[vagIdx];
    info.mLoopStart = mVabs[vabId].mLoopStarts[vagIdx];
    return info;
}

void PsxSoundEngine::SetInterpolation(PsxSpu::Interpolation interpolation)
{
    std::lock_guard<std::mutex> lock(mMutex);
    mSpu.SetInterpolation(interpolation);
}

void PsxSoundEngine::SetMasterVolume(s32 left, s32 right)
{
    Record("MVOL %d %d", left, right);
    std::lock_guard<std::mutex> lock(mMutex);
    mSpu.SetMasterVolume(VolumeRegister(left), VolumeRegister(right));
}

void PsxSoundEngine::SetReverbType(s32 type)
{
    Record("RVTYPE %d", type);
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
    Record("RVDEPTH %d %d", left, right);
    std::lock_guard<std::mutex> lock(mMutex);
    mSpu.SetReverbDepth(PsxSpuReverb::DepthToVolume(left), PsxSpuReverb::DepthToVolume(right));
}

void PsxSoundEngine::SetReverbEnabled(bool enable)
{
    Record(enable ? "RVON" : "RVOFF");
    std::lock_guard<std::mutex> lock(mMutex);
    mSpu.SetReverbEnabled(enable);
}

void PsxSoundEngine::ClearReverb()
{
    Record("RVCLEAR");
    std::lock_guard<std::mutex> lock(mMutex);
    if (mSpu.Declick())
    {
        mClearReverbSamplesLeft = kClearReverbDelaySamples;
        return;
    }
    mSpu.Reverb().Clear();
}

void PsxSoundEngine::Mix(StereoSample_S16* pOut, u32 frames)
{
    std::lock_guard<std::mutex> lock(mMutex);
    while (frames > 0)
    {
        // libsnd's tick, between samples like the PS1's vsync interrupt
        if (mSamplesToTick == 0)
        {
            mLibsnd.Tick();
            mSamplesToTick = kSamplesPerTick;
        }
        const u32 chunk = std::min({frames, kMixChunkFrames, mSamplesToTick});
        if (mLongReleaseSamplesLeft > 0)
        {
            mLongReleaseSamplesLeft -= std::min(mLongReleaseSamplesLeft, chunk);
            if (mLongReleaseSamplesLeft == 0)
            {
                mSpu.SetMinimumReleaseTime(0);
            }
        }
        mSpu.Render(mRenderBuffer.data(), chunk);
        if (mClearReverbSamplesLeft > 0)
        {
            mClearReverbSamplesLeft -= std::min(mClearReverbSamplesLeft, chunk);
            if (mClearReverbSamplesLeft == 0)
            {
                mSpu.Reverb().Clear();
            }
        }
        for (u32 i = 0; i < chunk; i++)
        {
            pOut[i].left = static_cast<s16>(std::clamp(pOut[i].left + mRenderBuffer[i * 2], -0x8000, 0x7FFF));
            pOut[i].right = static_cast<s16>(std::clamp(pOut[i].right + mRenderBuffer[i * 2 + 1], -0x8000, 0x7FFF));
        }
        pOut += chunk;
        frames -= chunk;
        mSamplesToTick -= chunk;
    }
}
