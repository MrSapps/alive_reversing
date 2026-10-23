#pragma once

#include "Types.hpp"

// The optional mask of a true colour sprite sheet: a second RGBA image the size of the sheet
// (name_mask.png next to name.png) with a flag for each pixel in its colour channels. The sheet's
// own alpha is only how much of the pixel is there (0 to 255, so a soft edge is fine). What the
// PSX kept in the sprite's palette entries is in the mask:
//
// - R: the pixel is solid, a semi-transparent sprite doesn't blend it (the palette entry didn't
//   have the STP bit)
// - G: the pixel glows, the sprite's tint (the light it is in) doesn't change it (the eyes of the
//   Sligs and Glukkons)
// - B: the invisibility effect leaves the pixel alone (the first palette entries, which the
//   original's fade never touched)
//
// A pixel with no flag set is the default, and a sheet with no mask has none set anywhere: it
// blends where the sprite is semi-transparent, is tinted and is faded by the invisibility.
namespace SpriteMask
{
constexpr u8 kSolid = 1 << 0;
constexpr u8 kGlow = 1 << 1;
constexpr u8 kKeepVisible = 1 << 2;

inline void Set(u8* pTexel, u8 flags)
{
    pTexel[0] = (flags & kSolid) ? 255 : 0;
    pTexel[1] = (flags & kGlow) ? 255 : 0;
    pTexel[2] = (flags & kKeepVisible) ? 255 : 0;
    pTexel[3] = 255;
}

inline u8 Get(const u8* pTexel)
{
    return static_cast<u8>((pTexel[0] >= 128 ? kSolid : 0) | (pTexel[1] >= 128 ? kGlow : 0) | (pTexel[2] >= 128 ? kKeepVisible : 0));
}
} // namespace SpriteMask
