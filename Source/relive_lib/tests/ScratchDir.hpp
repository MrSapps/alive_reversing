#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

// A fresh, uniquely-named scratch directory per test, cleaned up on destruction - avoids
// tests stepping on each other's files if run in parallel, and avoids leaving junk behind
// in the system temp dir across runs.
class ScratchDir final
{
public:
    ScratchDir()
        : mPath(std::filesystem::temp_directory_path() / std::filesystem::path("relive_tests_" + std::to_string(reinterpret_cast<uintptr_t>(this))))
    {
        std::filesystem::remove_all(mPath);
        std::filesystem::create_directories(mPath);
    }

    ~ScratchDir()
    {
        std::filesystem::remove_all(mPath);
    }

    ScratchDir(const ScratchDir&) = delete;
    ScratchDir& operator=(const ScratchDir&) = delete;

    std::string Path() const
    {
        return mPath.string();
    }

    std::string SubPath(const std::string& relative) const
    {
        return (mPath / relative).string();
    }

private:
    std::filesystem::path mPath;
};

// Makes dir the current directory until destroyed - for code that uses paths relative to the
// game directory, such as ResourceManagerWrapper's settings ini.
class ScopedCurrentDir final
{
public:
    explicit ScopedCurrentDir(const std::string& dir)
        : mPrevious(std::filesystem::current_path())
    {
        std::filesystem::current_path(dir);
    }

    ~ScopedCurrentDir()
    {
        std::filesystem::current_path(mPrevious);
    }

    ScopedCurrentDir(const ScopedCurrentDir&) = delete;
    ScopedCurrentDir& operator=(const ScopedCurrentDir&) = delete;

private:
    std::filesystem::path mPrevious;
};
