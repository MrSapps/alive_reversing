#pragma once

#include "Types.hpp"
#include <cstring>
#include <vector>

// Colour effects on a true colour sprite. On the PSX these were done by changing the sprite's
// palette; sprites are true colour now, so each effect is a change to the colours of the pixels
// that the renderers do as they draw. See Renderer/PALETTE_EFFECTS_MASTER.md for what the
// original effects did, and each member for the effect it takes the place of.
struct SpriteFx final
{
    static constexpr u32 kMaxSweeps = 3;
    static constexpr u32 kLumaPoints = 32;

    // A colour replaces the pixels from the darkest up to progress (0 to 1), the way a palette
    // overwritten entry by entry does (the electrocution): the entries went in the palette's
    // order, which a true colour sprite has no equivalent of, so it is brightness order. A
    // pixel's place in it is its rank among the sprite's colours (see mLuma), so that a sweep
    // goes over a sprite at an even pace whatever its colours are, like a palette's entries.
    // Later sweeps are over earlier ones.
    struct Sweep final
    {
        f32 mProgress = 0.0f;
        f32 mR = 0.0f;
        f32 mG = 0.0f;
        f32 mB = 0.0f;
        // 0: the colour is opaque, 1: it has the PSX semi-transparency bit (blended if the sprite
        // is semi-transparent), 2: it is the transparent colour, the pixels are erased
        u32 mKind = 0;
    };

    // The colour of every pixel becomes matrix * (r, g, b, 1), 0 to 1 per channel. Another
    // palette for the same sprite (the Mudokon's moods, the blue rock) is one of these, worked
    // out by the data conversion from the two palettes.
    f32 mMatrix[3][4] = {{1.0f, 0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 0.0f}};

    // Taken from every channel, 0 to 1, after the matrix (the invisibility fading). Pixels the
    // sheet's mask keeps visible (SpriteMask.hpp) are left alone.
    f32 mSubtract = 0.0f;

    // Every pixel of the sprite can be blended if it is semi-transparent: the palette entries the
    // invisibility sets the semi-transparency bit of (not the ones the mask keeps visible)
    u32 mForceBlend = 0;

    Sweep mSweeps[kMaxSweeps] = {};

    // The brightness (0 to 1) of the colour at each of kLumaPoints evenly spaced ranks of the
    // sprite's colours, darkest to brightest. The default is a sprite with colours of every
    // brightness. See AnimAttributes::mLuma.
    f32 mLuma[kLumaPoints] = {};

    SpriteFx()
    {
        for (u32 i = 0; i < kLumaPoints; i++)
        {
            mLuma[i] = static_cast<f32>(i) / (kLumaPoints - 1);
        }
    }

    bool operator==(const SpriteFx& other) const
    {
        return memcmp(this, &other, sizeof(SpriteFx)) == 0;
    }

    bool operator!=(const SpriteFx& other) const
    {
        return !(*this == other);
    }

    // True if the sprite is drawn as it is
    bool IsIdentity() const
    {
        const SpriteFx none;
        if (memcmp(mMatrix, none.mMatrix, sizeof(mMatrix)) != 0 || mSubtract != 0.0f || mForceBlend != 0)
        {
            return false;
        }

        for (const Sweep& sweep : mSweeps)
        {
            if (sweep.mProgress > 0.0f)
            {
                return false;
            }
        }
        return true;
    }

    // Takes the sweeps off, and the ranks they use
    void ClearSweeps()
    {
        const SpriteFx none;
        memcpy(mSweeps, none.mSweeps, sizeof(mSweeps));
        memcpy(mLuma, none.mLuma, sizeof(mLuma));
    }

    u32 Hash() const
    {
        // FNV-1a over the bytes: there is no padding, all the members are 4 bytes
        const u8* pBytes = reinterpret_cast<const u8*>(this);
        u32 hash = 2166136261u;
        for (std::size_t i = 0; i < sizeof(SpriteFx); i++)
        {
            hash = (hash ^ pBytes[i]) * 16777619u;
        }
        return hash;
    }

    // The brightnesses of the colours in rgba (8 bit RGBA pixels, the ones that are at least half
    // there) from darkest to brightest, sampled at kLumaPoints evenly spaced ranks. The colours
    // count as the PSX had them, 5 bits each, so there are as many as the palette had entries.
    // Leaves luma as it is (an even ramp) if there are none.
    static void ComputeLuma(const std::vector<u8>& rgba, f32 (&luma)[kLumaPoints]);

    void SetLuma(const f32 (&luma)[kLumaPoints])
    {
        memcpy(mLuma, luma, sizeof(mLuma));
    }

    void SetMatrix(const f32 (&matrix)[3][4])
    {
        memcpy(mMatrix, matrix, sizeof(mMatrix));
    }

    void ResetMatrix()
    {
        const SpriteFx identity;
        memcpy(mMatrix, identity.mMatrix, sizeof(mMatrix));
    }
};
