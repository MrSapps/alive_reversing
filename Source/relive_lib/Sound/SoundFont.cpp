#include "../stdafx.h"
#include "SoundFont.hpp"
#include <cstring>

// Every sample must be followed by at least 46 zero samples (SF2 2.04 section 6.1)
static constexpr u32 kSamplePadding = 46;

const SoundFont::Generator* SoundFont::Zone::Find(u16 oper) const
{
    for (const Generator& gen : mGens)
    {
        if (gen.mOper == oper)
        {
            return &gen;
        }
    }
    return nullptr;
}

bool SoundFont::Zone::IsGlobal(u16 terminalOper) const
{
    return mGens.empty() || mGens.back().mOper != terminalOper;
}

// ---------------------------------------------------------------------------------------------
// Writing

class RiffWriter final
{
public:
    void U8(u8 v)
    {
        mData.push_back(v);
    }

    void U16(u16 v)
    {
        U8(static_cast<u8>(v));
        U8(static_cast<u8>(v >> 8));
    }

    void U32(u32 v)
    {
        U16(static_cast<u16>(v));
        U16(static_cast<u16>(v >> 16));
    }

    void FourCC(const char_type* pId)
    {
        for (s32 i = 0; i < 4; i++)
        {
            U8(static_cast<u8>(pId[i]));
        }
    }

    // Fixed size, zero padded name (not always zero terminated in SF2 files, but ours are)
    void Name(const std::string& name, size_t size)
    {
        for (size_t i = 0; i < size; i++)
        {
            U8(i < name.size() && i < size - 1 ? static_cast<u8>(name[i]) : 0);
        }
    }

    // Starts a chunk, returns where its size goes for EndChunk
    size_t BeginChunk(const char_type* pId)
    {
        FourCC(pId);
        const size_t sizePos = mData.size();
        U32(0);
        return sizePos;
    }

    void EndChunk(size_t sizePos)
    {
        const u32 size = static_cast<u32>(mData.size() - sizePos - 4);
        for (s32 i = 0; i < 4; i++)
        {
            mData[sizePos + i] = static_cast<u8>(size >> (8 * i));
        }
        // Chunks are word aligned
        if (size & 1)
        {
            U8(0);
        }
    }

    size_t BeginList(const char_type* pType)
    {
        const size_t sizePos = BeginChunk("LIST");
        FourCC(pType);
        return sizePos;
    }

    // A zero terminated string chunk with an even size
    void StringChunk(const char_type* pId, const std::string& text)
    {
        const size_t pos = BeginChunk(pId);
        for (char_type c : text)
        {
            U8(static_cast<u8>(c));
        }
        U8(0);
        if (text.size() % 2 == 0)
        {
            U8(0);
        }
        EndChunk(pos);
    }

    std::vector<u8> mData;
};

template <typename TZoneOwner>
static void WriteZones(RiffWriter& w, const std::vector<TZoneOwner>& owners, const char_type* pBag, const char_type* pMod, const char_type* pGen)
{
    size_t pos = w.BeginChunk(pBag);
    u16 genIdx = 0;
    for (const TZoneOwner& owner : owners)
    {
        for (const SoundFont::Zone& zone : owner.mZones)
        {
            w.U16(genIdx);
            w.U16(0);
            genIdx = static_cast<u16>(genIdx + zone.mGens.size());
        }
    }
    w.U16(genIdx);
    w.U16(0);
    w.EndChunk(pos);

    // Only the terminal record: the default modulators are all that's used
    pos = w.BeginChunk(pMod);
    for (s32 i = 0; i < 5; i++)
    {
        w.U16(0);
    }
    w.EndChunk(pos);

    pos = w.BeginChunk(pGen);
    for (const TZoneOwner& owner : owners)
    {
        for (const SoundFont::Zone& zone : owner.mZones)
        {
            for (const SoundFont::Generator& gen : zone.mGens)
            {
                w.U16(gen.mOper);
                w.U16(gen.mAmount);
            }
        }
    }
    w.U32(0);
    w.EndChunk(pos);
}

std::vector<u8> SoundFont::Write() const
{
    RiffWriter w;
    const size_t riffPos = w.BeginChunk("RIFF");
    w.FourCC("sfbk");

    // INFO
    size_t listPos = w.BeginList("INFO");
    size_t pos = w.BeginChunk("ifil");
    w.U16(2); // SF2 version 2.01
    w.U16(1);
    w.EndChunk(pos);
    w.StringChunk("isng", "EMU8000");
    w.StringChunk("INAM", mName.empty() ? "Unnamed" : mName);
    if (!mComment.empty())
    {
        w.StringChunk("ICMT", mComment);
    }
    w.EndChunk(listPos);

    // Sample data
    std::vector<u32> sampleStarts;
    listPos = w.BeginList("sdta");
    pos = w.BeginChunk("smpl");
    u32 samplePos = 0;
    for (const Sample& sample : mSamples)
    {
        sampleStarts.push_back(samplePos);
        for (s16 s : sample.mPcm)
        {
            w.U16(static_cast<u16>(s));
        }
        for (u32 i = 0; i < kSamplePadding; i++)
        {
            w.U16(0);
        }
        samplePos += static_cast<u32>(sample.mPcm.size()) + kSamplePadding;
    }
    w.EndChunk(pos);
    w.EndChunk(listPos);

    // Presets, instruments and sample headers
    listPos = w.BeginList("pdta");

    pos = w.BeginChunk("phdr");
    u16 bagIdx = 0;
    for (const Preset& preset : mPresets)
    {
        w.Name(preset.mName, 20);
        w.U16(preset.mProgram);
        w.U16(preset.mBank);
        w.U16(bagIdx);
        w.U32(0); // library
        w.U32(0); // genre
        w.U32(0); // morphology
        bagIdx = static_cast<u16>(bagIdx + preset.mZones.size());
    }
    w.Name("EOP", 20);
    w.U16(0);
    w.U16(0);
    w.U16(bagIdx);
    w.U32(0);
    w.U32(0);
    w.U32(0);
    w.EndChunk(pos);
    WriteZones(w, mPresets, "pbag", "pmod", "pgen");

    pos = w.BeginChunk("inst");
    bagIdx = 0;
    for (const Instrument& inst : mInstruments)
    {
        w.Name(inst.mName, 20);
        w.U16(bagIdx);
        bagIdx = static_cast<u16>(bagIdx + inst.mZones.size());
    }
    w.Name("EOI", 20);
    w.U16(bagIdx);
    w.EndChunk(pos);
    WriteZones(w, mInstruments, "ibag", "imod", "igen");

    pos = w.BeginChunk("shdr");
    for (size_t i = 0; i < mSamples.size(); i++)
    {
        const Sample& sample = mSamples[i];
        const u32 start = sampleStarts[i];
        w.Name(sample.mName, 20);
        w.U32(start);
        w.U32(start + static_cast<u32>(sample.mPcm.size()));
        w.U32(start + sample.mLoopStart);
        w.U32(start + sample.mLoopEnd);
        w.U32(sample.mSampleRate);
        w.U8(sample.mOriginalPitch);
        w.U8(static_cast<u8>(sample.mPitchCorrection));
        w.U16(0); // sample link
        w.U16(1); // mono sample
    }
    w.Name("EOS", 20);
    for (s32 i = 0; i < 5; i++)
    {
        w.U32(0);
    }
    w.U16(0);
    w.U16(0);
    w.U16(0);
    w.EndChunk(pos);

    w.EndChunk(listPos);
    w.EndChunk(riffPos);
    return std::move(w.mData);
}

// ---------------------------------------------------------------------------------------------
// Reading

class RiffReader final
{
public:
    RiffReader(const u8* pData, size_t size)
        : mData(pData)
        , mSize(size)
    {
    }

    bool Has(size_t count) const
    {
        return mPos + count <= mSize;
    }

    u8 U8()
    {
        return mData[mPos++];
    }

    u16 U16()
    {
        const u16 v = static_cast<u16>(mData[mPos] | (mData[mPos + 1] << 8));
        mPos += 2;
        return v;
    }

    u32 U32()
    {
        const u32 v = static_cast<u32>(U16()) | (static_cast<u32>(U16()) << 16);
        return v;
    }

    std::string FourCC()
    {
        std::string id(reinterpret_cast<const char_type*>(mData + mPos), 4);
        mPos += 4;
        return id;
    }

    std::string Name(size_t size)
    {
        const char_type* p = reinterpret_cast<const char_type*>(mData + mPos);
        size_t len = 0;
        while (len < size && p[len])
        {
            len++;
        }
        mPos += size;
        return std::string(p, len);
    }

    const u8* mData;
    size_t mSize;
    size_t mPos = 0;
};

struct ChunkRef final
{
    const u8* mData = nullptr;
    u32 mSize = 0;
};

// The sub chunks of a LIST chunk's body, by id
static bool ReadSubChunks(const u8* pData, u32 size, std::vector<std::pair<std::string, ChunkRef>>& out)
{
    RiffReader r(pData, size);
    while (r.Has(8))
    {
        const std::string id = r.FourCC();
        const u32 chunkSize = r.U32();
        if (!r.Has(chunkSize))
        {
            return false;
        }
        out.push_back({id, {pData + r.mPos, chunkSize}});
        r.mPos += chunkSize + (chunkSize & 1);
    }
    return true;
}

static const ChunkRef* FindChunk(const std::vector<std::pair<std::string, ChunkRef>>& chunks, const char_type* pId)
{
    for (const auto& chunk : chunks)
    {
        if (chunk.first == pId)
        {
            return &chunk.second;
        }
    }
    return nullptr;
}

static std::string ChunkString(const ChunkRef& chunk)
{
    const char_type* p = reinterpret_cast<const char_type*>(chunk.mData);
    size_t len = 0;
    while (len < chunk.mSize && p[len])
    {
        len++;
    }
    return std::string(p, len);
}

struct BagRecord final
{
    u16 mGenIdx;
};

// Reads the bag + gen chunks of pdta into zones: zoneStart..zoneEnd bag indices
static bool ReadZones(const ChunkRef& bags, const ChunkRef& gens, u16 bagStart, u16 bagEnd, std::vector<SoundFont::Zone>& zones, std::string& error)
{
    const u32 bagCount = bags.mSize / 4;
    const u32 genCount = gens.mSize / 4;
    if (bagStart > bagEnd || bagEnd >= bagCount)
    {
        error = "bag index out of range";
        return false;
    }

    for (u32 bag = bagStart; bag < bagEnd; bag++)
    {
        RiffReader r(bags.mData, bags.mSize);
        r.mPos = bag * 4;
        const u16 genStart = r.U16();
        r.mPos = (bag + 1) * 4;
        const u16 genEnd = r.U16();
        if (genStart > genEnd || genEnd > genCount)
        {
            error = "generator index out of range";
            return false;
        }

        SoundFont::Zone zone;
        RiffReader g(gens.mData, gens.mSize);
        g.mPos = genStart * 4u;
        for (u32 i = genStart; i < genEnd; i++)
        {
            SoundFont::Generator gen;
            gen.mOper = g.U16();
            gen.mAmount = g.U16();
            zone.mGens.push_back(gen);
        }
        zones.push_back(std::move(zone));
    }
    return true;
}

bool SoundFont::Read(const std::vector<u8>& data, SoundFont& out, std::string& error)
{
    out = SoundFont();

    RiffReader r(data.data(), data.size());
    if (!r.Has(12) || r.FourCC() != "RIFF")
    {
        error = "not a RIFF file";
        return false;
    }
    const u32 riffSize = r.U32();
    if (r.FourCC() != "sfbk" || !r.Has(riffSize - 4))
    {
        error = "not an SF2 file";
        return false;
    }

    // The three LIST chunks
    std::vector<std::pair<std::string, ChunkRef>> info;
    std::vector<std::pair<std::string, ChunkRef>> sdta;
    std::vector<std::pair<std::string, ChunkRef>> pdta;
    while (r.Has(12))
    {
        const std::string id = r.FourCC();
        const u32 size = r.U32();
        if (!r.Has(size))
        {
            error = "chunk " + id + " is truncated";
            return false;
        }
        const u8* pBody = data.data() + r.mPos;
        if (id == "LIST" && size >= 4)
        {
            const std::string type(reinterpret_cast<const char_type*>(pBody), 4);
            auto* pList = type == "INFO" ? &info : type == "sdta" ? &sdta : type == "pdta" ? &pdta : nullptr;
            if (pList && !ReadSubChunks(pBody + 4, size - 4, *pList))
            {
                error = "LIST " + type + " is malformed";
                return false;
            }
        }
        r.mPos += size + (size & 1);
    }

    if (const ChunkRef* pName = FindChunk(info, "INAM"))
    {
        out.mName = ChunkString(*pName);
    }
    if (const ChunkRef* pComment = FindChunk(info, "ICMT"))
    {
        out.mComment = ChunkString(*pComment);
    }

    const ChunkRef* pSmpl = FindChunk(sdta, "smpl");
    const ChunkRef* pPhdr = FindChunk(pdta, "phdr");
    const ChunkRef* pPbag = FindChunk(pdta, "pbag");
    const ChunkRef* pPgen = FindChunk(pdta, "pgen");
    const ChunkRef* pInst = FindChunk(pdta, "inst");
    const ChunkRef* pIbag = FindChunk(pdta, "ibag");
    const ChunkRef* pIgen = FindChunk(pdta, "igen");
    const ChunkRef* pShdr = FindChunk(pdta, "shdr");
    if (!pPhdr || !pPbag || !pPgen || !pInst || !pIbag || !pIgen || !pShdr)
    {
        error = "missing a pdta chunk";
        return false;
    }
    if (pPhdr->mSize % 38 || pInst->mSize % 22 || pShdr->mSize % 46 || pPbag->mSize % 4 || pIbag->mSize % 4 || pPgen->mSize % 4 || pIgen->mSize % 4)
    {
        error = "a pdta chunk has a bad size";
        return false;
    }

    // Presets (the last record is the EOP terminal)
    const u32 presetCount = pPhdr->mSize / 38;
    for (u32 i = 0; i + 1 < presetCount; i++)
    {
        RiffReader p(pPhdr->mData, pPhdr->mSize);
        p.mPos = i * 38;
        Preset preset;
        preset.mName = p.Name(20);
        preset.mProgram = p.U16();
        preset.mBank = p.U16();
        const u16 bagStart = p.U16();
        p.mPos = (i + 1) * 38 + 24;
        const u16 bagEnd = p.U16();
        if (!ReadZones(*pPbag, *pPgen, bagStart, bagEnd, preset.mZones, error))
        {
            return false;
        }
        out.mPresets.push_back(std::move(preset));
    }

    const u32 instCount = pInst->mSize / 22;
    for (u32 i = 0; i + 1 < instCount; i++)
    {
        RiffReader p(pInst->mData, pInst->mSize);
        p.mPos = i * 22;
        Instrument inst;
        inst.mName = p.Name(20);
        const u16 bagStart = p.U16();
        p.mPos = (i + 1) * 22 + 20;
        const u16 bagEnd = p.U16();
        if (!ReadZones(*pIbag, *pIgen, bagStart, bagEnd, inst.mZones, error))
        {
            return false;
        }
        out.mInstruments.push_back(std::move(inst));
    }

    const u32 smplCount = pSmpl ? pSmpl->mSize / 2 : 0;
    const u32 sampleCount = pShdr->mSize / 46;
    for (u32 i = 0; i + 1 < sampleCount; i++)
    {
        RiffReader p(pShdr->mData, pShdr->mSize);
        p.mPos = i * 46;
        Sample sample;
        sample.mName = p.Name(20);
        const u32 start = p.U32();
        const u32 end = p.U32();
        const u32 loopStart = p.U32();
        const u32 loopEnd = p.U32();
        sample.mSampleRate = p.U32();
        sample.mOriginalPitch = p.U8();
        sample.mPitchCorrection = static_cast<s8>(p.U8());
        if (start > end || end > smplCount)
        {
            error = "sample " + sample.mName + " is out of range";
            return false;
        }
        sample.mPcm.resize(end - start);
        if (!sample.mPcm.empty())
        {
            memcpy(sample.mPcm.data(), pSmpl->mData + start * 2, sample.mPcm.size() * 2);
        }
        sample.mLoopStart = loopStart >= start ? loopStart - start : 0;
        sample.mLoopEnd = loopEnd >= start ? loopEnd - start : 0;
        out.mSamples.push_back(std::move(sample));
    }

    // Zones must point at instruments/samples that exist
    for (const Preset& preset : out.mPresets)
    {
        for (const Zone& zone : preset.mZones)
        {
            const Generator* pInstGen = zone.Find(eInstrument);
            if (pInstGen && pInstGen->mAmount >= out.mInstruments.size())
            {
                error = "preset " + preset.mName + " uses a missing instrument";
                return false;
            }
        }
    }
    for (const Instrument& inst : out.mInstruments)
    {
        for (const Zone& zone : inst.mZones)
        {
            const Generator* pSampleGen = zone.Find(eSampleId);
            if (pSampleGen && pSampleGen->mAmount >= out.mSamples.size())
            {
                error = "instrument " + inst.mName + " uses a missing sample";
                return false;
            }
        }
    }
    return true;
}
