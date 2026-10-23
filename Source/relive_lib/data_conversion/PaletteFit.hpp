#pragma once

#include "../Types.hpp"
#include "../AnimResources.hpp"
#include "rgb_conversion.hpp"
#include "file_system.hpp"

struct AnimationPal;

// Sprites are true colour, so another palette for a sprite (the Mudokon's moods, the blue rock in
// the stockyards, the colours of the background Glukkons) can't be a palette. It is a change of
// colours instead: a 3x4 matrix, out = matrix * (r, g, b, 1), made here from the sprite's own
// palette and the other palette by least squares. The sprites of one kind (all the Mudokon's) use
// the one palette layout, so one matrix serves them all.
class PaletteFit final
{
public:
    // Forget everything that was recorded, to convert another game
    static void Reset();

    // The palette a sprite is made with. Only the animations a palette is fitted to are kept.
    static void AddAnimPalette(AnimId anim, bool isAo, const AnimationPal& pal, u32 depth);

    // The other palette: PSX palette entries
    static void AddVariantPalette(PalId pal, const u16* pEntries, u32 count);

    // Writes the matrix of every palette that has both palettes. Returns how many were written.
    static u32 Write(FileSystem& fs, const FileSystem::Path& dataDir, bool isAo);

    // Which animation's palette is the one to fit to, AnimId::None if this palette isn't fitted
    static AnimId AnimFor(PalId pal, bool isAo);

    // The matrix that takes the colours of from to those of to, which are count entries (entry 0,
    // and any that is transparent in either, is left out). Both are 0 to 255 per channel.
    // Returns the average error per channel, 0 to 255.
    static f32 Fit(const RGBA32* from, const RGBA32* to, u32 count, f32 (&matrix)[3][4]);
};
