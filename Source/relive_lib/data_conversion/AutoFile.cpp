#include "stdafx.h"
#include "AutoFile.hpp"
#include "../FatalError.hpp"

AutoFILE::AutoFILE(FILE* file, bool isWriter, bool autoFlushFile)
    : mFile(file)
    , mIsWriter(isWriter)
    , mAutoFlushFile(autoFlushFile)
{
}

AutoFILE::AutoFILE(AutoFILE&& rhs) noexcept
    : mFile(rhs.mFile)
    , mIsWriter(rhs.mIsWriter)
    , mAutoFlushFile(rhs.mAutoFlushFile)
{
    rhs.mFile = nullptr;
}

AutoFILE& AutoFILE::operator=(AutoFILE&& rhs) noexcept
{
    if (this != &rhs)
    {
        Close();
        mFile = rhs.mFile;
        mIsWriter = rhs.mIsWriter;
        mAutoFlushFile = rhs.mAutoFlushFile;
        rhs.mFile = nullptr;
    }
    return *this;
}

AutoFILE::~AutoFILE()
{
    Close();
}

FILE* AutoFILE::GetFile() const
{
    return mFile;
}

bool AutoFILE::Write(const u8* pBytes, u32 numBytes)
{
    const bool ret = ::fwrite(pBytes, 1, numBytes, mFile) == 1;
    Flush();
    return ret;
}

bool AutoFILE::Read(u8* pBytes, u32 numBytes)
{
    return ::fread(pBytes, 1, numBytes, mFile) == numBytes;
}

bool AutoFILE::ReadInto(std::string& target)
{
    const long fileSize = FileSize();
    const u32 oldPos = Pos();
    if (fileSize < 0 || static_cast<long>(oldPos) > fileSize)
    {
        return false;
    }

    target.resize(static_cast<std::size_t>(fileSize) - oldPos);
    return Read(reinterpret_cast<u8*>(target.data()), static_cast<u32>(target.size()));
}

u32 AutoFILE::PeekU32()
{
    const auto oldPos = ::ftell(mFile);

    const u32 data = ReadU32();

    if (::fseek(mFile, oldPos, SEEK_SET) != 0)
    {
        ALIVE_FATAL("Seek back failed");
    }

    return data;
}

u32 AutoFILE::ReadU32() const
{
    u32 value = 0;
    if (::fread(&value, sizeof(u32), 1, mFile) != 1)
    {
        ALIVE_FATAL("Read U32 failed");
    }
    return value;
}

u32 AutoFILE::Pos()
{
    return static_cast<u32>(::ftell(mFile));
}

void AutoFILE::Seek(u32 pos, AutoFILE::SeekMode mode)
{
    switch (mode)
    {
        case SeekMode::Current:
            if (::fseek(mFile, pos, SEEK_CUR) != 0)
            {
                ALIVE_FATAL("Seek back failed");
            }
            break;

        case SeekMode::Start:
            if (::fseek(mFile, pos, SEEK_SET) != 0)
            {
                ALIVE_FATAL("Seek failed");
            }
            break;

        default:
            ALIVE_FATAL("Unknown seek mode");
            break;
    }
}

long AutoFILE::FileSize()
{
    const long oldPos = ftell(mFile);
    fseek(mFile, 0, SEEK_END);
    const long fileSize = ftell(mFile);
    fseek(mFile, oldPos, SEEK_SET);
    return fileSize;
}

void AutoFILE::Close()
{
    if (mFile)
    {
        if (mIsWriter)
        {
            ::fflush(mFile);
        }
        ::fclose(mFile);
        mFile = nullptr;
    }
}

void AutoFILE::Flush()
{
    if (mAutoFlushFile)
    {
        if (::fflush(mFile) != 0)
        {
            ALIVE_FATAL("fflush failed");
        }
    }
}
