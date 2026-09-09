#pragma once

#include "../../relive_lib/Types.hpp"
#include "file_api.hpp"

// Implement ReliveAPI::IFile override that supports unicode from utf8 on windows
class File final : public ReliveAPI::IFile
{
public:
    File(const std::string& fileName, ReliveAPI::IFileIO::Mode mode);
    ~File() override;

    bool IsOpen() const override
    {
        return mFileHandle != nullptr;
    }

    bool Seek(std::size_t absPos) override
    {
        return ::fseek(mFileHandle, static_cast<long>(absPos), SEEK_SET) == 0;
    }

    bool Read(u8* buffer, std::size_t len) override
    {
        return ::fread(buffer, 1, len, mFileHandle) == len;
    }

    bool Write(const u8* buffer, std::size_t len) override
    {
        return ::fwrite(buffer, 1, len, mFileHandle) == len;
    }

    bool ReadInto(std::string& str) override;
    bool PadEOF(u32 multiple) override;

private:
    FILE* mFileHandle = nullptr;
};
