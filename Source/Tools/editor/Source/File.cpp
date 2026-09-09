#include "File.hpp"
#include <QString>
#include <vector>

File::File(const std::string& fileName, ReliveAPI::IFileIO::Mode mode)
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

File::~File()
{
    if (IsOpen())
    {
        ::fclose(mFileHandle);
    }
}

bool File::ReadInto(std::string& str)
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

bool File::PadEOF(u32 multiple)
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
