#pragma once

#include <QString>
#include <QVector>
#include <memory>
#include "../../relive_lib/GameType.hpp"

// A mod project: the top-level container the editor works within, holding zero or more levels,
// each holding zero or more paths. On disk:
//   <mDirectory>/modinfo.json                          {"name":..., "author":..., "target_game":"AO"|"AE"}
//   <mDirectory>/levels/<levelDir>/level_info.json      {"paths":[{"path_id":"0"}, ...]}
//   <mDirectory>/levels/<levelDir>/<pathId>/path.json   exactly what Model::LoadJsonFromFile/ToJson read/write
// This mirrors the layout data_conversion.cpp (the base-game -> JSON extractor) already produces
// under relive_data/<ao|ae>/ - level dirs sit under a "levels" layer, as siblings of a top-level
// "sounds/<theme>/" dir (not modeled by EditorMod at all - the editor never touches sound assets)
// rather than directly under the mod root, so a mod root and a relive_data/<ao|ae> root end up
// structurally identical (see EditorMod::CopyLevelsFrom, used by "base a new mod on..."). There's
// no separate "paths" layer under a level either - a level's content already *is* its paths, so
// that would just be redundant nesting. A mod is explicitly "based on a base game" and is meant
// to eventually layer into the engine's resource loading the same way extracted game data does.
// Deliberately independent of relive_lib::Mods/Mod (used only by the engine's -mod= runtime
// flag, and currently implemented but not wired up to anything) - this is pure editor-UI code
// using Qt file APIs, so it doesn't need to touch shared engine code to get a working
// mod-browsing feature.
class EditorMod final
{
public:
    QString mDirectory;
    QString mName;
    QString mAuthor;
    GameType mTargetGame = GameType::eAo;

    // Creates mDirectory (must not already exist) and writes modinfo.json into it. Returns
    // nullptr if the directory couldn't be created or the file couldn't be written.
    static std::unique_ptr<EditorMod> CreateNew(const QString& dir, const QString& name, const QString& author, GameType targetGame);

    // Reads modinfo.json from an existing directory. Returns nullptr if it's missing or invalid.
    static std::unique_ptr<EditorMod> LoadFromDirectory(const QString& dir);

    // Rewrites modinfo.json from the current field values.
    bool SaveModInfo() const;

    // Immediate subdirectories of mDirectory/levels/ that have a level_info.json.
    QVector<QString> DiscoverLevels() const;

    // Path ids for a level, read from <levelDir>/level_info.json - falls back to scanning
    // <levelDir>/*/path.json if the manifest is missing or unreadable.
    QVector<int> DiscoverPathIds(const QString& levelDir) const;

    // Records pathId in <levelDir>/level_info.json (creating the level's dir and the manifest
    // file as needed). No-op if it's already recorded.
    void RecordPathId(const QString& levelDir, int pathId) const;

    // Creates <levelDir>/ under this mod's levels/ if it doesn't already exist. Returns whether
    // it exists (already did, or was just created) afterwards.
    bool CreateLevel(const QString& levelDir) const;

    // Recursively copies <sourceDir>/levels/ into <destModDir>/levels/ - used by "base a new mod
    // on an existing project" (see NewModDialog). sourceDir can be another mod's root or a
    // relive_data/<ao|ae> root (converted base game data) - both have the same levels/<name>/...
    // shape, so no special-casing is needed between the two. Camera images/JSON live alongside
    // path.json in the same per-path directory, so they're carried along automatically - no
    // separate handling needed. Returns false if sourceDir has no levels/ subfolder, or the copy
    // failed partway (destModDir may be left partially copied in that case).
    static bool CopyLevelsFrom(const QString& sourceDir, const QString& destModDir);

    QString LevelDir(const QString& levelDir) const;
    QString PathJsonFile(const QString& levelDir, int pathId) const;

private:
    QString ModInfoFile() const;
    QString LevelInfoFile(const QString& levelDir) const;
};
