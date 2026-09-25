// Unit tests for IniFile, the parser behind relive.ini.

#include "IniFile.hpp"
#include "data_conversion/file_system.hpp"
#include "ScratchDir.hpp"

#include <gtest/gtest.h>

// The input sections as the game writes them (the format abe2.ini used, which old recordings hold).
static const char* kInputSettingsIni =
    "[Control]\n"
    "controller = Keyboard\n"
    "\n"
    "[Keyboard]\n"
    "run = Left Shift\n"
    "sneak = Left Alt\n"
    "jump = Space\n"
    "\n"
    "[Gamepad]\n"
    "buttons = 12\n"
    "run = Button 6\n"
    "\n"
    "[Alive]\n"
        "latency_hack = true\n";

TEST(IniFile, ParseKeepsSectionsAndEntriesInFileOrder)
{
    const IniFile ini = IniFile::Parse(kInputSettingsIni);

    ASSERT_EQ(ini.Sections().size(), 4u);
    EXPECT_EQ(ini.Sections()[0].mName, "Control");
    EXPECT_EQ(ini.Sections()[1].mName, "Keyboard");
    EXPECT_EQ(ini.Sections()[2].mName, "Gamepad");
    EXPECT_EQ(ini.Sections()[3].mName, "Alive");

    const auto& keyboard = ini.Sections()[1].mEntries;
    ASSERT_EQ(keyboard.size(), 3u);
    EXPECT_EQ(keyboard[0].mKey, "run");
    EXPECT_EQ(keyboard[0].mValue, "Left Shift");
    EXPECT_EQ(keyboard[2].mKey, "jump");
    EXPECT_EQ(keyboard[2].mValue, "Space");
}

TEST(IniFile, ParseTrimsWhitespaceAndWindowsLineEndings)
{
    const IniFile ini = IniFile::Parse("  [Keyboard] \r\n  run   =   Left Shift  \r\n");
    EXPECT_EQ(ini.GetValue("Keyboard", "run"), "Left Shift");
}

TEST(IniFile, ParseSkipsCommentsBlankLinesAndLinesWithoutEquals)
{
    const IniFile ini = IniFile::Parse("[A]\n; comment = x\n# also = comment\n\nnot a value\nkey = value\n");
    ASSERT_EQ(ini.Sections().size(), 1u);
    ASSERT_EQ(ini.Sections()[0].mEntries.size(), 1u);
    EXPECT_EQ(ini.GetValue("A", "key"), "value");
}

TEST(IniFile, ValueIsEverythingAfterTheFirstEquals)
{
    const IniFile ini = IniFile::Parse("[A]\nkey = a = b\n");
    EXPECT_EQ(ini.GetValue("A", "key"), "a = b");
}

TEST(IniFile, KeysBeforeAnySectionGoInAnUnnamedSection)
{
    const IniFile ini = IniFile::Parse("key = value\n[A]\nother = 1\n");
    EXPECT_EQ(ini.GetValue("", "key"), "value");
    EXPECT_EQ(ini.GetValue("A", "other"), "1");
}

TEST(IniFile, RepeatedSectionsAreKeptSeparatelyInOrder)
{
    const IniFile ini = IniFile::Parse("[Keyboard]\nrun = A\n[Keyboard]\nrun = B\n");
    ASSERT_EQ(ini.Sections().size(), 2u);
    EXPECT_EQ(ini.Sections()[0].mEntries[0].mValue, "A");
    EXPECT_EQ(ini.Sections()[1].mEntries[0].mValue, "B");
    // Lookups use the first one
    EXPECT_EQ(ini.GetValue("Keyboard", "run"), "A");
}

TEST(IniFile, GetValueIsNulloptForMissingSectionOrKey)
{
    const IniFile ini = IniFile::Parse(kInputSettingsIni);
    EXPECT_EQ(ini.GetValue("Display", "fullscreen"), std::nullopt);
    EXPECT_EQ(ini.GetValue("Keyboard", "missing"), std::nullopt);
}

TEST(IniFile, GetBoolOnlyAcceptsTrueAndFalse)
{
    const IniFile ini = IniFile::Parse("[A]\nyes = true\nno = false\nbad = 1\n");
    EXPECT_EQ(ini.GetBool("A", "yes"), true);
    EXPECT_EQ(ini.GetBool("A", "no"), false);
    EXPECT_EQ(ini.GetBool("A", "bad"), std::nullopt);
    EXPECT_EQ(ini.GetBool("A", "missing"), std::nullopt);
}

TEST(IniFile, SetValueReplacesAnExistingKeyInPlace)
{
    IniFile ini = IniFile::Parse("[A]\nfirst = 1\nsecond = 2\n");
    ini.SetValue("A", "first", "changed");

    const auto& entries = ini.Sections()[0].mEntries;
    ASSERT_EQ(entries.size(), 2u);
    EXPECT_EQ(entries[0].mKey, "first");
    EXPECT_EQ(entries[0].mValue, "changed");
}

TEST(IniFile, SetValueAddsMissingKeysAndSections)
{
    IniFile ini;
    ini.SetValue("A", "key", "value");
    ini.SetBool("B", "flag", true);

    EXPECT_EQ(ini.GetValue("A", "key"), "value");
    EXPECT_EQ(ini.GetBool("B", "flag"), true);
    ASSERT_EQ(ini.Sections().size(), 2u);
    EXPECT_EQ(ini.Sections()[1].mName, "B");
}

TEST(IniFile, ReplaceSectionKeepsItsPositionAndTheOtherSections)
{
    IniFile ini = IniFile::Parse(kInputSettingsIni);
    ini.ReplaceSection({"Keyboard", {{"run", "R"}}});

    ASSERT_EQ(ini.Sections().size(), 4u);
    EXPECT_EQ(ini.Sections()[1].mName, "Keyboard");
    ASSERT_EQ(ini.Sections()[1].mEntries.size(), 1u);
    EXPECT_EQ(ini.GetValue("Keyboard", "run"), "R");
    EXPECT_EQ(ini.GetValue("Keyboard", "jump"), std::nullopt);
    EXPECT_EQ(ini.GetValue("Gamepad", "buttons"), "12");
}

TEST(IniFile, ReplaceSectionAddsAMissingSectionAtTheEnd)
{
    IniFile ini = IniFile::Parse(kInputSettingsIni);
    ini.ReplaceSection({"Display", {{"fullscreen", "true"}}});

    ASSERT_EQ(ini.Sections().size(), 5u);
    EXPECT_EQ(ini.Sections()[4].mName, "Display");
}

TEST(IniFile, ToStringWritesKeyEqualsValueLinesAndRoundTrips)
{
    const IniFile ini = IniFile::Parse(kInputSettingsIni);
    const std::string text = ini.ToString();

    EXPECT_NE(text.find("[Keyboard]\nrun = Left Shift\n"), std::string::npos);
    EXPECT_EQ(IniFile::Parse(text).ToString(), text);
}

TEST(IniFile, ParseOfEmptyTextHasNoSections)
{
    EXPECT_TRUE(IniFile::Parse("").Sections().empty());
    EXPECT_EQ(IniFile{}.ToString(), "");
}

TEST(IniFile, LoadOfAMissingFileIsEmpty)
{
    ScratchDir dir;
    FileSystem fs;
    EXPECT_TRUE(IniFile::Load(fs, dir.SubPath("missing.ini")).Sections().empty());
}

TEST(IniFile, SaveThenLoadRoundTrips)
{
    ScratchDir dir;
    FileSystem fs;
    const IniFile ini = IniFile::Parse(kInputSettingsIni);

    ASSERT_TRUE(ini.Save(fs, dir.SubPath("settings.ini")));
    EXPECT_EQ(IniFile::Load(fs, dir.SubPath("settings.ini")).ToString(), ini.ToString());
}
