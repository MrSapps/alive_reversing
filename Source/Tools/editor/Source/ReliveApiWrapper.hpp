#pragma once

#include "relive_api.hpp"
#include "file_api.hpp"
#include <QString>

// Implement ReliveAPI::IFileIO override that supports unicode from utf8 on windows

class File final : public ReliveAPI::IFile
{
public:
    File(const std::string& fileName, ReliveAPI::IFileIO::Mode mode)
    {
#ifdef _WIN32
        // Convert the uft8 string to a utf16 string for old man windows
        QString utf8 = QString::fromStdString(fileName);
        std::vector<wchar_t> buffer;
        buffer.resize(utf8.length() + 1);
        utf8.toWCharArray(buffer.data());

        switch (mode)
        {
        case ReliveAPI::IFileIO::Mode::Write:
            mFileHandle = ::_wfopen(buffer.data(), L"w");
            break;

        case ReliveAPI::IFileIO::Mode::Read:
            mFileHandle = ::_wfopen(buffer.data(), L"r");
            break;

        case ReliveAPI::IFileIO::Mode::ReadBinary:
            mFileHandle = ::_wfopen(buffer.data(), L"rb");
            break;

        case ReliveAPI::IFileIO::Mode::WriteBinary:
            mFileHandle = ::_wfopen(buffer.data(), L"wb");
            break;
    }

#else
        // OSX, Linux and most other OSes support utf8 directly, the future is now, old man
        switch (mode)
        {
        case ReliveAPI::IFileIO::Mode::Write:
            mFileHandle = ::fopen(fileName.c_str(), "w");
            break;

        case ReliveAPI::IFileIO::Mode::Read:
            mFileHandle = ::fopen(fileName.c_str(), "r");
            break;

        case ReliveAPI::IFileIO::Mode::ReadBinary:
            mFileHandle = ::fopen(fileName.c_str(), "rb");
            break;

        case ReliveAPI::IFileIO::Mode::WriteBinary:
            mFileHandle = ::fopen(fileName.c_str(), "wb");
            break;
        }
#endif
    }

    ~File() override
    {
        if (IsOpen())
        {
            ::fclose(mFileHandle);
        }
    }

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

    bool ReadInto(std::string& str) override
    {
        const auto oldPos = ::ftell(mFileHandle);
        if (::fseek(mFileHandle, 0, SEEK_END) != 0)
        {
            return false;
        }

        const auto fileSize = ftell(mFileHandle);
        if (fileSize == -1)
        {
            return false;
        }

        str.resize(fileSize);
        if (!Seek(oldPos))
        {
            return false;
        }

        return Read(reinterpret_cast<u8*>(str.data()), str.size());
    }

    bool PadEOF(u32 multiple) override
    {
        const auto pos = ::ftell(mFileHandle);
        if (pos == -1)
        {
            return false;
        }

        if (pos + 1 != RoundUp(static_cast<int>(pos), static_cast<int>(multiple)))
        {
            ::fseek(mFileHandle, RoundUp(pos) - 1, SEEK_SET);
            u8 emptyByte = 0;
            ::fwrite(&emptyByte, sizeof(u8), 1, mFileHandle);
        }

        return true;
    }

private:
    FILE* mFileHandle = nullptr;
};

class EditorFileIO final : public ReliveAPI::IFileIO
{
public:
    std::unique_ptr<ReliveAPI::IFile> Open(const std::string& fileName, ReliveAPI::IFileIO::Mode mode) override
    {
        auto ret = std::make_unique<File>(fileName, mode);
        if (!ret->IsOpen())
        {
            return nullptr;
        }
        return ret;
    }
};

