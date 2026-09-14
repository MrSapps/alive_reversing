// Unit tests for FileSystem (data_conversion/file_system.hpp) and relive::Mods, which reads
// mod directories through it. Plain gtest, no Qt - both are already Qt-free (relive_lib can't
// depend on Qt; that's the editor's own EditorMod's job - see EditorMod.hpp's own comment on the
// split), so these run directly against the real filesystem via <filesystem> for scratch temp
// directories, same idiom GridPlacementTests.cpp uses for pure-math coverage elsewhere in this
// repo, just backed by real disk I/O instead of in-memory values here.

#include "data_conversion/file_system.hpp"
#include "Mods.hpp"

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <algorithm>

namespace
{
    namespace fs = std::filesystem;

    // A fresh, uniquely-named scratch directory per test, cleaned up on destruction - avoids
    // tests stepping on each other's files if run in parallel, and avoids leaving junk behind
    // in the system temp dir across runs.
    class ScratchDir final
    {
    public:
        ScratchDir()
            : mPath(fs::temp_directory_path() / fs::path("relive_filesystem_tests_" + std::to_string(reinterpret_cast<uintptr_t>(this))))
        {
            fs::remove_all(mPath);
            fs::create_directories(mPath);
        }

        ~ScratchDir()
        {
            fs::remove_all(mPath);
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
        fs::path mPath;
    };
}

TEST(FileSystem, SaveThenLoadToStringRoundTrips)
{
    ScratchDir dir;
    FileSystem sut;

    const std::string filePath = dir.SubPath("hello.txt");
    const std::string contents = "hello, world";
    const std::vector<u8> data(contents.begin(), contents.end());

    ASSERT_TRUE(sut.Save(filePath.c_str(), data));
    EXPECT_EQ(sut.LoadToString(filePath.c_str()), contents);
}

TEST(FileSystem, SaveThenLoadToVecRoundTrips)
{
    ScratchDir dir;
    FileSystem sut;

    const std::string filePath = dir.SubPath("data.bin");
    const std::vector<u8> data = {1, 2, 3, 4, 250, 251, 252, 0};

    ASSERT_TRUE(sut.Save(filePath.c_str(), data));
    EXPECT_EQ(sut.LoadToVec(filePath.c_str()), data);
}

TEST(FileSystem, LoadToStringOfMissingFileReturnsEmpty)
{
    ScratchDir dir;
    FileSystem sut;
    EXPECT_EQ(sut.LoadToString(dir.SubPath("does_not_exist.txt").c_str()), "");
}

TEST(FileSystem, FileExistsReflectsActualState)
{
    ScratchDir dir;
    FileSystem sut;
    const std::string filePath = dir.SubPath("present.txt");

    EXPECT_FALSE(sut.FileExists(filePath.c_str()));
    ASSERT_TRUE(sut.Save(filePath.c_str(), std::vector<u8>{1}));
    EXPECT_TRUE(sut.FileExists(filePath.c_str()));
}

TEST(FileSystem, CreateDirectoryThenDirectoryExists)
{
    ScratchDir dir;
    FileSystem sut;
    const std::string subDir = dir.SubPath("nested/child");

    EXPECT_FALSE(sut.DirectoryExists(subDir.c_str()));

    FileSystem::Path path;
    path.Append(dir.Path()).Append("nested").Append("child");
    sut.CreateDirectory(path);

    EXPECT_TRUE(sut.DirectoryExists(subDir.c_str()));
}

TEST(FileSystem, EnumerateSubDirectoriesListsOnlyDirectoriesNotFiles)
{
    ScratchDir dir;
    FileSystem sut;

    fs::create_directories(fs::path(dir.Path()) / "SubA");
    fs::create_directories(fs::path(dir.Path()) / "SubB");
    {
        std::ofstream f(fs::path(dir.Path()) / "not_a_dir.txt");
        f << "x";
    }

    std::vector<std::string> found;
    sut.EnumerateSubDirectories(dir.Path().c_str(), [&](const char_type* name, u32)
    {
        found.push_back(name);
    });

    std::sort(found.begin(), found.end());
    EXPECT_EQ(found, (std::vector<std::string>{"SubA", "SubB"}));
}

TEST(FileSystem, EnumerateSubDirectoriesOfEmptyDirectoryFindsNothing)
{
    ScratchDir dir;
    FileSystem sut;

    std::vector<std::string> found;
    sut.EnumerateSubDirectories(dir.Path().c_str(), [&](const char_type* name, u32)
    {
        found.push_back(name);
    });

    EXPECT_TRUE(found.empty());
}

namespace
{
    void WriteModInfo(const ScratchDir& dir, const std::string& modDirName, const std::string& name, const std::string& author, const std::string& targetGame)
    {
        fs::create_directories(fs::path(dir.Path()) / modDirName);
        std::ofstream f(fs::path(dir.Path()) / modDirName / "modinfo.json");
        f << "{\"name\":\"" << name << "\",\"author\":\"" << author << "\",\"target_game\":\"" << targetGame << "\"}";
    }
}

TEST(Mods, EnumerateModsFindsValidModsAndSkipsDirectoriesWithoutModInfo)
{
    ScratchDir dir;
    FileSystem fileSystem;

    WriteModInfo(dir, "ModA", "Alpha Mod", "Alice", "AO");
    WriteModInfo(dir, "ModB", "Beta Mod", "Bob", "AE");
    fs::create_directories(fs::path(dir.Path()) / "NotAMod"); // no modinfo.json

    relive::Mods sut(fileSystem);
    sut.EnumerateMods(dir.Path());

    const relive::Mod* byDir = sut.FindByDirOrModName("ModA");
    ASSERT_NE(byDir, nullptr);
    EXPECT_EQ(byDir->mName, "Alpha Mod");
    EXPECT_EQ(byDir->mAuthor, "Alice");
    EXPECT_EQ(byDir->mTargetGame, "AO");

    const relive::Mod* byName = sut.FindByDirOrModName("Beta Mod");
    ASSERT_NE(byName, nullptr);
    EXPECT_EQ(byName->mDirectory, "ModB");
    EXPECT_EQ(byName->mTargetGame, "AE");

    EXPECT_EQ(sut.FindByDirOrModName("NotAMod"), nullptr);
}

TEST(Mods, FindByDirOrModNameOfUnknownNameReturnsNull)
{
    ScratchDir dir;
    FileSystem fileSystem;
    relive::Mods sut(fileSystem);
    sut.EnumerateMods(dir.Path());

    EXPECT_EQ(sut.FindByDirOrModName("DoesNotExist"), nullptr);
}

TEST(Mods, EnumerateModsClearsPreviousResultsOnRepeatedCalls)
{
    ScratchDir dir;
    FileSystem fileSystem;
    WriteModInfo(dir, "ModA", "Alpha Mod", "Alice", "AO");

    relive::Mods sut(fileSystem);
    sut.EnumerateMods(dir.Path());
    ASSERT_NE(sut.FindByDirOrModName("ModA"), nullptr);

    // Re-enumerating a now-empty directory should drop the previously found mod, not just add
    // to it - EnumerateMods is "here's what's there now", not "append whatever's new".
    ScratchDir emptyDir;
    sut.EnumerateMods(emptyDir.Path());
    EXPECT_EQ(sut.FindByDirOrModName("ModA"), nullptr);
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
