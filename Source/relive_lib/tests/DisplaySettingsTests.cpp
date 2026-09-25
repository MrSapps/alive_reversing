// Unit tests for DisplaySettings: the [Display] section of relive.ini.

#include "DisplaySettings.hpp"
#include "IniFile.hpp"
#include "ResourceManagerWrapper.hpp"
#include "data_conversion/file_system.hpp"
#include "ScratchDir.hpp"

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>

// The input sections the game writes to relive.ini (as they were in abe2.ini).
static const char* kInputSettingsIni =
    "[Control]\n"
    "controller = Keyboard\n"
    "\n"
    "[Keyboard]\n"
    "run = Left Shift\n"
    "jump = Space\n"
    "\n"
    "[Alive]\n"
    "latency_hack = true\n";

static void WriteFile(const std::string& path, const std::string& text)
{
    std::ofstream file(path, std::ios::binary);
    file << text;
}

static std::string ReadFile(const std::string& path)
{
    FileSystem fs;
    return fs.LoadToString(path.c_str());
}

static DisplaySettings NonDefaultSettings()
{
    DisplaySettings settings;
    settings.mRenderer = IRenderer::Renderers::OpenGL;
    settings.mFullscreen = true;
    settings.mKeepAspectRatio = false;
    settings.mFilterScreen = false;
    settings.mUseOriginalResolution = false;
    return settings;
}

// DisplaySettings: reading and writing [Display]

TEST(DisplaySettings, DefaultsWhenThereIsNoDisplaySection)
{
    EXPECT_EQ(DisplaySettings::FromIni(IniFile::Parse(kInputSettingsIni)), DisplaySettings{});

    const DisplaySettings defaults;
    EXPECT_EQ(defaults.mRenderer, IRenderer::Renderers::Sdl3);
    EXPECT_FALSE(defaults.mFullscreen);
    EXPECT_TRUE(defaults.mKeepAspectRatio);
    EXPECT_TRUE(defaults.mFilterScreen);
    EXPECT_TRUE(defaults.mUseOriginalResolution);
}

TEST(DisplaySettings, WriteToThenFromIniRoundTrips)
{
    IniFile ini;
    NonDefaultSettings().WriteTo(ini);
    EXPECT_EQ(DisplaySettings::FromIni(ini), NonDefaultSettings());
}

TEST(DisplaySettings, FromIniReadsTheDocumentedKeys)
{
    const IniFile ini = IniFile::Parse(
        "[Display]\n"
        "renderer = opengl\n"
        "fullscreen = true\n"
        "keep_aspect_ratio = false\n"
        "filter_screen = false\n"
        "use_original_resolution = false\n");
    EXPECT_EQ(DisplaySettings::FromIni(ini), NonDefaultSettings());
}

TEST(DisplaySettings, InvalidValuesKeepTheirDefaults)
{
    const IniFile ini = IniFile::Parse(
        "[Display]\n"
        "renderer = vulkan\n"
        "fullscreen = yes\n"
        "keep_aspect_ratio = 0\n");
    EXPECT_EQ(DisplaySettings::FromIni(ini), DisplaySettings{});
}

TEST(DisplaySettings, RendererNamesMatchTheCommandLine)
{
    for (const char* name : {"sdl", "SDL3", "sdl3"})
    {
        EXPECT_EQ(DisplaySettings::RendererFromString(name), IRenderer::Renderers::Sdl3) << name;
    }
    for (const char* name : {"gl", "GL3", "opengl", "OpenGL3"})
    {
        EXPECT_EQ(DisplaySettings::RendererFromString(name), IRenderer::Renderers::OpenGL) << name;
    }
    EXPECT_EQ(DisplaySettings::RendererFromString("vulkan"), std::nullopt);

    for (IRenderer::Renderers type : {IRenderer::Renderers::Sdl3, IRenderer::Renderers::OpenGL})
    {
        EXPECT_EQ(DisplaySettings::RendererFromString(DisplaySettings::RendererToString(type)), type);
    }
}

TEST(DisplaySettings, WriteToReplacesAnExistingDisplaySection)
{
    IniFile ini = IniFile::Parse("[Display]\nfullscreen = true\nold_key = 1\n");
    DisplaySettings{}.WriteTo(ini);

    EXPECT_EQ(ini.GetBool("Display", "fullscreen"), false);
    EXPECT_EQ(ini.GetValue("Display", "old_key"), std::nullopt);
    EXPECT_EQ(ini.Sections().size(), 1u);
}

// Persistence through relive.ini (ResourceManagerWrapper's settings ini)

// A ResourceManagerWrapper whose settings ini is in a scratch dir.
class SettingsIniFixture : public ::testing::Test
{
protected:
    ScratchDir mDir;
    ScopedCurrentDir mCurrentDir{mDir.Path()};
    FileSystem mFs;
    ResourceManagerWrapper mResMan{mFs, ""};

    std::string IniPath() const
    {
        return mDir.SubPath("relive.ini");
    }

    // What the hotkeys and -renderer= do
    void SaveDisplaySettings(const DisplaySettings& settings)
    {
        IniFile ini = mResMan.LoadSettingsIni();
        settings.WriteTo(ini);
        mResMan.SaveSettingsIni(ini);
    }
};

TEST_F(SettingsIniFixture, NoIniFileGivesDefaultDisplaySettings)
{
    EXPECT_EQ(mResMan.LoadSettingsIniText(), "");
    EXPECT_EQ(DisplaySettings::FromIni(mResMan.LoadSettingsIni()), DisplaySettings{});
}

TEST_F(SettingsIniFixture, LoadSettingsIniTextIsTheFileUnchanged)
{
    // Recordings store this text as-is, so it mustn't be reformatted
    const std::string text = "[Keyboard]\r\nrun   =  Left Shift\r\n; comment\r\n";
    WriteFile(IniPath(), text);
    EXPECT_EQ(mResMan.LoadSettingsIniText(), text);
}

TEST_F(SettingsIniFixture, SavedDisplaySettingsLoadOnTheNextRun)
{
    // First run: switch to fullscreen and stretch to the window (F12 and F11)
    DisplaySettings settings = DisplaySettings::FromIni(mResMan.LoadSettingsIni());
    settings.mFullscreen = true;
    settings.mKeepAspectRatio = false;
    SaveDisplaySettings(settings);

    // Next run
    const DisplaySettings loaded = DisplaySettings::FromIni(mResMan.LoadSettingsIni());
    EXPECT_TRUE(loaded.mFullscreen);
    EXPECT_FALSE(loaded.mKeepAspectRatio);
    EXPECT_TRUE(loaded.mFilterScreen);
}

TEST_F(SettingsIniFixture, SavingDisplaySettingsKeepsTheInputSettings)
{
    WriteFile(IniPath(), kInputSettingsIni);
    SaveDisplaySettings(NonDefaultSettings());

    const IniFile saved = mResMan.LoadSettingsIni();
    EXPECT_EQ(saved.GetValue("Control", "controller"), "Keyboard");
    EXPECT_EQ(saved.GetValue("Keyboard", "run"), "Left Shift");
    EXPECT_EQ(saved.GetValue("Keyboard", "jump"), "Space");
    EXPECT_EQ(DisplaySettings::FromIni(saved), NonDefaultSettings());

    // The input sections come first, in their original order
    ASSERT_EQ(saved.Sections().size(), 4u);
    EXPECT_EQ(saved.Sections()[0].mName, "Control");
    EXPECT_EQ(saved.Sections()[1].mName, "Keyboard");
    EXPECT_EQ(saved.Sections()[2].mName, "Alive");
    EXPECT_EQ(saved.Sections()[3].mName, DisplaySettings::kSection);
}

TEST_F(SettingsIniFixture, SavingInputSettingsKeepsTheDisplaySettings)
{
    SaveDisplaySettings(NonDefaultSettings());

    // What Input_SaveSettingsIni_Common does: replace only the input sections
    IniFile ini = mResMan.LoadSettingsIni();
    ini.ReplaceSection({"Keyboard", {{"run", "R"}}});
    ASSERT_TRUE(mResMan.SaveSettingsIni(ini));

    EXPECT_EQ(DisplaySettings::FromIni(mResMan.LoadSettingsIni()), NonDefaultSettings());
    EXPECT_EQ(mResMan.LoadSettingsIni().GetValue("Keyboard", "run"), "R");
}
