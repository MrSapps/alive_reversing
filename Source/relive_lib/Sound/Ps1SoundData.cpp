#include "../stdafx.h"
#include "Ps1SoundData.hpp"
#include "../data_conversion/LvlReader.hpp"
#include "../data_conversion/file_system.hpp"
#include "../logger.hpp"
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>

Ps1SoundData::Ps1SoundData(FileSystem& fs)
    : mFs(fs)
{
}

std::string Ps1SoundData::Upper(const std::string& name)
{
    std::string out = name;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return out;
}

// A real LVL has "Indx" at offset 8 and a sane file count (some files named .LVL on a disc aren't, and
// LvlReader trusts the count)
static bool LooksLikeLvl(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    u8 header[0x20] = {};
    if (!in.read(reinterpret_cast<char*>(header), sizeof(header)))
    {
        return false;
    }
    s32 fileCount = 0;
    std::memcpy(&fileCount, &header[0x10], sizeof(fileCount));
    return std::memcmp(&header[8], "Indx", 4) == 0 && fileCount > 0 && fileCount < 4096;
}

static bool IsSoundFile(const std::string& upperName)
{
    const std::size_t dot = upperName.rfind('.');
    if (dot == std::string::npos)
    {
        return false;
    }
    const std::string ext = upperName.substr(dot);
    return ext == ".VH" || ext == ".VB" || ext == ".BSQ";
}

bool Ps1SoundData::Open(const std::string& dir)
{
    std::error_code ec;
    std::filesystem::recursive_directory_iterator it(dir, std::filesystem::directory_options::follow_directory_symlink, ec);
    if (ec)
    {
        LOG_ERROR("-ps1_data: can't read %s", dir.c_str());
        return false;
    }

    for (const std::filesystem::directory_entry& entry : it)
    {
        if (!entry.is_regular_file(ec) || Upper(entry.path().extension().string()) != ".LVL")
        {
            continue;
        }

        const std::string path = entry.path().string();
        if (!LooksLikeLvl(path))
        {
            continue;
        }
        LvlReader lvl(mFs, path.c_str());
        if (!lvl.IsOpen())
        {
            continue;
        }

        for (s32 i = 0; i < lvl.FileCount(); i++)
        {
            const std::string name = lvl.FileNameAt(i);
            if (IsSoundFile(Upper(name)))
            {
                mFiles.emplace(Upper(name), Entry{path, Upper(entry.path().filename().string()), name});
            }
        }
    }

    LOG_INFO("-ps1_data: %zu sound files in %s", mFiles.size(), dir.c_str());
    return !mFiles.empty();
}

bool Ps1SoundData::Has(const std::string& fileName) const
{
    return mFiles.count(Upper(fileName)) != 0;
}

std::vector<u8> Ps1SoundData::ReadFile(const std::string& fileName) const
{
    const auto it = mFiles.find(Upper(fileName));
    if (it == mFiles.end())
    {
        return {};
    }

    LvlReader lvl(mFs, it->second.mLvlPath.c_str());
    if (!lvl.IsOpen())
    {
        return {};
    }
    auto bytes = lvl.ReadFile(it->second.mFileName.c_str());
    return bytes ? std::move(*bytes) : std::vector<u8>{};
}

std::vector<u8> Ps1SoundData::ReadSeq(const std::string& seqName, const std::string& vhFile) const
{
    const u32 id = static_cast<u32>(ResourceManagerWrapper::SEQ_HashName(seqName.c_str()));

    // The LVL that holds the VH goes first, then every other
    std::string firstLvl;
    const auto vh = mFiles.find(Upper(vhFile));
    if (vh != mFiles.end())
    {
        firstLvl = vh->second.mLvlPath;
    }

    std::vector<const Entry*> bsqs;
    for (const auto& kv : mFiles)
    {
        if (kv.first.size() > 4 && kv.first.compare(kv.first.size() - 4, 4, ".BSQ") == 0)
        {
            if (kv.second.mLvlPath == firstLvl)
            {
                bsqs.insert(bsqs.begin(), &kv.second);
            }
            else
            {
                bsqs.push_back(&kv.second);
            }
        }
    }

    for (const Entry* pEntry : bsqs)
    {
        LvlReader lvl(mFs, pEntry->mLvlPath.c_str());
        if (!lvl.IsOpen())
        {
            continue;
        }
        const auto bsqData = lvl.ReadFile(pEntry->mFileName.c_str());
        if (!bsqData)
        {
            continue;
        }

        const ChunkedLvlFile bsq(*bsqData);
        for (u32 i = 0; i < bsq.ChunkCount(); i++)
        {
            const LvlFileChunk& chunk = bsq.ChunkAt(i);
            if (chunk.Header().mResourceType == ResourceManagerWrapper::Resource_Seq && chunk.Id() == id)
            {
                return chunk.Data();
            }
        }
    }
    return {};
}

std::vector<Ps1SoundData::Sample> Ps1SoundData::DecodeVab(const std::vector<u8>& vh, const std::vector<u8>& vb)
{
    std::vector<Sample> out;

    // VabHeader: 32 bytes, 128 program attributes of 16, a 16 tone table of 32 byte VagAtrs for each
    // program that has tones (the header's ps), then the 256 VAG sizes
    if (vh.size() < 32 + 128 * 16)
    {
        return out;
    }
    u16 programs = 0;
    u16 vags = 0;
    std::memcpy(&programs, &vh[0x12], 2);
    std::memcpy(&vags, &vh[0x16], 2);
    const std::size_t tableOffset = 32 + 128 * 16 + static_cast<std::size_t>(programs) * 16 * 32;
    if (tableOffset + 256 * 2 > vh.size())
    {
        return out;
    }

    static const s32 kPos[5] = {0, 60, 115, 98, 122};
    static const s32 kNeg[5] = {0, 0, -52, -55, -60};

    std::size_t offset = 0;
    for (u32 n = 1; n <= vags && n < 256; n++)
    {
        u16 sizeUnits = 0;
        std::memcpy(&sizeUnits, &vh[tableOffset + n * 2], 2);
        // VAG n starts after VAGs 0 to n - 1
        u16 prevUnits = 0;
        std::memcpy(&prevUnits, &vh[tableOffset + (n - 1) * 2], 2);
        offset += static_cast<std::size_t>(prevUnits) * 8;

        Sample sample;
        const std::size_t size = static_cast<std::size_t>(sizeUnits) * 8;
        if (size >= 16 && offset + size <= vb.size())
        {
            s32 h1 = 0;
            s32 h2 = 0;
            u32 loopStart = 0;
            bool loop = false;

            // Decodes the block at blockOffset, continuing the filter history
            auto decodeBlock = [&](const u8* p) {
                u32 shift = p[0] & 0xF;
                if (shift > 12)
                {
                    shift = 9;
                }
                const u32 filter = std::min<u32>((p[0] >> 4) & 0xF, 4);
                for (u32 i = 0; i < 28; i++)
                {
                    const u32 nib = (p[2 + i / 2] >> ((i & 1) * 4)) & 0xF;
                    s32 v = static_cast<s16>(nib << 12) >> shift;
                    v += (h1 * kPos[filter] + h2 * kNeg[filter]) >> 6;
                    v = std::clamp(v, -32768, 32767);
                    h2 = h1;
                    h1 = v;
                    sample.mPcm.push_back(static_cast<s16>(v));
                }
            };

            const std::size_t blocks = size / 16;
            for (std::size_t block = 0; block < blocks; block++)
            {
                const u8* p = &vb[offset + block * 16];
                const u8 flags = p[1];
                if (flags & 4)
                {
                    loopStart = static_cast<u32>(block * 28);
                }
                decodeBlock(p);
                if (flags & 1)
                {
                    loop = (flags & 2) != 0;
                    break;
                }
            }

            if (loop)
            {
                // The SPU decodes each pass of a loop with the filter history the last left, so the
                // second pass differs a little from the first: decode it again and loop that
                const u32 firstEnd = static_cast<u32>(sample.mPcm.size());
                const u32 loopBlocks = (firstEnd - loopStart) / 28;
                for (u32 block = 0; block < loopBlocks; block++)
                {
                    decodeBlock(&vb[offset + (loopStart / 28 + block) * 16]);
                }
                loopStart = firstEnd;
            }
            sample.mLoop = loop;
            sample.mLoopStart = loopStart;
        }
        out.push_back(std::move(sample));
    }
    return out;
}
