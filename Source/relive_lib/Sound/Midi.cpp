#include "../stdafx.h"
#include "Midi.hpp"
#include "GameSound.hpp"
#include "Ps1SoundData.hpp"
#include "VabFile.hpp"
#include "OpenSeqHandle.hpp"
#include "../../relive_lib/BinaryPath.hpp"
#include "../../relive_lib/FatalError.hpp"
#include "../Math.hpp"
#include "../ResourceManagerWrapper.hpp"
#include "../AmbientSound.hpp"
#include "../BaseMap.hpp"
#include "../GameObjects/BackgroundMusic.hpp"
#include "../GameObjects/BaseGameObject.hpp"
#include "../Sfx.hpp"
#include "../SoundDebug.hpp"
#include "../data_conversion/file_system.hpp"
#include <algorithm>
#include <cstring>

static constexpr s32 kSeqTableSizeAE = 144;
static constexpr s32 kSeqTableSizeAO = 164;

// The PS1 executables' SsSetMVol on init and on each reset (AE's at 0x800... and AO's at 0x800784d4)
static constexpr s32 kMasterVolumeAE = 100;
static constexpr s32 kMasterVolumeAO = 127;

// The reverb type the games set on init (libsnd's SPU_REV_MODE_STUDIO_C)
static constexpr s32 kReverbType = 4;

static std::unique_ptr<GameSound> sGameSound;

GameSound* GameSound::Get()
{
    return sGameSound.get();
}

static SDLSoundSystem::Options AudioOptions(const GameSoundConfig& config)
{
    SDLSoundSystem::Options options;
    options.mInterpolation = config.mGaussianInterpolation ? PsxSpu::Interpolation::Gaussian : PsxSpu::Interpolation::BandLimited;
    options.mOffline = config.mOffline;
    options.mDumpPath = config.mDumpPath;
    options.mLiveDumpPath = config.mLiveDumpPath;
    return options;
}

GameSound::GameSound(const GameSoundConfig& config)
    : mConfig(config)
    , mSystem(std::make_unique<SDLSoundSystem>(AudioOptions(config)))
{
    for (s16& seq : mOpenSeqs)
    {
        seq = -1;
    }

    PsxSoundEngine& engine = Engine();
    engine.SetReverbDepth(0, 0);
    engine.SetReverbType(kReverbType);
    engine.SetReverbEnabled(true);
    SetMasterVolume(config.mGame == GameType::eAo ? kMasterVolumeAO : kMasterVolumeAE);
}

GameSound::~GameSound() = default;

void GameSound::SetMasterVolume(s32 volume)
{
    Engine().SetMasterVolume(volume, volume);
}

s32 GameSound::SeqTableSize() const
{
    return mConfig.mGame == GameType::eAo ? kSeqTableSizeAO : kSeqTableSizeAE;
}

s32 GameSound::LastLoadedVabId() const
{
    const auto pInfo = mLastLoadedSoundBlockInfo.lock();
    if (!pInfo)
    {
        ALIVE_FATAL("Tried to play a sound before any sound block was loaded");
    }
    return pInfo->mVabId;
}

// ---- Sound blocks ----

void GameSound::CloseVab(s32 vabId)
{
    if (vabId < 0 || vabId >= kMaxVabs)
    {
        return;
    }
    PsxSoundEngine& engine = Engine();
    engine.Record("VABCLOSE %d", vabId);
    engine.UtAllKeyOff();
    engine.CloseVab(vabId);
    mOpenVabs &= ~(1u << vabId);
}

void GameSound::FreeAllVabs()
{
    auto pIter = mLastLoadedSoundBlockInfo.lock();
    if (pIter)
    {
        CloseVab(pIter->mVabId);
        pIter->mVabId = -1;
    }
    mLastLoadedSoundBlockInfo.reset();
}

void GameSound::FreeAllSeqs()
{
    // Not set until the first level's sounds load, which quitting straight away comes before
    if (!mSeqTable)
    {
        return;
    }

    for (s32 i = 0; i < SeqTableSize(); i++)
    {
        mSeqTable[i].field_C_ppSeq_Data.clear();
    }
}

void GameSound::Reset()
{
    StopAllSeqs();
    FreeAllSeqs();
    FreeAllVabs();
    SetMasterVolume(mConfig.mGame == GameType::eAo ? kMasterVolumeAO : kMasterVolumeAE);
}

void GameSound::Shutdown()
{
    Reset();

    PsxSoundEngine& engine = Engine();
    SetMasterVolume(0);
    engine.SetReverbEnabled(false);
    engine.SetReverbDepth(0, 0);
    engine.StopAll();
    for (s32 i = 0; i < kMaxVabs; i++)
    {
        if (mOpenVabs & (1u << i))
        {
            CloseVab(i);
        }
    }
}

// AE: the VB file is a table of records, the samples are in sounds.dat
static void LoadVabBodyAE(FileSystem& fs, PsxSoundEngine& engine, s32 vabId, const std::vector<u8>& vbData, s32 vagCount)
{
    AutoFILE soundsDat = fs.OpenFile("sounds.dat", "rb");
    if (!soundsDat.GetFile())
    {
        return;
    }

    std::vector<s16> pcm;
    for (s32 i = 0; i < vagCount; i++)
    {
        if (static_cast<size_t>(i + 1) * sizeof(VabBodyRecord) > vbData.size())
        {
            break;
        }

        VabBodyRecord record = {};
        memcpy(&record, vbData.data() + static_cast<size_t>(i) * sizeof(VabBodyRecord), sizeof(VabBodyRecord));

        // The length is in 16 bit samples. A negative second field is a looping sample.
        const s32 sampleCount = static_cast<s32>(8 * static_cast<u32>(record.field_0_length_or_duration)) / 16;
        const s32 fileOffset = static_cast<s32>(record.field_8_fileOffset);
        if (sampleCount <= 0 || fileOffset == -1)
        {
            continue;
        }

        pcm.assign(static_cast<size_t>(sampleCount), 0);
        soundsDat.Seek(static_cast<u32>(fileOffset), AutoFILE::SeekMode::Start);
        soundsDat.Read(reinterpret_cast<u8*>(pcm.data()), static_cast<u32>(2 * sampleCount));
        engine.SetSample(vabId, i, pcm.data(), static_cast<u32>(sampleCount), record.field_4_unused < 0);
    }
}

// AO: the VB file has the samples after a header of two s32s each, one after the other
static void LoadVabBodyAO(PsxSoundEngine& engine, s32 vabId, const std::vector<u8>& vbData, s32 vagCount)
{
    size_t pos = 0;
    std::vector<s16> pcm;
    for (s32 i = 0; i < vagCount; i++)
    {
        if (pos + 2 * sizeof(s32) > vbData.size())
        {
            break;
        }

        s32 lengthOrDuration = 0;
        s32 unused = 0;
        memcpy(&lengthOrDuration, vbData.data() + pos, sizeof(s32));
        memcpy(&unused, vbData.data() + pos + sizeof(s32), sizeof(s32));
        const size_t dataPos = pos + 2 * sizeof(s32);
        // The next sample starts after this one's length in bytes
        pos += static_cast<size_t>(static_cast<u32>(lengthOrDuration)) + 8;

        const s32 sampleCount = (8 * lengthOrDuration) / 16;
        if (sampleCount <= 0 || dataPos + static_cast<size_t>(2 * sampleCount) > vbData.size())
        {
            continue;
        }

        pcm.resize(static_cast<size_t>(sampleCount));
        memcpy(pcm.data(), vbData.data() + dataPos, static_cast<size_t>(2 * sampleCount));
        engine.SetSample(vabId, i, pcm.data(), static_cast<u32>(sampleCount), unused < 0);
    }
}

void GameSound::LoadVab(PathSoundInfo& info, ResourceManagerWrapper& resMan)
{
    // The PS1's own data (-ps1_data), if it has this VAB. It has the PS1's tones and ADPCM samples
    const Ps1SoundData* pPs1Data = resMan.GetPs1SoundData();
    const bool ps1Vab = pPs1Data && pPs1Data->Has(info.mVhFile) && pPs1Data->Has(info.mVbFile);

    info.mVhFileData = resMan.LoadSoundFile(info.mVhFile.c_str(), info.mSoundTheme);
    if (!ps1Vab)
    {
        VabFile::RestorePs1Tones(mConfig.mGame, info.mVhFile, info.mVhFileData);
    }
    const std::vector<u8> vbData = resMan.LoadSoundFile(info.mVbFile.c_str(), info.mSoundTheme);

    // SsVabOpenHead
    VabHeader header = {};
    if (info.mVhFileData.size() < sizeof(VabHeader))
    {
        info.mVabId = -1;
        return;
    }
    memcpy(&header, info.mVhFileData.data(), sizeof(VabHeader));
    const s32 vabId = header.field_8_id;
    const size_t headerSize = sizeof(VabHeader) + static_cast<size_t>(std::clamp<s32>(header.field_12_num_progs, 0, 128)) * 16 * sizeof(VagAtr);
    if (vabId < 0 || vabId >= kMaxVabs || info.mVhFileData.size() < headerSize)
    {
        info.mVabId = -1;
        return;
    }

    CloseVab(vabId);
    PsxSoundEngine& engine = Engine();
    engine.OpenVab(vabId, info.mVhFileData.data(), static_cast<u32>(headerSize));
    mOpenVabs |= 1u << vabId;
    info.mVabId = vabId;
    engine.Record("VAB %d %s %s %s", info.mVabId, info.mSoundTheme.c_str(), info.mVhFile.c_str(), info.mVbFile.c_str());

    const s32 vagCount = std::min<s32>(header.field_16_num_vags, 255);
    if (ps1Vab)
    {
        // The samples are ADPCM VAGs: decode them for the emulated SPU
        const std::vector<Ps1SoundData::Sample> samples = Ps1SoundData::DecodeVab(info.mVhFileData, vbData);
        for (std::size_t i = 0; i < samples.size(); i++)
        {
            const Ps1SoundData::Sample& sample = samples[i];
            if (!sample.mPcm.empty())
            {
                engine.SetSample(vabId, static_cast<s32>(i), sample.mPcm.data(), static_cast<u32>(sample.mPcm.size()), sample.mLoop, sample.mLoopStart);
            }
        }
    }
    else if (mConfig.mGame == GameType::eAe)
    {
        LoadVabBodyAE(resMan.mFs, engine, vabId, vbData, vagCount);
    }
    else
    {
        LoadVabBodyAO(engine, vabId, vbData, vagCount);
    }
}

void GameSound::LoadVabs(std::shared_ptr<PathSoundInfo>& info, s32 reverb, ResourceManagerWrapper& resMan)
{
    auto oldPtr = mLastLoadedSoundBlockInfo.lock();
    if (oldPtr.get() == info.get())
    {
        return;
    }

    PsxSoundEngine& engine = Engine();
    engine.SetReverbEnabled(false);
    engine.SetReverbDepth(0, 0);
    engine.ClearReverb();

    mLastLoadedSoundBlockInfo = info;
    LoadVab(*info, resMan);

    engine.SetReverbDepth(reverb, reverb);
    engine.SetReverbEnabled(true);
}

void GameSound::PendSoundFiles(const PathSoundInfo& info, ResourceManagerWrapper& resMan)
{
    resMan.PendSoundFile(info.mVhFile, info.mSoundTheme);
    resMan.PendSoundFile(info.mVbFile, info.mSoundTheme);
    for (const auto& seqName : info.mSeqFiles)
    {
        resMan.PendSoundFile(seqName, info.mSoundTheme);
    }
}

void GameSound::LoadSeqs(OpenSeqHandle* pSeqTable, const PathSoundInfo& info, ResourceManagerWrapper& resMan)
{
    if (!pSeqTable)
    {
        return;
    }
    mSeqTable = pSeqTable;

    // Generate resource ids from hashing the name if we haven't already
    if (mNeedToHashSeqNames)
    {
        for (s32 i = 0; i < SeqTableSize(); i++)
        {
            mSeqTable[i].field_C_ppSeq_Data.clear();
            mSeqTable[i].field_A_id_seqOpenId = -1;
            mSeqTable[i].field_4_generated_res_id = ResourceManagerWrapper::SEQ_HashName(mSeqTable[i].field_0_mBsqName);
        }
        mNeedToHashSeqNames = false;
    }

    // We have to insert into the table at the position that matches the file name
    for (const auto& seqName : info.mSeqFiles)
    {
        s32 idx = -1;
        for (s32 i = 0; i < SeqTableSize(); i++)
        {
            if (strcmp(mSeqTable[i].field_0_mBsqName, seqName.c_str()) == 0)
            {
                idx = i;
                break;
            }
        }
        if (idx < 0)
        {
            ALIVE_FATAL("Couldn't find seq name in the table");
        }
        mSeqTable[idx].field_C_ppSeq_Data = resMan.LoadSoundFile(seqName.c_str(), info.mSoundTheme);
    }
}

// ---- Sound effects ----

// Logs the result, for replaying a run on the PS1's sound effect code with the same choices
static s32 SfxRandom(s32 min, s32 max)
{
    const s32 value = Math_RandomRange(min, max);
    SoundDebug::LogSeqOutput("RAND %d %d %d", min, max, value);
    return value;
}

// The old PC code had a switch for the pitch variation, which was always on. It's still in the log
// the reference harness reads.
static constexpr s32 kPitchVariationLogged = 1;

// SsVoKeyOn
s32 GameSound::PlayNote(s32 vabIdAndProgram, s32 pitch, s32 leftVol, s32 rightVol)
{
    const s32 voices = Engine().VoKeyOn(vabIdAndProgram, pitch, static_cast<u16>(leftVol), static_cast<u16>(rightVol));
    SoundDebug::LogSeqOutput("KEYON %d %d %d %d %d", voices, vabIdAndProgram, pitch, static_cast<u16>(leftVol), static_cast<u16>(rightVol));
    return voices;
}

// Note: Inlined in psx (SFX_SetPitch_4CA510 + the pitch code of SND_MIDI)
// pitch: the offset in 1/128 semitones. libsnd only changes a voice that is playing that VAB, program
// and note.
void GameSound::SetChannelsPitch(s32 channelsBits, s32 vabId, s32 program, s32 note, s16 pitch)
{
    s32 v3 = 0;
    s16 v4 = 0;

    if (pitch >= 0)
    {
        v3 = (pitch >> 7) & 0xFFFF;
        v4 = pitch & 127;
    }
    else
    {
        v3 = -1 - (-pitch >> 7);
        v4 = 127 - (-(s8) pitch & 127);
    }

    for (s16 i = 0; i < PsxSpu::kNumVoices; i++)
    {
        if ((1 << i) & channelsBits)
        {
            SoundDebug::LogSeqOutput("PITCH %d %d %d %d %d %d %d", i, vabId, program, note, 0, note + v3, v4);
            Engine().UtChangePitch(i, static_cast<s16>(vabId), static_cast<s16>(program), static_cast<s16>(note), 0, static_cast<s16>(note + v3), v4);
        }
    }
}

s32 GameSound::ApplyPitchVariation(const relive::SfxDefinition& sfxDef, s32 voices, s16 pitch_min, s16 pitch_max)
{
    if (pitch_min || pitch_max)
    {
        SetChannelsPitch(voices, LastLoadedVabId(), sfxDef.mProgram, sfxDef.mNote, SfxRandom(pitch_min, pitch_max));
    }
    return voices;
}

s32 GameSound::SfxPlayMono(const relive::SfxDefinition& sfxDef, s32 volume, s32 pitch_min, s32 pitch_max)
{
    SoundDebug::MusicScope scope;
    SoundDebug::LogSeqCall("SFXM %d %d %d %d %d %d %d %d %d %d %d", sfxDef.mBlockIdx, sfxDef.mProgram, sfxDef.mNote, sfxDef.mDefaultVolume, sfxDef.mPitchMin, sfxDef.mPitchMax,
        volume, pitch_min, pitch_max, LastLoadedVabId(), kPitchVariationLogged);
    if (!volume)
    {
        volume = sfxDef.mDefaultVolume;
    }

    if (pitch_min == 0x7FFF)
    {
        pitch_min = sfxDef.mPitchMin;
    }

    if (pitch_max == 0x7FFF)
    {
        pitch_max = sfxDef.mPitchMax;
    }

    // AO's libsnd clamps to 10, AE's to 1 (checked against the PS1 executables)
    const s32 minVolume = mConfig.mGame == GameType::eAo ? 10 : 1;
    volume = std::clamp<s32>(volume, minVolume, 127);

    // Note: Inlined in psx
    const s32 voices = PlayNote(sfxDef.mProgram | (LastLoadedVabId() << 8), sfxDef.mNote << 8, volume, volume);
    return ApplyPitchVariation(sfxDef, voices, static_cast<s16>(pitch_min), static_cast<s16>(pitch_max));
}

s32 GameSound::SfxPlayStereo(const relive::SfxDefinition& sfxDef, s16 volLeft, s16 volRight, s16 pitch_min, s16 pitch_max)
{
    SoundDebug::MusicScope scope;
    SoundDebug::LogSeqCall("SFXS %d %d %d %d %d %d %d %d %d %d %d %d", sfxDef.mBlockIdx, sfxDef.mProgram, sfxDef.mNote, sfxDef.mDefaultVolume, sfxDef.mPitchMin, sfxDef.mPitchMax,
        volLeft, volRight, pitch_min, pitch_max, LastLoadedVabId(), kPitchVariationLogged);
    if (pitch_min == 0x7FFF)
    {
        pitch_min = sfxDef.mPitchMin;
    }

    if (pitch_max == 0x7FFF)
    {
        pitch_max = sfxDef.mPitchMax;
    }

    volLeft = std::clamp<s16>(volLeft, 10, 127);
    volRight = std::clamp<s16>(volRight, 10, 127);

    // Note: Inlined in psx
    const s32 voices = PlayNote(sfxDef.mProgram | (LastLoadedVabId() << 8), sfxDef.mNote << 8, volLeft, volRight);
    return ApplyPitchVariation(sfxDef, voices, pitch_min, pitch_max);
}

s32 GameSound::PlayMidi(s32 program, s32 vabId, s32 note, s16 vol, s16 min, s16 max)
{
    SoundDebug::MusicScope scope;
    SoundDebug::LogSeqCall("SNDMIDI %d %d %d %d %d %d %d", program, vabId, note, vol, min, max, kPitchVariationLogged);
    const s32 volClamped = std::clamp<s32>(vol, 10, 127);

    // Note: Inlined in psx. The parameters are named the wrong way round: program is the VAB
    // (the high byte) and vabId the program.
    const s32 voices = PlayNote(vabId | ((s16) program << 8), note << 8, volClamped, volClamped);
    if (min || max)
    {
        SetChannelsPitch(voices, program, vabId, note, SfxRandom(min, max));
    }
    return voices;
}

void GameSound::StopChannelsMask(u32 bitMask)
{
    for (s32 i = 0; i < PsxSpu::kNumVoices; i++)
    {
        // Does the index match a bit in the bitmask?
        if ((1 << i) & bitMask)
        {
            Engine().UtKeyOffV(static_cast<s16>(i));
        }
    }
}

// ---- SEQs ----

void GameSound::StopAllSeqs()
{
    mSeqsPlayingCount = 0;
    PsxSoundEngine& engine = Engine();
    for (s16 i = 0; i < Libsnd::kMaxSeqs; i++)
    {
        if (mOpenSeqs[i] >= 0)
        {
            if (engine.IsEos(i))
            {
                engine.SeqStop(i);
            }
            engine.SeqClose(i);
            mSeqTable[mOpenSeqs[i]].field_A_id_seqOpenId = -1;
            mOpenSeqs[i] = -1;
        }
    }
}

// Closes the SEQs that have finished
void GameSound::StopFinishedSeqs()
{
    PsxSoundEngine& engine = Engine();
    for (s16 i = 0; i < Libsnd::kMaxSeqs; i++)
    {
        if (mOpenSeqs[i] >= 0 && !engine.IsEos(i))
        {
            engine.SeqClose(i);
            mSeqTable[mOpenSeqs[i]].field_A_id_seqOpenId = -1;
            mOpenSeqs[i] = -1;
            mSeqsPlayingCount--;
        }
    }
}

bool GameSound::OpenSeq(u16 idx)
{
    OpenSeqHandle& rec = mSeqTable[idx];
    if (rec.field_A_id_seqOpenId >= 0)
    {
        return true;
    }

    // Too many playing: close the ones that are done, and if none where there's no room
    if (mSeqsPlayingCount >= kMaxPlayingSeqs)
    {
        StopFinishedSeqs();
        if (mSeqsPlayingCount >= kMaxPlayingSeqs)
        {
            return false;
        }
    }

    rec.field_A_id_seqOpenId = Engine().SeqOpen(rec.field_C_ppSeq_Data.data(), static_cast<u32>(rec.field_C_ppSeq_Data.size()), static_cast<s16>(LastLoadedVabId()));
    Engine().Record("SEQOPEN %d %d %d %s", rec.field_A_id_seqOpenId, rec.field_4_generated_res_id, LastLoadedVabId(), rec.field_0_mBsqName);
    if (rec.field_A_id_seqOpenId < 0 || rec.field_A_id_seqOpenId >= Libsnd::kMaxSeqs)
    {
        // libsnd couldn't open it
        rec.field_A_id_seqOpenId = -1;
        return false;
    }

    // Index into the IDS via the seq ID and map it to the index
    mOpenSeqs[rec.field_A_id_seqOpenId] = static_cast<s16>(idx);
    mSeqsPlayingCount++;
    return true;
}

s16 GameSound::SeqPlaySeq(u16 idx, s16 repeatCount, s16 bDontStop)
{
    SoundDebug::LogSeqOutput("PLAYSEQ %d %d %d", idx, repeatCount, bDontStop);
    OpenSeqHandle& rec = mSeqTable[idx];
    if (rec.field_C_ppSeq_Data.empty())
    {
        return 0;
    }

    PsxSoundEngine& engine = Engine();
    if (rec.field_A_id_seqOpenId < 0)
    {
        if (!OpenSeq(idx))
        {
            return 0;
        }
    }
    else if (engine.IsEos(rec.field_A_id_seqOpenId))
    {
        if (!bDontStop)
        {
            return 0;
        }
        engine.SeqStop(rec.field_A_id_seqOpenId);
    }

    const s16 clampedVol = std::clamp<s16>(rec.field_9_volume, 10, 127);
    engine.SeqSetVol(rec.field_A_id_seqOpenId, clampedVol, clampedVol);
    engine.SeqPlay(rec.field_A_id_seqOpenId, 1, repeatCount);
    return 1;
}

s16 GameSound::SeqPlay(u16 idx, s16 repeatCount, s16 volLeft, s16 volRight)
{
    SoundDebug::LogSeqOutput("PLAY %d %d %d %d", idx, repeatCount, volLeft, volRight);
    OpenSeqHandle& rec = mSeqTable[idx];
    if (rec.field_C_ppSeq_Data.empty())
    {
        return 0;
    }

    PsxSoundEngine& engine = Engine();
    if (rec.field_A_id_seqOpenId < 0)
    {
        if (!OpenSeq(idx))
        {
            return 0;
        }
    }
    else if (engine.IsEos(rec.field_A_id_seqOpenId))
    {
        engine.SeqStop(rec.field_A_id_seqOpenId);
    }

    engine.SeqSetVol(rec.field_A_id_seqOpenId, std::clamp<s16>(volLeft, 10, 127), std::clamp<s16>(volRight, 10, 127));
    engine.SeqPlay(rec.field_A_id_seqOpenId, 1, repeatCount);
    return 1;
}

s32 GameSound::SeqIsPlaying(u16 idx)
{
    OpenSeqHandle& rec = mSeqTable[idx];
    if (rec.field_A_id_seqOpenId != -1 && !rec.field_C_ppSeq_Data.empty())
    {
        return Engine().IsEos(rec.field_A_id_seqOpenId) != 0;
    }
    return 0;
}

void GameSound::SeqSetVol(s32 idx, s16 volLeft, s16 volRight)
{
    SoundDebug::LogSeqOutput("SETVOL %d %d %d", idx & 0xFFFF, volLeft, volRight);
    const u16 limitedIdx = idx & 0xFFFF;
    if (SeqIsPlaying(limitedIdx))
    {
        Engine().SeqSetVol(mSeqTable[limitedIdx].field_A_id_seqOpenId, volLeft, volRight);
    }
}

void GameSound::SeqStop(u16 idx)
{
    SoundDebug::LogSeqOutput("STOP %d", idx);
    if (SeqIsPlaying(idx))
    {
        Engine().SeqStop(mSeqTable[idx].field_A_id_seqOpenId);
    }
}

// Stops the music, the ambience, the SEQs, the objects' sounds and every voice
void GameSound::StopAll()
{
    if (mConfig.mEnableMusic)
    {
        mConfig.mEnableMusic(0);
    }
    BackgroundMusic::Stop();
    SND_Reset_Ambiance();
    StopAllSeqs();
    for (s32 i = 0; i < gBaseGameObjects->Size(); i++)
    {
        BaseGameObject* pObj = gBaseGameObjects->ItemAt(i);
        if (!pObj)
        {
            break;
        }

        if (!pObj->GetDead())
        {
            pObj->VStopAudio();
        }
    }
    Engine().UtAllKeyOff();
}

// Starts the music, and the sounds of the objects in the cameras around, again
void GameSound::Restart(BaseMap& map)
{
    if (mConfig.mEnableMusic)
    {
        mConfig.mEnableMusic(1);
    }
    BackgroundMusic::Play();
    map.Start_Sounds_For_Objects_In_Near_Cameras();
}

// ---- The functions the game calls ----

void SND_Init(const GameSoundConfig& config)
{
    sGameSound = std::make_unique<GameSound>(config);
}

void SND_Shutdown()
{
    if (sGameSound)
    {
        sGameSound->Shutdown();
    }
}

void SND_SsQuit()
{
    sGameSound.reset();
}

void SND_Reset()
{
    if (sGameSound)
    {
        sGameSound->Reset();
    }
}

void SND_Pend_Sound_Files(const PathSoundInfo& info, ResourceManagerWrapper& resMan)
{
    if (sGameSound)
    {
        sGameSound->PendSoundFiles(info, resMan);
    }
}

void SND_Load_VABS(std::shared_ptr<PathSoundInfo>& info, s32 reverb, ResourceManagerWrapper& resMan)
{
    if (sGameSound)
    {
        sGameSound->LoadVabs(info, reverb, resMan);
    }
}

void SND_Load_Seqs(OpenSeqHandle* pSeqTable, std::shared_ptr<PathSoundInfo>& info, ResourceManagerWrapper& resMan)
{
    if (sGameSound)
    {
        sGameSound->LoadSeqs(pSeqTable, *info, resMan);
    }
}

s8 SND_Seq_Table_Valid()
{
    return sGameSound && sGameSound->SeqTable() != nullptr;
}

void SND_StopAll()
{
    if (sGameSound)
    {
        sGameSound->StopAll();
    }
}

void SND_Restart(BaseMap& map)
{
    if (sGameSound)
    {
        sGameSound->Restart(map);
    }
}

void SND_Stop_All_Seqs()
{
    if (sGameSound)
    {
        sGameSound->StopAllSeqs();
    }
}

void SND_Stop_Channels_Mask(u32 bitMask)
{
    if (sGameSound)
    {
        sGameSound->StopChannelsMask(bitMask);
    }
}

void SND_AllKeyOff()
{
    if (sGameSound)
    {
        sGameSound->Engine().UtAllKeyOff();
    }
}

void SND_SEQ_Stop(u16 idx)
{
    if (sGameSound)
    {
        sGameSound->SeqStop(idx);
    }
}

s16 SND_SEQ_PlaySeq(u16 idx, s16 repeatCount, s16 bDontStop)
{
    return sGameSound ? sGameSound->SeqPlaySeq(idx, repeatCount, bDontStop) : 0;
}

void SND_SEQ_SetVol(s32 idx, s16 volLeft, s16 volRight)
{
    if (sGameSound)
    {
        sGameSound->SeqSetVol(idx, volLeft, volRight);
    }
}

s16 SND_SEQ_Play(u16 idx, s16 repeatCount, s16 volLeft, s16 volRight)
{
    return sGameSound ? sGameSound->SeqPlay(idx, repeatCount, volLeft, volRight) : 0;
}

s32 SND_SsIsEos_DeInlined(u16 idx)
{
    return sGameSound ? sGameSound->SeqIsPlaying(idx) : 0;
}

s32 SFX_SfxDefinition_Play_Stereo(const relive::SfxDefinition& sfxDef, s16 volLeft, s16 volRight, s16 pitch_min, s16 pitch_max)
{
    return sGameSound ? sGameSound->SfxPlayStereo(sfxDef, volLeft, volRight, pitch_min, pitch_max) : 0;
}

s32 SFX_SfxDefinition_Play_Mono(const relive::SfxDefinition& sfxDef, s32 volume, s32 pitch_min, s32 pitch_max)
{
    return sGameSound ? sGameSound->SfxPlayMono(sfxDef, volume, pitch_min, pitch_max) : 0;
}

s32 SND_MIDI(s32 program, s32 vabId, s32 note, s16 vol, s16 min, s16 max)
{
    return sGameSound ? sGameSound->PlayMidi(program, vabId, note, vol, min, max) : 0;
}
