// The data conversion's sound conversion: VAB (from a LVL, and sounds.dat for AE) -> SF2,
// SEQ -> MIDI file

#include <gtest/gtest.h>
#include "ScratchDir.hpp"
#include "SoundTestData.hpp"
#include "data_conversion/SoundConverter.hpp"
#include "data_conversion/LvlReader.hpp"
#include "Sound/SoundFont.hpp"
#include "Sound/VabSoundFont.hpp"
#include "Sound/PsxSpuApi.hpp"
#include "Sound/SeqMidi.hpp"
#include <cstring>
#include <filesystem>
#include <fstream>

using namespace SoundTestData;

static void Save(const std::filesystem::path& path, const std::vector<u8>& data)
{
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
}

static std::vector<u8> Load(const std::filesystem::path& path)
{
    std::ifstream f(path, std::ios::binary);
    return std::vector<u8>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

// A LVL archive holding files: a header sector with the file table, then each file from a
// sector boundary
static std::vector<u8> BuildLvl(const std::vector<std::pair<std::string, std::vector<u8>>>& files)
{
    constexpr s32 kSector = 2048;
    std::vector<u8> lvl(kSector);
    LvlHeader header = {};
    header.field_10_sub.field_0_num_files = static_cast<s32>(files.size());
    header.field_10_sub.field_4_header_size_in_sectors = 1;
    memcpy(lvl.data(), &header, sizeof(LvlHeader) - sizeof(LvlFileRecord));

    for (size_t i = 0; i < files.size(); i++)
    {
        LvlFileRecord rec = {};
        strncpy(rec.field_0_file_name, files[i].first.c_str(), sizeof(rec.field_0_file_name));
        rec.field_C_start_sector = static_cast<s32>(lvl.size() / kSector);
        rec.field_14_file_size = static_cast<s32>(files[i].second.size());
        rec.field_10_num_sectors = (rec.field_14_file_size + kSector - 1) / kSector;
        memcpy(lvl.data() + sizeof(LvlHeader) - sizeof(LvlFileRecord) + i * sizeof(LvlFileRecord), &rec, sizeof(rec));

        lvl.insert(lvl.end(), files[i].second.begin(), files[i].second.end());
        lvl.resize((lvl.size() + kSector - 1) / kSector * kSector);
    }
    return lvl;
}

static VabSoundFont::Vab ReadSoundBank(const std::filesystem::path& path)
{
    SoundFont sf2;
    std::string error;
    EXPECT_TRUE(SoundFont::Read(Load(path), sf2, error)) << error;
    VabSoundFont::Vab vab;
    EXPECT_TRUE(VabSoundFont::FromSoundFont(sf2, vab, error)) << error;
    return vab;
}

static void ConvertsSoundBank(bool isAo)
{
    ScratchDir dir;
    ScopedCurrentDir cwd(dir.Path());

    const Vab testVab = LevelVab();
    std::vector<u8> soundsDat;
    const std::vector<u8> vb = isAo ? BuildVbAo(testVab) : BuildVbAe(testVab, soundsDat);
    Save("TEST.LVL", BuildLvl({{"OTHER.BAN", {1, 2, 3}}, {"TEST.VH", BuildVh(testVab)}, {"TEST.VB", vb}}));
    if (!isAo)
    {
        Save("sounds.dat", soundsDat);
    }

    FileSystem fs;
    LvlReader lvl(fs, "TEST.LVL");
    ASSERT_TRUE(lvl.IsOpen());

    SoundConverter converter(fs, isAo);
    FileSystem::Path themeDir;
    themeDir.Append("sounds").Append("THEME");
    EXPECT_EQ(converter.ConvertSoundBank(lvl, themeDir, "TEST.VH", "TEST.VB"), "TEST.sf2");

    const std::filesystem::path sf2Path = std::filesystem::path("sounds") / "THEME" / "TEST.sf2";
    const VabSoundFont::Vab vab = ReadSoundBank(sf2Path);
    ASSERT_EQ(vab.mSamples.size(), testVab.mSamples.size());
    for (size_t i = 0; i < vab.mSamples.size(); i++)
    {
        EXPECT_EQ(vab.mSamples[i].mPcm, testVab.mSamples[i].mPcm) << i;
        EXPECT_EQ(vab.mSamples[i].mLoop, testVab.mSamples[i].mLoop) << i;
    }
    EXPECT_EQ(reinterpret_cast<const VabHeader*>(vab.mVh.data())->field_8_id, testVab.mId);

    // Every path of the theme asks again, it is only converted once
    std::filesystem::remove(sf2Path);
    EXPECT_EQ(converter.ConvertSoundBank(lvl, themeDir, "TEST.VH", "TEST.VB"), "TEST.sf2");
    EXPECT_FALSE(std::filesystem::exists(sf2Path));
}

TEST(SoundConverter, AeSoundBank)
{
    ConvertsSoundBank(false);
}

TEST(SoundConverter, AoSoundBank)
{
    ConvertsSoundBank(true);
}

// Without sounds.dat AE had no sound, the bank has the tones but no sample data
TEST(SoundConverter, AeWithoutSoundsDat)
{
    ScratchDir dir;
    ScopedCurrentDir cwd(dir.Path());

    const Vab testVab = LevelVab();
    std::vector<u8> soundsDat;
    Save("TEST.LVL", BuildLvl({{"TEST.VH", BuildVh(testVab)}, {"TEST.VB", BuildVbAe(testVab, soundsDat)}}));

    FileSystem fs;
    LvlReader lvl(fs, "TEST.LVL");
    SoundConverter converter(fs, false);
    FileSystem::Path themeDir("THEME");
    EXPECT_EQ(converter.ConvertSoundBank(lvl, themeDir, "TEST.VH", "TEST.VB"), "TEST.sf2");

    const VabSoundFont::Vab vab = ReadSoundBank(std::filesystem::path("THEME") / "TEST.sf2");
    ASSERT_EQ(vab.mSamples.size(), testVab.mSamples.size());
    for (size_t i = 0; i < vab.mSamples.size(); i++)
    {
        EXPECT_EQ(vab.mSamples[i].mPcm, std::vector<s16>(testVab.mSamples[i].mPcm.size())) << i;
    }
}

TEST(SoundConverter, Seq)
{
    ScratchDir dir;
    ScopedCurrentDir cwd(dir.Path());
    FileSystem fs;
    SoundConverter converter(fs, false);
    FileSystem::Path themeDir("THEME");

    SeqBuilder builder(96, 400000);
    const std::vector<u8> seq = builder.NoteOn(0, 60, 100).Wait(96).NoteOff(0, 60).End();
    EXPECT_EQ(converter.ConvertSeq(themeDir, "OPTAMB.SEQ", seq), "OPTAMB.mid");

    std::vector<u8> back;
    std::string error;
    ASSERT_TRUE(SeqMidi::SmfToSeq(Load(std::filesystem::path("THEME") / "OPTAMB.mid"), back, error)) << error;
    EXPECT_EQ(back, seq);

    // Not a SEQ: nothing written
    EXPECT_EQ(converter.ConvertSeq(themeDir, "BAD.SEQ", {1, 2, 3}), "");
    EXPECT_FALSE(std::filesystem::exists(std::filesystem::path("THEME") / "BAD.mid"));
}
