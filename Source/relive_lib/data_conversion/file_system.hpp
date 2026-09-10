#pragma once

#include "../../AliveLibAE/stdafx.h"
#include "AutoFile.hpp"

// TODO: Lots of stuff missing, needs to do utf8 -> utf16 on windows
class FileSystem final
{
public:
    using TEnumCallBack = void(const char_type*, u32);

    class Path final
    {
    public:
        Path& Append(const std::string& directory);

        Path Parent() const;

        const std::string& GetPath() const;

    private:
        std::string mPath;
    };

    bool Save(const Path& path, const std::vector<u8>& data);
    bool Save(const char_type* path, const std::vector<u8>& data);

    std::string LoadToString(const Path& path);

    std::string LoadToString(const char* path);

    std::vector<u8> LoadToVec(const Path& path);
    std::vector<u8> LoadToVec(const char* path);

    bool LoadToVec(const Path& path, std::vector<u8>& buffer);
    bool LoadToVec(const char* path, std::vector<u8>& buffer);

    void CreateDirectory(const Path& path);

    bool FileExists(const char_type* fileName);

    // Strips a leading "./" or ".\\", and on non-Windows, falls back to a case-insensitive
    // scan of the containing directory if an exact-case open fails - game data is
    // Windows-authored and its casing doesn't always match what the code requests.
    // Returns an AutoFILE owning the resulting handle; check GetFile() for failure.
    AutoFILE OpenFile(const char_type* path, const char_type* mode, bool autoFlushFile = false);

    bool DirectoryExists(const char_type* pDirName);
    void EnumerateDirectory(const char_type* fileName, TEnumCallBack cb);
};
