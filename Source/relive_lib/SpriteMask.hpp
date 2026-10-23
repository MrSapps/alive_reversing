#pragma once

#include "Types.hpp"

// The optional mask of a true colour sprite sheet: a second RGBA image the size of the sheet
// (name_mask.png next to name.png) with a flag for each pixel in its colour channels. The sheet's
// own alpha is how much of the pixel is there (0 to 255, so a soft edge is fine), and in a
// semi-transparent sprite alpha 255 is solid: only the pixels below it blend, by their alpha.
// What else the PSX kept in the sprite's palette entries is in the mask:
//
// - R: not used
// - G: how much the pixel glows, 0 to 255: the sprite's tint (the light it is in) changes it by
//   the rest (the eyes of the Sligs and Glukkons are 255, an HD sheet can fall off towards the edge)
// - B: the invisibility effect leaves the pixel alone (the first palette entries, which the
//   original's fade never touched)
//
// A pixel with no flag set is the default, and a sheet with no mask has none set anywhere: it
// is tinted and is faded by the invisibility.
namespace SpriteMask
{
constexpr u8 kGlow = 1 << 1;
constexpr u8 kKeepVisible = 1 << 2;

inline void Set(u8* pTexel, u8 flags)
{
    pTexel[0] = 0;
    pTexel[1] = (flags & kGlow) ? 255 : 0;
    pTexel[2] = (flags & kKeepVisible) ? 255 : 0;
    pTexel[3] = 255;
}

inline u8 Get(const u8* pTexel)
{
    return static_cast<u8>((pTexel[1] >= 128 ? kGlow : 0) | (pTexel[2] >= 128 ? kKeepVisible : 0));
}

// How much the pixel glows, 0 to 255
inline u8 Glow(const u8* pTexel)
{
    return pTexel[1];
}

// The pixel as the renderers use it: the glow in the low byte, 256 if the invisibility leaves it alone
inline u16 Pack(const u8* pTexel)
{
    return static_cast<u16>(pTexel[1] | (pTexel[2] >= 128 ? 256 : 0));
}

inline u8 PackedGlow(u16 packed)
{
    return static_cast<u8>(packed & 0xFF);
}

inline bool PackedKeepVisible(u16 packed)
{
    return (packed & 256) != 0;
}
} // namespace SpriteMask
