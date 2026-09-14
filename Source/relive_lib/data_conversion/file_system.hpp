#pragma once

#include "../../AliveLibAE/stdafx.h"
#include "AutoFile.hpp"
#include <functional>

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

    // Lists the immediate subdirectories of dirPath (an actual directory path, not a wildcard
    // filter like EnumerateDirectory's fileName - "." and ".." are skipped). EnumerateDirectory
    // only ever reports files (see its implementation), so this is a separate function rather
    // than an overload of it. Takes std::function rather than reusing TEnumCallBack's raw
    // function-pointer type so callers can pass a capturing lambda (e.g. to accumulate results
    // into a local container) - EnumerateDirectory's existing callers all use non-capturing
    // lambdas today so that type was never a problem there, but this one's first caller needs to.
    void EnumerateSubDirectories(const char_type* dirPath, const std::function<void(const char_type*, u32)>& cb);
};
