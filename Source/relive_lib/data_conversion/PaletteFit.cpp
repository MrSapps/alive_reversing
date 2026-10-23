#include "PaletteFit.hpp"
#include "AnimationConverter.hpp"
#include "AnimConversionInfo.hpp"
#include "../logger.hpp"
#include "../FatalError.hpp"
#include <algorithm>
#include <cmath>
#include <map>

struct AnimPaletteRecord final
{
    AnimationPal mPal;
    u32 mDepth = 0;
};

static std::map<std::pair<AnimId, bool>, AnimPaletteRecord> sAnimPalettes;
static std::map<PalId, std::vector<RGBA32>> sVariantPalettes;

struct PaletteFitInfo final
{
    PalId mPal;
    AnimId mAeAnim;
    AnimId mAoAnim;
};

static const PaletteFitInfo kFits[] = {
    {PalId::BlindMud, AnimId::Mudokon_Idle, AnimId::None},
    {PalId::AngryMud, AnimId::Mudokon_Idle, AnimId::None},
    {PalId::SadMud, AnimId::Mudokon_Idle, AnimId::None},
    {PalId::WiredMud, AnimId::Mudokon_Idle, AnimId::None},
    {PalId::SickMud, AnimId::Mudokon_Idle, AnimId::None},
    // The gibs are made from Abe's gib sprites
    {PalId::BlindMudGib, AnimId::BlindMud_Head_Gib, AnimId::None},
    {PalId::GreenFlash, AnimId::Bomb_RedGreenTick, AnimId::Bomb_RedGreenTick},
    {PalId::BlueRock, AnimId::None, AnimId::Rock},
    {PalId::WhiteHighlite, AnimId::None, AnimId::MenuHighlight_Circle},
    {PalId::RedBackgroundGlukkon, AnimId::None, AnimId::Background_Glukkon_Idle},
    {PalId::BlueBackgroundGlukkon, AnimId::None, AnimId::Background_Glukkon_Idle},
    {PalId::AquaBackgroundGlukkon, AnimId::None, AnimId::Background_Glukkon_Idle},
    {PalId::GreenBackgroundGlukkon, AnimId::None, AnimId::Background_Glukkon_Idle},
    {PalId::BlueRockSack, AnimId::None, AnimId::RockSack_Idle},
    {PalId::StockYardsAbeGib, AnimId::None, AnimId::Abe_Head_Gib},
    {PalId::StockYardsSlogGib, AnimId::None, AnimId::Slog_Head_Gib},
    {PalId::Bomb, AnimId::None, AnimId::TimedMine_Idle},
    {PalId::StockYardsSlig, AnimId::None, AnimId::Slig_Idle},
};

void PaletteFit::Reset()
{
    sAnimPalettes.clear();
    sVariantPalettes.clear();
}

AnimId PaletteFit::AnimFor(PalId pal, bool isAo)
{
    for (const PaletteFitInfo& fit : kFits)
    {
        if (fit.mPal == pal)
        {
            return isAo ? fit.mAoAnim : fit.mAeAnim;
        }
    }
    return AnimId::None;
}

void PaletteFit::AddAnimPalette(AnimId anim, bool isAo, const AnimationPal& pal, u32 depth)
{
    for (const PaletteFitInfo& fit : kFits)
    {
        if ((isAo ? fit.mAoAnim : fit.mAeAnim) == anim)
        {
            sAnimPalettes[{anim, isAo}] = {pal, depth};
            return;
        }
    }
}

void PaletteFit::AddVariantPalette(PalId pal, const u16* pEntries, u32 count)
{
    std::vector<RGBA32>& entries = sVariantPalettes[pal];
    entries.resize(count);
    for (u32 i = 0; i < count; i++)
    {
        entries[i] = pEntries[i] == 0 ? RGBA32{0, 0, 0, 0} : RGBConversion::RGBA555ToRGBA888Components(pEntries[i]);
    }
}

// Solves the n x n system a * x = b in place (Gaussian elimination, with pivoting)
template <std::size_t N>
static bool Solve(f64 (&a)[N][N], f64 (&b)[N])
{
    for (std::size_t col = 0; col < N; col++)
    {
        std::size_t pivot = col;
        for (std::size_t row = col + 1; row < N; row++)
        {
            if (std::fabs(a[row][col]) > std::fabs(a[pivot][col]))
            {
                pivot = row;
            }
        }
        if (std::fabs(a[pivot][col]) < 1e-12)
        {
            return false;
        }
        if (pivot != col)
        {
            for (std::size_t k = 0; k < N; k++)
            {
                std::swap(a[col][k], a[pivot][k]);
            }
            std::swap(b[col], b[pivot]);
        }
        for (std::size_t row = col + 1; row < N; row++)
        {
            const f64 factor = a[row][col] / a[col][col];
            for (std::size_t k = col; k < N; k++)
            {
                a[row][k] -= factor * a[col][k];
            }
            b[row] -= factor * b[col];
        }
    }
    for (std::size_t i = N; i-- > 0;)
    {
        for (std::size_t k = i + 1; k < N; k++)
        {
            b[i] -= a[i][k] * b[k];
        }
        b[i] /= a[i][i];
    }
    return true;
}

f32 PaletteFit::Fit(const RGBA32* from, const RGBA32* to, u32 count, f32 (&matrix)[3][4])
{
    // Start from no change
    for (s32 row = 0; row < 3; row++)
    {
        for (s32 col = 0; col < 4; col++)
        {
            matrix[row][col] = row == col ? 1.0f : 0.0f;
        }
    }

    // Least squares for each output channel: minimise the squared difference between
    // m . (r, g, b, 1) and the colour it should be, with a little pull towards "no change" so
    // that a palette with only a few colours, or all on a line, still gives something sane.
    constexpr f64 kPull = 0.001;
    f64 normal[4][4] = {};
    f64 rhs[3][4] = {};
    u32 used = 0;
    for (u32 i = 1; i < count; i++)
    {
        const bool fromTransparent = from[i].r == 0 && from[i].g == 0 && from[i].b == 0 && from[i].a == 0;
        const bool toTransparent = to[i].r == 0 && to[i].g == 0 && to[i].b == 0 && to[i].a == 0;
        if (fromTransparent || toTransparent)
        {
            continue;
        }

        const f64 in[4] = {from[i].r / 255.0, from[i].g / 255.0, from[i].b / 255.0, 1.0};
        const f64 out[3] = {to[i].r / 255.0, to[i].g / 255.0, to[i].b / 255.0};
        for (s32 r = 0; r < 4; r++)
        {
            for (s32 c = 0; c < 4; c++)
            {
                normal[r][c] += in[r] * in[c];
            }
            for (s32 ch = 0; ch < 3; ch++)
            {
                rhs[ch][r] += in[r] * out[ch];
            }
        }
        used++;
    }

    if (used == 0)
    {
        return 0.0f;
    }

    for (s32 ch = 0; ch < 3; ch++)
    {
        f64 a[4][4];
        f64 b[4];
        for (s32 r = 0; r < 4; r++)
        {
            for (s32 c = 0; c < 4; c++)
            {
                a[r][c] = normal[r][c];
            }
            // Pull towards identity (row ch of the matrix is 1 at column ch, 0 elsewhere)
            a[r][r] += kPull * used;
            b[r] = rhs[ch][r] + (r == ch ? kPull * used : 0.0);
        }
        if (Solve(a, b))
        {
            for (s32 c = 0; c < 4; c++)
            {
                matrix[ch][c] = static_cast<f32>(b[c]);
            }
        }
    }

    // How close it came
    f64 error = 0.0;
    for (u32 i = 1; i < count; i++)
    {
        const bool fromTransparent = from[i].r == 0 && from[i].g == 0 && from[i].b == 0 && from[i].a == 0;
        const bool toTransparent = to[i].r == 0 && to[i].g == 0 && to[i].b == 0 && to[i].a == 0;
        if (fromTransparent || toTransparent)
        {
            continue;
        }
        const f32 in[4] = {from[i].r / 255.0f, from[i].g / 255.0f, from[i].b / 255.0f, 1.0f};
        const f32 out[3] = {to[i].r / 255.0f, to[i].g / 255.0f, to[i].b / 255.0f};
        for (s32 ch = 0; ch < 3; ch++)
        {
            f32 v = 0.0f;
            for (s32 c = 0; c < 4; c++)
            {
                v += matrix[ch][c] * in[c];
            }
            v = std::min(1.0f, std::max(0.0f, v));
            error += static_cast<f64>(std::fabs(v - out[ch]));
        }
    }
    return static_cast<f32>(error / (used * 3.0) * 255.0);
}

u32 PaletteFit::Write(FileSystem& fs, const FileSystem::Path& dataDir, bool isAo)
{
    u32 written = 0;
    for (const PaletteFitInfo& fit : kFits)
    {
        const AnimId anim = isAo ? fit.mAoAnim : fit.mAeAnim;
        if (anim == AnimId::None)
        {
            continue;
        }

        auto animPal = sAnimPalettes.find({anim, isAo});
        auto variant = sVariantPalettes.find(fit.mPal);
        if (animPal == sAnimPalettes.end() || variant == sVariantPalettes.end())
        {
            LOG_WARNING("No palette change for %s: the %s palette wasn't converted", ToString(fit.mPal), animPal == sAnimPalettes.end() ? "sprite's" : "other");
            continue;
        }

        const u32 count = std::min<u32>(static_cast<u32>(variant->second.size()), std::min<u32>(animPal->second.mDepth, 256));
        f32 matrix[3][4];
        const f32 error = Fit(animPal->second.mPal.mPal, variant->second.data(), count, matrix);
        LOG_INFO("Palette change %s: %u colours, average error %.1f of 255", ToString(fit.mPal), count, static_cast<f64>(error));

        FileSystem::Path path = dataDir;
        path.Append(ToString(fit.mPal));
        AutoFILE f = fs.OpenFile(path.GetPath().c_str(), "wb");
        f.Write(reinterpret_cast<const u8*>(matrix), sizeof(matrix));
        written++;
    }
    return written;
}
