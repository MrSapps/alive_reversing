#include <gtest/gtest.h>
#include "Automation.hpp"

static bool Parse(const std::string& script, std::vector<Automation::Command>& commands, std::string& error)
{
    return Automation::Parse(script, commands, error);
}

static std::string ParseError(const std::string& script)
{
    std::vector<Automation::Command> commands;
    std::string error;
    EXPECT_FALSE(Parse(script, commands, error)) << script;
    return error;
}

TEST(Automation, ParsesEveryCommand)
{
    const std::string script =
        "# Look at a camera\n"
        "timeout 60\n"
        "start rupture_farms 15 1\n"
        "start rupture_farms 15 1 1378 83\n"
        "wait_until in_game\n"
        "wait_until camera rupture_farms 15 2   # after walking right\n"
        "\n"
        "wait_frames 30\n"
        "screenshot out/shot.png\n"
        "dump_state out/state.json\n"
        "dump_objects out/objects.json\n"
        "switch 100 1\n"
        "expect_objects eSlamDoor 0\n"
        "quit\n";

    std::vector<Automation::Command> commands;
    std::string error;
    ASSERT_TRUE(Parse(script, commands, error)) << error;
    ASSERT_EQ(commands.size(), 12u);

    EXPECT_EQ(commands[0].mName, "timeout");
    EXPECT_EQ(commands[0].mLine, 2);

    EXPECT_EQ(commands[1].mName, "start");
    EXPECT_EQ(commands[1].mArgs, (std::vector<std::string>{"rupture_farms", "15", "1"}));
    EXPECT_EQ(commands[2].mArgs.size(), 5u);

    EXPECT_EQ(commands[4].mName, "wait_until");
    EXPECT_EQ(commands[4].mArgs, (std::vector<std::string>{"camera", "rupture_farms", "15", "2"}));
    EXPECT_EQ(commands[4].mLine, 6);

    EXPECT_EQ(commands[6].mArgs, (std::vector<std::string>{"out/shot.png"}));
    EXPECT_EQ(commands[11].mName, "quit");
    EXPECT_TRUE(commands[11].mArgs.empty());
}

TEST(Automation, EmptyScriptHasNoCommands)
{
    std::vector<Automation::Command> commands;
    std::string error;
    EXPECT_TRUE(Parse("\n  # nothing\n", commands, error));
    EXPECT_TRUE(commands.empty());
}

TEST(Automation, ErrorsSayWhichLine)
{
    EXPECT_EQ(ParseError("quit\nfly_away\n"), "line 2: unknown command 'fly_away'");
}

TEST(Automation, RejectsBadArguments)
{
    EXPECT_NE(ParseError("start atlantis 1 1").find("unknown level 'atlantis'"), std::string::npos);
    EXPECT_NE(ParseError("start none 1 1").find("unknown level"), std::string::npos);
    EXPECT_NE(ParseError("start rupture_farms 15").find("expected"), std::string::npos);
    EXPECT_NE(ParseError("start rupture_farms 15 1 100").find("expected"), std::string::npos);
    EXPECT_NE(ParseError("start rupture_farms fifteen 1").find("'fifteen'"), std::string::npos);
    EXPECT_NE(ParseError("wait_frames 0").find("'0'"), std::string::npos);
    EXPECT_NE(ParseError("wait_frames 3x").find("'3x'"), std::string::npos);
    EXPECT_NE(ParseError("wait_until lunch").find("expected"), std::string::npos);
    EXPECT_NE(ParseError("wait_until camera mines 1").find("expected"), std::string::npos);
    EXPECT_NE(ParseError("screenshot").find("expected"), std::string::npos);
    EXPECT_NE(ParseError("quit now").find("expected"), std::string::npos);
    EXPECT_NE(ParseError("timeout -5").find("'-5'"), std::string::npos);
}

TEST(Automation, SoundCommands)
{
    std::vector<Automation::Command> commands;
    std::string error;
    ASSERT_TRUE(Parse("music tension\nmusic 13\nsfx 5\nsfx 5 90\nseq_play 12\nseq_play 12 80 0\nseq_stop 12\nmusic tension other 0 1\nmusic 0 none\nkill_other\nsfx 5 20 90\n", commands, error)) << error;
    EXPECT_EQ(commands.size(), 11u);
    EXPECT_NE(ParseError("music 4 nobody").find("line 1"), std::string::npos);
    EXPECT_NE(ParseError("music 4 abe 2").find("line 1"), std::string::npos);
    EXPECT_NE(ParseError("kill_other now").find("expected"), std::string::npos);

    EXPECT_NE(ParseError("music bogus").find("line 1"), std::string::npos);
    EXPECT_NE(ParseError("music").find("expected"), std::string::npos);
    EXPECT_NE(ParseError("sfx 99999").find("line 1"), std::string::npos);
    EXPECT_NE(ParseError("sfx 5 200").find("line 1"), std::string::npos);
    EXPECT_NE(ParseError("seq_play").find("expected"), std::string::npos);
    EXPECT_NE(ParseError("seq_stop 1 2").find("expected"), std::string::npos);
}
