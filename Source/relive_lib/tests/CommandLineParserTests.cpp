// Unit tests for CommandLineParser: splitting the command line into switches and values.

#include "CommandLineParser.hpp"

#include <gtest/gtest.h>

TEST(CommandLineParser, SkipsTheExecutableName)
{
    const char* argv[] = {"/home/AO/relive", "-ddfps"};
    const CommandLineParser clp(2, argv);

    ASSERT_EQ(clp.Args().size(), 1u);
    EXPECT_FALSE(clp.HasSwitch("/home/AO/relive"));
    EXPECT_TRUE(clp.HasSwitch("-ddfps"));
}

TEST(CommandLineParser, SwitchesMatchWholeArguments)
{
    const CommandLineParser clp({"-ddfps2", "-record=AO_run.dat", "-mod=AOmod"});

    // The old parser searched the joined command line for substrings, so all of these matched
    EXPECT_FALSE(clp.HasSwitch("-ddfps"));
    EXPECT_FALSE(clp.HasSwitch("AO"));
    EXPECT_FALSE(clp.HasSwitch("-record"));
}

TEST(CommandLineParser, GetValueReturnsTheTextAfterTheEquals)
{
    const CommandLineParser clp({"-mod=my mod", "-record=a=b.dat", "-empty="});

    EXPECT_EQ(clp.GetValue("-mod"), "my mod");
    EXPECT_EQ(clp.GetValue("-record"), "a=b.dat");
    EXPECT_EQ(clp.GetValue("-empty"), "");
    EXPECT_EQ(clp.GetValue("-play"), std::nullopt);
}

TEST(CommandLineParser, GetValueNeedsTheWholeName)
{
    const CommandLineParser clp({"-model=x"});
    EXPECT_EQ(clp.GetValue("-mod"), std::nullopt);
}

TEST(CommandLineParser, TheLastRepeatedArgumentWins)
{
    const CommandLineParser clp({"-renderer=sdl", "-renderer=gl", "-fullscreen", "-fullscreen=false"});

    EXPECT_EQ(clp.GetValue("-renderer"), "gl");
    EXPECT_EQ(clp.GetBool("-fullscreen"), false);
}

TEST(CommandLineParser, GetBoolAcceptsBareSwitchesAndTrueOrFalse)
{
    const CommandLineParser clp({"-a", "-b=true", "-c=false", "-d=yes"});

    EXPECT_EQ(clp.GetBool("-a"), true);
    EXPECT_EQ(clp.GetBool("-b"), true);
    EXPECT_EQ(clp.GetBool("-c"), false);
    EXPECT_EQ(clp.GetBool("-d"), std::nullopt);
    EXPECT_EQ(clp.GetBool("-missing"), std::nullopt);
}

TEST(CommandLineParser, HasIsTrueForSwitchesAndValues)
{
    const CommandLineParser clp({"-a", "-b=1"});

    EXPECT_TRUE(clp.Has("-a"));
    EXPECT_TRUE(clp.Has("-b"));
    EXPECT_FALSE(clp.Has("-c"));
}

TEST(CommandLineParser, EmptyCommandLine)
{
    const char* argv[] = {"relive"};
    const CommandLineParser clp(1, argv);

    EXPECT_TRUE(clp.Args().empty());
    EXPECT_FALSE(clp.HasSwitch("AO"));
}
