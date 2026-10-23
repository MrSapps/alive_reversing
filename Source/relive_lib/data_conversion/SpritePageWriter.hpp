#pragma once

#include "AnimationConverter.hpp"
#include "file_system.hpp"
#include <string>
#include <vector>

// The sprite sheets of the animations, and the pages they are packed into.
//
// Each animation has a sheet: animations/<group>/<name>.png (and <name>_mask.png, see
// SpriteMask.hpp), with <name>.json saying where its frames are in it. The data conversion writes
// them, and they are what mods and the Blender add-on write too. They are the source: nothing
// here changes them.
//
// The pages are made from the sheets, frame by frame (the sheets are laid out like the original
// game's textures and have holes, which the pages don't). The sheets of a group (the animations
// of a BAN file, which a level loads together) go on the same pages, so the renderers upload one
// texture for many animations and sprites of the animations batch together. An animation on a
// page gets <name>.packed.json next to its json, which says which page it is on and where its
// frames are on it; the game reads that one instead of the json if it is in the same data
// directory, so a mod's own sheet of the animation is not affected by it. An animation that
// doesn't fit on a page has no packed json and is used from its sheet.
//
// A page is as big as what is on it needs, up to pageSize (a power of two). The biggest page a
// GPU can take depends on the GPU (see DataConversion::SpritePageSizeFor), so the pages are made
// again, from the sheets, when that changes, or when a sheet is newer than the pages.
class SpritePageWriter final
{
public:
    // Where the pages are, in the data directory
    static constexpr char kPagesDirectory[] = "animations/pages";
    static constexpr char kPackedJsonExtension[] = ".packed.json";

    // Writes the animation's sheet and json. animPath is where they go, without the extension
    // (dataDir/animations/<group>/<name>).
    static void SaveSheet(FileSystem& fs, const FileSystem::Path& animPath, const AnimationConverter::Sheet& sheet);

    // The list of what the pages were made from, and how: in the pages' directory
    static constexpr char kManifestPath[] = "animations/pages/sources.txt";

    // Packs the sheets under dataDir/animations into pages and writes the packed jsons.
    // Returns the number of pages.
    static u32 Repack(FileSystem& fs, const FileSystem::Path& dataDir, u32 pageSize);

    // The same for a mod: layers are the data directories of the mod and the game it is for, the
    // mod first, as the game searches them. The animations are the ones the game would use (the
    // mod's sheet if it has one, the game's if it doesn't), so a group the mod only partly
    // replaces is still on pages together. The pages and packed jsons are made in the first
    // directory, and what the later ones have is left alone. A mod without sheets gets none.
    static u32 Repack(FileSystem& fs, const std::vector<std::string>& layers, u32 pageSize);

    // Whether the pages are out of date: there are sheets and no pages, the sheets (or the page
    // size) are not what the pages were made from, or a sheet in any of the directories is newer
    // than the oldest page
    static bool PagesStale(const FileSystem::Path& dataDir, u32 pageSize);
    static bool PagesStale(const std::vector<std::string>& layers, u32 pageSize);
};
