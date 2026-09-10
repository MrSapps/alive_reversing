#pragma once

#include "../../Tools/relive_api/ByteStream.hpp"


#include "../../relive_lib/Function.hpp"
#include "../../relive_lib/ResourceManagerWrapper.hpp"

#include "../../Tools/relive_api/relive_api_exceptions.hpp"
#include "file_system.hpp"
#include "../../Tools/relive_api/RoundUp.hpp"

#include <cstddef>
#include <cstring>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ReliveAPI {

// Represents the LVL structure on disk
struct LvlFileRecord final
{
    char_type field_0_file_name[12];
    s32 field_C_start_sector;
    s32 field_10_num_sectors;
    s32 field_14_file_size;
};

// Represents the LVL structure on disk
struct LvlHeader_Sub final
{
    s32 field_0_num_files;
    s32 field_4_header_size_in_sectors;
    s32 field_8_unknown2;
    s32 field_C_unknown3;
    LvlFileRecord field_10_file_recs[1]; // TODO: Strictly UB on >= 1 access
};

// Represents the LVL structure on disk
struct LvlHeader final
{
    // TODO: Up to field_C is really a ResourceManager::Header
    s32 field_0_first_file_offset;
    s32 field_4_ref_count;
    s32 field_8_magic;
    s32 field_C_id;
    LvlHeader_Sub field_10_sub;
};

[[nodiscard]] inline std::string ToString(const LvlFileRecord& rec)
{
    size_t i = 0;
    for (i = 0; i < ALIVE_COUNTOF(LvlFileRecord::field_0_file_name); ++i)
    {
        if (!rec.field_0_file_name[i])
        {
            break;
        }
    }
    return std::string(rec.field_0_file_name, i);
}

// Represents the res header structure on disk
struct ResourceHeader final
{
    u32 field_0_size;
    s16 field_4_ref_count;
    s16 field_6_flags;
    u32 mResourceType;
    u32 field_C_id;
};
ALIVE_ASSERT_SIZEOF_ALWAYS(ResourceHeader, 16);

class LvlFileChunk final
{
public:
    LvlFileChunk(u32 id, ResourceManagerWrapper::ResourceType resType, std::vector<u8>&& data)
        : mData(std::move(data))
    {
        mHeader.field_0_size = static_cast<u32>(mData.size());
        mHeader.field_C_id = id;
        mHeader.mResourceType = resType;
    }

    [[nodiscard]] u32 Id() const
    {
        return mHeader.field_C_id;
    }

    [[nodiscard]] u32 Size() const
    {
        return mHeader.field_0_size;
    }

    [[nodiscard]] const ResourceHeader& Header() const
    {
        return mHeader;
    }

    [[nodiscard]] const std::vector<u8>& Data() const
    {
        return mData;
    }

    [[nodiscard]] std::vector<u8>& Data()
    {
        return mData;
    }

private:
    ResourceHeader mHeader = {};
    std::vector<u8> mData;
};

class ChunkedLvlFile final
{
public:
    ChunkedLvlFile()
    {

    }

    explicit ChunkedLvlFile(const std::vector<u8>& data)
    {
        Read(data);
    }

    // TODO: return ptr?
    [[nodiscard]] std::optional<LvlFileChunk> ChunkById(u32 id) const
    {
        for (auto& chunk : mChunks)
        {
            if (chunk.Id() == id)
            {
                return {chunk};
            }
        }
        return {};
    }

    [[nodiscard]] std::optional<LvlFileChunk> ChunkByType(u32 type) const
    {
        for (auto& chunk : mChunks)
        {
            if (chunk.Header().mResourceType == type)
            {
                return {chunk};
            }
        }
        return {};
    }

    void RemoveChunksOfType(u32 type)
    {
        auto it = mChunks.begin();
        while (it != mChunks.end())
        {
            if (it->Header().mResourceType == type)
            {
                it = mChunks.erase(it);
            }
            else
            {
                it++;
            }
        }
    }

    void AddChunk(const LvlFileChunk& chunkToAdd)
    {
        LvlFileChunk copy = chunkToAdd;
        AddChunk(std::move(copy));
    }

    void AddChunk(LvlFileChunk&& chunkToAdd)
    {
        for (auto& chunk : mChunks)
        {
            if (chunk.Id() == chunkToAdd.Id())
            {
                chunk = std::move(chunkToAdd);
                return;
            }
        }

        bool hasEndChunkAtEnd = false;
        if (!mChunks.empty())
        {
            if (mChunks[mChunks.size() - 1].Header().mResourceType == ResourceManagerWrapper::Resource_End)
            {
                hasEndChunkAtEnd = true;
            }
        }

        if (hasEndChunkAtEnd)
        {
            mChunks.insert(mChunks.end() - 1, std::move(chunkToAdd));
        }
        else
        {
            mChunks.push_back(std::move(chunkToAdd));
        }
    }

    [[nodiscard]] std::vector<u8> Data() const
    {
        std::size_t neededSize = 0;
        for (auto& chunk : mChunks)
        {
            neededSize += chunk.Size();
        }
        neededSize += sizeof(ResourceHeader) * mChunks.size();

        ByteStream s;
        s.ReserveSize(neededSize);

        for (const auto& chunk : mChunks)
        {
            auto adjustedHeader = chunk.Header();
            if (adjustedHeader.field_0_size > 0)
            {
                adjustedHeader.field_0_size += sizeof(ResourceHeader);
            }

            s.Write(adjustedHeader.field_0_size);
            s.Write(adjustedHeader.field_4_ref_count);
            s.Write(adjustedHeader.field_6_flags);
            s.Write(adjustedHeader.mResourceType);
            s.Write(adjustedHeader.field_C_id);

            if (chunk.Size() > 0)
            {
                s.Write(chunk.Data());
            }
        }

        return std::move(s).GetBuffer();
    }

    u32 ChunkCount() const
    {
        return static_cast<u32>(mChunks.size());
    }

    const LvlFileChunk& ChunkAt(u32 idx) const
    {
        return mChunks[idx];
    }

private:
    void Read(const std::vector<u8>& data)
    {
        ByteStream s(data);

        do
        {
            ResourceHeader resHeader = {};
            s.Read(resHeader.field_0_size);
            s.Read(resHeader.field_4_ref_count);
            s.Read(resHeader.field_6_flags);
            s.Read(resHeader.mResourceType);
            s.Read(resHeader.field_C_id);

            std::vector<u8> tmpData(resHeader.field_0_size);
            if (resHeader.field_0_size > 0)
            {
                tmpData.resize(tmpData.size() - sizeof(ResourceHeader));
                s.Read(tmpData);
            }

            mChunks.emplace_back(resHeader.field_C_id, static_cast<ResourceManagerWrapper::ResourceType>(resHeader.mResourceType), std::move(tmpData));

            if (resHeader.mResourceType == ResourceManagerWrapper::ResourceType::Resource_End)
            {
                break;
            }
        }
        while (!s.AtReadEnd());
    }

    std::vector<LvlFileChunk> mChunks;
};

class LvlReader final
{
public:
    explicit LvlReader(FileSystem& fs, const char_type* lvlFile, bool bThrowOnFail = true)
    {
        mFileHandle = fs.OpenFile(lvlFile, "rb");
        if (!mFileHandle.GetFile())
        {
            if (bThrowOnFail)
            {
                throw ReliveAPI::IOReadException(lvlFile);
            }
            return;
        }

        if (!ReadTOC())
        {
            Close();
        }
    }

    [[nodiscard]] bool IsOpen() const
    {
        return mFileHandle.GetFile() != nullptr;
    }

    void Close()
    {
        mFileHandle.Close();
    }

    ~LvlReader()
    {
        Close();
    }

    [[nodiscard]] bool ReadFileInto(std::vector<u8>& target, const char_type* fileName)
    {
        if (!IsOpen())
        {
            return false;
        }

        for (const auto& rec : mFileRecords)
        {
            if (std::strncmp(rec.field_0_file_name, fileName, ALIVE_COUNTOF(LvlFileRecord::field_0_file_name)) == 0)
            {
                const auto fileOffset = rec.field_C_start_sector * 2048;

                if (::fseek(mFileHandle.GetFile(), fileOffset, SEEK_SET) != 0)
                {
                    return false;
                }

                target.resize(rec.field_14_file_size);

                if (!mFileHandle.Read(target))
                {
                    return false;
                }

                return true;
            }
        }

        return false;
    }

    [[nodiscard]] std::optional<std::vector<u8>> ReadFile(const char_type* fileName)
    {
        std::optional<std::vector<u8>> result;
        result.emplace();

        if (!ReadFileInto(*result, fileName))
        {
            return {};
        }

        return result;
    }

    [[nodiscard]] s32 FileCount() const
    {
        return mHeader.field_10_sub.field_0_num_files;
    }

    [[nodiscard]] std::string FileNameAt(s32 idx) const
    {
        return ToString(mFileRecords[idx]);
    }

    [[nodiscard]] s32 FileSizeAt(s32 idx) const
    {
        return mFileRecords[idx].field_14_file_size;
    }

protected:
    [[nodiscard]] bool ReadTOC()
    {
        if (!mFileHandle.Read(reinterpret_cast<u8*>(&mHeader), sizeof(LvlHeader) - sizeof(LvlFileRecord)))
        {
            return false;
        }

        // TODO: Should probably validate the header, but the real game does not give a toss
        // so probably is not that important.

        mFileRecords.resize(mHeader.field_10_sub.field_0_num_files);

        if (!mFileHandle.Read(mFileRecords))
        {
            return false;
        }

        return true;
    }

    AutoFILE mFileHandle;
    LvlHeader mHeader = {};
    std::vector<LvlFileRecord> mFileRecords;
};

} // namespace ReliveAPI
