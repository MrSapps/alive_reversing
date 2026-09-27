#include "../stdafx.h"
#include "VabSoundFont.hpp"
#include "PsxSpuApi.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <sstream>

static constexpr size_t kTonesPerProgram = 16;

// The first VagAtr of the tone blocks, if the VH is big enough for all of them
static const VagAtr* ToneBlocks(const std::vector<u8>& vh)
{
    if (vh.size() < sizeof(VabHeader))
    {
        return nullptr;
    }
    const VabHeader* pHeader = reinterpret_cast<const VabHeader*>(vh.data());
    if (pHeader->field_12_num_progs < 0 || vh.size() < sizeof(VabHeader) + pHeader->field_12_num_progs * kTonesPerProgram * sizeof(VagAtr))
    {
        return nullptr;
    }
    return reinterpret_cast<const VagAtr*>(vh.data() + sizeof(VabHeader));
}

s32 VabSoundFont::VagCount(const std::vector<u8>& vh)
{
    if (!ToneBlocks(vh))
    {
        return -1;
    }
    // As SsVabOpenHead does it
    s32 numVags = reinterpret_cast<const VabHeader*>(vh.data())->field_16_num_vags;
    if (numVags > 256)
    {
        numVags = 255;
    }
    return static_cast<u8>(numVags);
}

template <typename T>
static T ReadLE(const std::vector<u8>& data, size_t pos)
{
    T v = {};
    memcpy(&v, data.data() + pos, sizeof(T));
    return v;
}

// sampleLen samples from data at pos, zero filled past its end (as a short read leaves them)
static std::vector<s16> ReadPcm(const std::vector<u8>& data, size_t pos, s32 sampleLen)
{
    std::vector<s16> pcm(sampleLen);
    if (pos < data.size())
    {
        const size_t bytes = std::min(static_cast<size_t>(sampleLen) * 2, data.size() - pos);
        memcpy(pcm.data(), data.data() + pos, bytes);
    }
    return pcm;
}

bool VabSoundFont::ReadVbAe(const std::vector<u8>& vh, const std::vector<u8>& vb, const std::vector<u8>& soundsDat, std::vector<Sample>& samples)
{
    const s32 count = VagCount(vh);
    if (count < 0)
    {
        return false;
    }

    samples.clear();
    samples.resize(count);
    for (s32 i = 0; i < count; i++)
    {
        const size_t recordPos = static_cast<size_t>(i) * sizeof(VabBodyRecord);
        if (recordPos + sizeof(VabBodyRecord) > vb.size())
        {
            continue;
        }
        const u32 length = ReadLE<u32>(vb, recordPos);
        const s32 flags = ReadLE<s32>(vb, recordPos + 4);
        const s32 offset = ReadLE<s32>(vb, recordPos + 8);

        // As SND_SoundsDat_Get_Sample_Len_4FC400 and SND_SoundsDat_Read_4FC4E0 do it
        const s32 sampleLen = static_cast<s32>(8 * length) / 16;
        samples[i].mLoop = flags < 0;
        if (sampleLen > 0)
        {
            samples[i].mPcm = offset == -1 ? std::vector<s16>(sampleLen) : ReadPcm(soundsDat, static_cast<u32>(offset), sampleLen);
        }
    }
    return true;
}

bool VabSoundFont::ReadVbAo(const std::vector<u8>& vh, const std::vector<u8>& vb, std::vector<Sample>& samples)
{
    const s32 count = VagCount(vh);
    if (count < 0)
    {
        return false;
    }

    samples.clear();
    samples.resize(count);
    size_t pos = 0;
    for (s32 i = 0; i < count; i++)
    {
        if (pos + 8 > vb.size())
        {
            break;
        }
        const s32 length = ReadLE<s32>(vb, pos);
        const s32 flags = ReadLE<s32>(vb, pos + 4);

        // As AO's SsVabTransBody does it
        const s32 sampleLen = (8 * length) / 16;
        samples[i].mLoop = flags < 0;
        if (sampleLen > 0)
        {
            samples[i].mPcm = ReadPcm(vb, pos + 8, sampleLen);
        }
        pos += static_cast<u32>(length) + 8;
    }
    return true;
}

// ---------------------------------------------------------------------------------------------
// Standard SF2 approximations of the PS1 values. Only SF2 players use these: the engine uses the
// raw values.

static u16 VolToCentibels(s32 vol)
{
    if (vol <= 0)
    {
        return 1440;
    }
    return static_cast<u16>(std::clamp(static_cast<s32>(std::lround(200.0 * std::log10(127.0 / vol))), 0, 1440));
}

static s32 CentibelsToVol(s16 cb)
{
    return std::clamp(static_cast<s32>(std::lround(127.0 * std::pow(10.0, -cb / 200.0))), 1, 127);
}

static s16 PanToSf2(s32 pan)
{
    return static_cast<s16>(std::lround((pan - 64) * 500.0 / 64.0));
}

static s32 Sf2ToPan(s16 pan)
{
    return std::clamp(64 + static_cast<s32>(std::lround(pan * 64.0 / 500.0)), 0, 127);
}

static s16 MsToTimecents(f64 ms)
{
    if (ms <= 1.0)
    {
        return -12000;
    }
    return static_cast<s16>(std::clamp(static_cast<s32>(std::lround(1200.0 * std::log2(ms / 1000.0))), -12000, 8000));
}

// The envelope the engine makes from adsr1/adsr2 (see SsVabOpenHead), as SF2 timings
static void AddEnvelope(SoundFont::Zone& zone, s16 adsr1, s16 adsr2)
{
    const f64 attackMs = std::min(std::pow(2.0, ((adsr1 >> 8) & 0x7F) * 0.25) * 0.09, 32767.0);
    const s32 sustainLevel = ((adsr1 >> 4) & 0xF) * 16 / 15; // Of 16
    const f64 decayMs = std::min((2 * (~adsr1 & 0xF)) / 15.0 * 600.0, 32767.0);
    const f64 releaseMs = std::min(std::pow(2.0, adsr2 & 0x1F) * 0.045, 32767.0);

    zone.Add(SoundFont::eAttackVolEnv, static_cast<u16>(MsToTimecents(attackMs)));
    zone.Add(SoundFont::eDecayVolEnv, static_cast<u16>(MsToTimecents(decayMs)));
    zone.Add(SoundFont::eSustainVolEnv, sustainLevel >= 16 ? static_cast<u16>(0) : VolToCentibels(std::max(sustainLevel, 0) * 127 / 16));
    zone.Add(SoundFont::eReleaseVolEnv, static_cast<u16>(MsToTimecents(releaseMs)));
}

static std::string HeaderComment(const VabHeader& h)
{
    char_type buf[256] = {};
    snprintf(buf, sizeof(buf), "relive_vab id=%d version=%d progs=%d tones=%d vags=%d master_vol=%d master_pan=%d attr1=%d attr2=%d",
        h.field_8_id, h.field_4_version, h.field_12_num_progs, h.field_14_num_tones, h.field_16_num_vags,
        h.field_18_master_vol, h.field_19_master_pan, h.field_1A_attr1, h.field_1B_attr2);
    return buf;
}

// The key=value pairs of HeaderComment, if the comment is one
static std::map<std::string, s32> ParseHeaderComment(const std::string& comment)
{
    std::map<std::string, s32> values;
    std::istringstream words(comment);
    std::string word;
    if (!(words >> word) || word != "relive_vab")
    {
        return values;
    }
    while (words >> word)
    {
        const size_t eq = word.find('=');
        if (eq != std::string::npos)
        {
            values[word.substr(0, eq)] = std::atoi(word.substr(eq + 1).c_str());
        }
    }
    return values;
}

static u16 Raw(s32 value)
{
    return static_cast<u16>(static_cast<s16>(value));
}

SoundFont VabSoundFont::ToSoundFont(const Vab& vab, const std::string& name)
{
    SoundFont sf2;
    sf2.mName = name;

    const VagAtr* pBlocks = ToneBlocks(vab.mVh);
    if (!pBlocks)
    {
        return sf2;
    }
    const VabHeader& header = *reinterpret_cast<const VabHeader*>(vab.mVh.data());
    sf2.mComment = HeaderComment(header);

    for (size_t i = 0; i < vab.mSamples.size(); i++)
    {
        const Sample& sample = vab.mSamples[i];
        SoundFont::Sample s;
        s.mName = "vag " + std::to_string(i + 1);
        s.mPcm = sample.mPcm;
        // A looping sample loops all of it. The engine only loops samples with a loop, so
        // loop points are only set on those.
        if (sample.mLoop)
        {
            s.mLoopStart = 0;
            s.mLoopEnd = static_cast<u32>(sample.mPcm.size());
        }
        sf2.mSamples.push_back(std::move(s));
    }

    // The tones SsVabOpenHead would load: the ones with a volume, a later block's tone replacing
    // an earlier one of the same program + tone index
    std::map<s32, std::map<size_t, VagAtr>> programs;
    for (s32 block = 0; block < header.field_12_num_progs; block++)
    {
        for (size_t tone = 0; tone < kTonesPerProgram; tone++)
        {
            const VagAtr& atr = pBlocks[block * kTonesPerProgram + tone];
            if (atr.field_2_vol > 0 && atr.field_14_prog >= 0 && atr.field_14_prog < 128)
            {
                programs[atr.field_14_prog][tone] = atr;
            }
        }
    }

    for (const auto& program : programs)
    {
        const s32 progNum = program.first;
        SoundFont::Instrument inst;
        inst.mName = "prog " + std::to_string(progNum);

        for (const auto& toneEntry : program.second)
        {
            const VagAtr& atr = toneEntry.second;
            SoundFont::Zone zone;
            zone.AddRange(SoundFont::eKeyRange, static_cast<u8>(std::clamp<s32>(atr.field_6_min, 0, 127)), static_cast<u8>(std::clamp<s32>(atr.field_7_max, 0, 127)));
            zone.Add(SoundFont::eOverridingRootKey, static_cast<u16>(std::min<s32>(atr.field_4_centre, 127)));
            zone.Add(SoundFont::eFineTune, Raw(-static_cast<s32>(std::lround(atr.field_5_shift * 100.0 / 128.0))));
            zone.Add(SoundFont::eInitialAttenuation, VolToCentibels(atr.field_2_vol));
            zone.Add(SoundFont::ePan, Raw(PanToSf2(atr.field_3_pan)));
            zone.Add(SoundFont::eReverbEffectsSend, static_cast<u16>((atr.field_1_mode & 4) ? 1000 : 0));
            AddEnvelope(zone, atr.field_10_adsr1, atr.field_12_adsr2);

            const s32 sampleIdx = static_cast<u8>(atr.field_16_vag) - 1;
            const bool haveSample = sampleIdx >= 0 && sampleIdx < static_cast<s32>(vab.mSamples.size());
            zone.Add(SoundFont::eSampleModes, static_cast<u16>(haveSample && vab.mSamples[sampleIdx].mLoop ? 1 : 0));

            zone.Add(ePsxTone, static_cast<u16>(toneEntry.first));
            zone.Add(ePsxPriority, Raw(atr.field_0_priority));
            zone.Add(ePsxMode, Raw(atr.field_1_mode));
            zone.Add(ePsxVol, Raw(atr.field_2_vol));
            zone.Add(ePsxPan, Raw(atr.field_3_pan));
            zone.Add(ePsxCentre, atr.field_4_centre);
            zone.Add(ePsxShift, atr.field_5_shift);
            zone.Add(ePsxMin, Raw(atr.field_6_min));
            zone.Add(ePsxMax, Raw(atr.field_7_max));
            zone.Add(ePsxVibW, Raw(atr.field_8_vibW));
            zone.Add(ePsxVibT, Raw(atr.field_9_vibT));
            zone.Add(ePsxPorW, Raw(atr.field_A_porW));
            zone.Add(ePsxPorT, Raw(atr.field_B_porT));
            zone.Add(ePsxPitchBendMin, Raw(atr.field_C_pitch_bend_min));
            zone.Add(ePsxPitchBendMax, Raw(atr.field_D_pitch_bend_max));
            zone.Add(ePsxAdsr1, Raw(atr.field_10_adsr1));
            zone.Add(ePsxAdsr2, Raw(atr.field_12_adsr2));
            zone.Add(ePsxVag, Raw(atr.field_16_vag));

            // Every zone but a global one needs a sample. A tone whose VAG doesn't exist plays
            // nothing in the engine, ePsxVag keeps its real number.
            if (vab.mSamples.empty())
            {
                continue;
            }
            zone.Add(SoundFont::eSampleId, static_cast<u16>(haveSample ? sampleIdx : 0));
            inst.mZones.push_back(std::move(zone));
        }

        const ProgAtr& progAtr = header.field_20_progs[progNum];
        SoundFont::Zone global;
        global.Add(ePsxProgTones, progAtr.field_0_num_tones);
        global.Add(ePsxProgVol, Raw(progAtr.field_1_vol));
        global.Add(ePsxProgPriority, Raw(progAtr.field_2_priority));
        global.Add(ePsxProgMode, Raw(progAtr.field_3_mode));
        global.Add(ePsxProgPan, Raw(progAtr.field_4_pan));
        global.Add(ePsxProgAttr, Raw(progAtr.field_6_attr));

        SoundFont::Zone instZone;
        instZone.Add(SoundFont::eInstrument, static_cast<u16>(sf2.mInstruments.size()));

        SoundFont::Preset preset;
        preset.mName = inst.mName;
        preset.mProgram = static_cast<u16>(progNum);
        preset.mBank = 0;
        preset.mZones.push_back(std::move(global));
        preset.mZones.push_back(std::move(instZone));
        sf2.mPresets.push_back(std::move(preset));
        sf2.mInstruments.push_back(std::move(inst));
    }
    return sf2;
}

// A tone from a zone: the raw PS1 values when it has them, else the nearest ones to its SF2 values
static VagAtr ZoneToVagAtr(const SoundFont& sf2, const SoundFont::Zone& zone, s32 program)
{
    auto get = [&](u16 oper, s32 fallback) -> s32
    {
        const SoundFont::Generator* pGen = zone.Find(oper);
        return pGen ? pGen->Signed() : fallback;
    };

    const SoundFont::Generator* pSampleGen = zone.Find(SoundFont::eSampleId);
    const s32 sampleIdx = pSampleGen ? pSampleGen->mAmount : 0;
    const SoundFont::Sample* pSample = sampleIdx < static_cast<s32>(sf2.mSamples.size()) ? &sf2.mSamples[sampleIdx] : nullptr;

    const SoundFont::Generator* pKeyRange = zone.Find(SoundFont::eKeyRange);
    const s32 rootKey = get(SoundFont::eOverridingRootKey, pSample ? pSample->mOriginalPitch : 60);
    const s32 fineTune = get(SoundFont::eFineTune, 0) + (pSample ? pSample->mPitchCorrection : 0);
    // The key a tone plays its sample at its own pitch at: centre + shift / 128 on the PS1,
    // root key - fine tune / 100 in SF2
    const s32 pitch128 = std::clamp(rootKey * 128 - static_cast<s32>(std::lround(fineTune * 128.0 / 100.0)), 0, 127 * 128 + 127);
    const s32 centre = pitch128 / 128;
    const s32 shift = pitch128 % 128;

    VagAtr atr = {};
    atr.field_0_priority = static_cast<s8>(get(VabSoundFont::ePsxPriority, 7));
    atr.field_1_mode = static_cast<s8>(get(VabSoundFont::ePsxMode, get(SoundFont::eReverbEffectsSend, 0) > 0 ? 4 : 0));
    atr.field_2_vol = static_cast<s8>(get(VabSoundFont::ePsxVol, CentibelsToVol(static_cast<s16>(get(SoundFont::eInitialAttenuation, 0)))));
    atr.field_3_pan = static_cast<s8>(get(VabSoundFont::ePsxPan, Sf2ToPan(static_cast<s16>(get(SoundFont::ePan, 0)))));
    atr.field_4_centre = static_cast<u8>(get(VabSoundFont::ePsxCentre, centre));
    atr.field_5_shift = static_cast<u8>(get(VabSoundFont::ePsxShift, shift));
    atr.field_6_min = static_cast<s8>(get(VabSoundFont::ePsxMin, pKeyRange ? pKeyRange->Lo() : 0));
    atr.field_7_max = static_cast<s8>(get(VabSoundFont::ePsxMax, pKeyRange ? pKeyRange->Hi() : 127));
    atr.field_8_vibW = static_cast<s8>(get(VabSoundFont::ePsxVibW, 0));
    atr.field_9_vibT = static_cast<s8>(get(VabSoundFont::ePsxVibT, 0));
    atr.field_A_porW = static_cast<s8>(get(VabSoundFont::ePsxPorW, 0));
    atr.field_B_porT = static_cast<s8>(get(VabSoundFont::ePsxPorT, 0));
    atr.field_C_pitch_bend_min = static_cast<s8>(get(VabSoundFont::ePsxPitchBendMin, 2));
    atr.field_D_pitch_bend_max = static_cast<s8>(get(VabSoundFont::ePsxPitchBendMax, 2));
    // TODO: derive the PS1 envelope from the SF2 one when it's not there, 0 is no envelope
    atr.field_10_adsr1 = static_cast<s16>(get(VabSoundFont::ePsxAdsr1, 0));
    atr.field_12_adsr2 = static_cast<s16>(get(VabSoundFont::ePsxAdsr2, 0));
    atr.field_14_prog = static_cast<s16>(program);
    atr.field_16_vag = static_cast<s16>(get(VabSoundFont::ePsxVag, sampleIdx + 1));
    return atr;
}

bool VabSoundFont::FromSoundFont(const SoundFont& sf2, Vab& vab, std::string& error)
{
    const std::map<std::string, s32> headerValues = ParseHeaderComment(sf2.mComment);
    auto headerValue = [&](const char_type* pKey, s32 fallback)
    {
        const auto it = headerValues.find(pKey);
        return it != headerValues.end() ? it->second : fallback;
    };

    VabHeader header = {};
    header.field_0_form = 0x56414270; // "pBAV"
    header.field_4_version = headerValue("version", 7);
    header.field_8_id = headerValue("id", 0);
    header.field_14_num_tones = static_cast<s16>(headerValue("tones", 0));
    header.field_16_num_vags = static_cast<u16>(headerValue("vags", static_cast<s32>(sf2.mSamples.size())));
    header.field_18_master_vol = static_cast<s8>(headerValue("master_vol", 127));
    header.field_19_master_pan = static_cast<s8>(headerValue("master_pan", 64));
    header.field_1A_attr1 = static_cast<s8>(headerValue("attr1", 0));
    header.field_1B_attr2 = static_cast<s8>(headerValue("attr2", 0));

    if (header.field_8_id < 0 || header.field_8_id >= kMaxVabs)
    {
        error = "VAB id " + std::to_string(header.field_8_id) + " is out of range";
        return false;
    }

    // One block of 16 tones per program
    std::map<s32, std::vector<VagAtr>> blocks;
    for (const SoundFont::Preset& preset : sf2.mPresets)
    {
        if (preset.mBank != 0 || preset.mProgram >= 128)
        {
            continue;
        }
        const s32 program = preset.mProgram;
        std::vector<VagAtr>& block = blocks[program];
        block.resize(kTonesPerProgram, VagAtr{});

        for (const SoundFont::Zone& presetZone : preset.mZones)
        {
            if (presetZone.IsGlobal(SoundFont::eInstrument))
            {
                ProgAtr& atr = header.field_20_progs[program];
                auto get = [&](u16 oper, s32 fallback)
                {
                    const SoundFont::Generator* pGen = presetZone.Find(oper);
                    return pGen ? pGen->Signed() : fallback;
                };
                atr.field_0_num_tones = static_cast<u8>(get(ePsxProgTones, 0));
                atr.field_1_vol = static_cast<s8>(get(ePsxProgVol, 127));
                atr.field_2_priority = static_cast<s8>(get(ePsxProgPriority, 0));
                atr.field_3_mode = static_cast<s8>(get(ePsxProgMode, 0));
                atr.field_4_pan = static_cast<s8>(get(ePsxProgPan, 64));
                atr.field_6_attr = static_cast<s16>(get(ePsxProgAttr, 0));
                continue;
            }

            const SoundFont::Instrument& inst = sf2.mInstruments[presetZone.Find(SoundFont::eInstrument)->mAmount];
            size_t nextTone = 0;
            for (const SoundFont::Zone& zone : inst.mZones)
            {
                if (zone.IsGlobal(SoundFont::eSampleId))
                {
                    continue;
                }
                const SoundFont::Generator* pTone = zone.Find(ePsxTone);
                const size_t tone = pTone ? pTone->mAmount : nextTone;
                if (tone >= kTonesPerProgram)
                {
                    error = "program " + std::to_string(program) + " has more than 16 tones";
                    return false;
                }
                block[tone] = ZoneToVagAtr(sf2, zone, program);
                nextTone = tone + 1;
            }
        }
    }

    // As many blocks as the VAB had, an empty block (no tone has a volume) is ignored
    const s32 numProgs = std::max(headerValue("progs", 0), static_cast<s32>(blocks.size()));
    header.field_12_num_progs = static_cast<s16>(numProgs);

    vab.mVh.assign(reinterpret_cast<const u8*>(&header), reinterpret_cast<const u8*>(&header) + sizeof(header));
    s32 written = 0;
    for (const auto& block : blocks)
    {
        const u8* p = reinterpret_cast<const u8*>(block.second.data());
        vab.mVh.insert(vab.mVh.end(), p, p + kTonesPerProgram * sizeof(VagAtr));
        written++;
    }
    vab.mVh.resize(vab.mVh.size() + (numProgs - written) * kTonesPerProgram * sizeof(VagAtr) + 512, 0);

    vab.mSamples.clear();
    for (const SoundFont::Sample& s : sf2.mSamples)
    {
        Sample sample;
        sample.mPcm = s.mPcm;
        sample.mLoop = s.mLoopEnd > s.mLoopStart;
        vab.mSamples.push_back(std::move(sample));
    }
    return true;
}
