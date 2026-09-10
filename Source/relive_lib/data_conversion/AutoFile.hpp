#pragma once

#include "../../AliveLibAE/stdafx.h"

class FileSystem;

class [[nodiscard]] AutoFILE final
{
public:
    friend class FileSystem;

    enum class SeekMode
    {
        Current,
        Start,
    };

    AutoFILE(const AutoFILE&) = delete;
    AutoFILE& operator=(const AutoFILE&) const = delete;
    AutoFILE() = default;
    AutoFILE(AutoFILE&& rhs) noexcept;
    AutoFILE& operator=(AutoFILE&& rhs) noexcept;

    ~AutoFILE();

    FILE* GetFile() const;

    bool Write(const u8* pBytes, u32 numBytes);

    bool Read(u8* pBytes, u32 numBytes);

    // Reads all remaining bytes from the current position to EOF.
    bool ReadInto(std::string& target);

    template <typename TypeToWrite>
    bool Write(const TypeToWrite& value)
    {
        static_assert(std::is_pod<TypeToWrite>::value, "TypeToWrite must be pod");
        const bool ret = ::fwrite(&value, sizeof(TypeToWrite), 1, mFile) == 1;
        Flush();
        return ret;
    }

    template <typename TypeToWrite>
    bool Write(const std::vector<TypeToWrite>& value)
    {
        static_assert(std::is_pod<TypeToWrite>::value, "TypeToWrite must be pod");
        const bool ret = ::fwrite(value.data(), sizeof(TypeToWrite), value.size(), mFile) == value.size();
        Flush();
        return ret;
    }

    template <typename TypeToRead>
    bool Read(TypeToRead& value)
    {
        static_assert(std::is_pod<TypeToRead>::value, "TypeToRead must be pod");
        return ::fread(&value, sizeof(TypeToRead), 1, mFile) == 1;
    }

    template <typename TypeToRead>
    bool Read(std::vector<TypeToRead>& value)
    {
        static_assert(std::is_pod<TypeToRead>::value, "TypeToRead must be pod");
        return ::fread(value.data(), sizeof(TypeToRead), value.size(), mFile) == value.size();
    }

    u32 PeekU32();

    u32 ReadU32() const;

    u32 Pos();

    void Seek(u32 pos, SeekMode mode);

    long FileSize();

    void Close();

private:
    AutoFILE(FILE* file, bool isWriter, bool autoFlushFile);

    void Flush();

    FILE* mFile = nullptr;
    bool mIsWriter = false;
    bool mAutoFlushFile = false;
};
