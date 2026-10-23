// Unit tests for PaletteFit: the change of colours between two palettes

#include "data_conversion/PaletteFit.hpp"
#include <gtest/gtest.h>
#include <cmath>

static void Apply(const f32 (&m)[3][4], const RGBA32& in, f32 (&out)[3])
{
    const f32 v[4] = {in.r / 255.0f, in.g / 255.0f, in.b / 255.0f, 1.0f};
    for (s32 ch = 0; ch < 3; ch++)
    {
        out[ch] = m[ch][0] * v[0] + m[ch][1] * v[1] + m[ch][2] * v[2] + m[ch][3] * v[3];
    }
}

static std::vector<RGBA32> Ramp(u32 count)
{
    std::vector<RGBA32> colours(count);
    for (u32 i = 1; i < count; i++)
    {
        colours[i] = {static_cast<u8>(i * 255 / count), static_cast<u8>((i * 97) % 200 + 20), static_cast<u8>(255 - i * 200 / count), 255};
    }
    return colours;
}

TEST(PaletteFit, TheSamePaletteIsNoChange)
{
    const std::vector<RGBA32> pal = Ramp(64);
    f32 matrix[3][4];
    const f32 error = PaletteFit::Fit(pal.data(), pal.data(), 64, matrix);
    EXPECT_LT(error, 0.5f);
    for (s32 ch = 0; ch < 3; ch++)
    {
        for (s32 c = 0; c < 4; c++)
        {
            EXPECT_NEAR(matrix[ch][c], ch == c ? 1.0f : 0.0f, 0.05f);
        }
    }
}

TEST(PaletteFit, SwappedChannelsAreFound)
{
    const std::vector<RGBA32> from = Ramp(64);
    std::vector<RGBA32> to = from;
    for (RGBA32& c : to)
    {
        std::swap(c.r, c.g);
    }

    f32 matrix[3][4];
    const f32 error = PaletteFit::Fit(from.data(), to.data(), 64, matrix);
    EXPECT_LT(error, 3.0f);

    f32 out[3];
    Apply(matrix, from[10], out);
    EXPECT_NEAR(out[0] * 255.0f, from[10].g, 6.0f);
    EXPECT_NEAR(out[1] * 255.0f, from[10].r, 6.0f);
    EXPECT_NEAR(out[2] * 255.0f, from[10].b, 6.0f);
}

TEST(PaletteFit, ATintIsFound)
{
    const std::vector<RGBA32> from = Ramp(64);
    std::vector<RGBA32> to = from;
    for (RGBA32& c : to)
    {
        c.r = static_cast<u8>(c.r * 0.5f);
        c.b = static_cast<u8>(std::min(255, c.b + 40));
    }

    f32 matrix[3][4];
    const f32 error = PaletteFit::Fit(from.data(), to.data(), 64, matrix);
    EXPECT_LT(error, 3.0f);
}

TEST(PaletteFit, TransparentEntriesAreLeftOut)
{
    std::vector<RGBA32> from = Ramp(16);
    std::vector<RGBA32> to = from;
    // Entry 5 is transparent in the other palette, whatever it was
    to[5] = {0, 0, 0, 0};

    f32 matrix[3][4];
    const f32 error = PaletteFit::Fit(from.data(), to.data(), 16, matrix);
    EXPECT_LT(error, 0.5f);
}

TEST(PaletteFit, NoColoursIsNoChange)
{
    const std::vector<RGBA32> pal(4);
    f32 matrix[3][4];
    PaletteFit::Fit(pal.data(), pal.data(), 4, matrix);
    EXPECT_FLOAT_EQ(matrix[0][0], 1.0f);
    EXPECT_FLOAT_EQ(matrix[1][1], 1.0f);
    EXPECT_FLOAT_EQ(matrix[2][2], 1.0f);
    EXPECT_FLOAT_EQ(matrix[0][3], 0.0f);
}
