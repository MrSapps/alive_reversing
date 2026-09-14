#pragma once

#include <QString>
#include <QVector>
#include <memory>
#include "../../relive_lib/GameType.hpp"

// A mod project: the top-level container the editor works within, holding zero or more levels,
// each holding zero or more paths. On disk:
//   <mDirectory>/modinfo.json                         {"name":..., "author":..., "target_game":"AO"|"AE"}
//   <mDirectory>/<levelDir>/paths/level_info.json      {"paths":[{"path_id":"0"}, ...]}
//   <mDirectory>/<levelDir>/paths/<pathId>/path.json   exactly what Model::LoadJsonFromFile/ToJson read/write
// This mirrors the layout data_conversion.cpp (the base-game -> JSON extractor) already produces,
// since a mod is explicitly "based on a base game" and is meant to eventually layer into the
// engine's resource loading the same way extracted game data does. Deliberately independent of
// relive_lib::Mods/Mod (used only by the engine's -mod= runtime flag, and currently a dead,
// unimplemented stub) - this is pure editor-UI code using Qt file APIs, so it doesn't need to
// touch shared engine code to get a working mod-browsing feature.
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

    // Immediate subdirectories of mDirectory that contain a paths/ folder.
    QVector<QString> DiscoverLevels() const;

    // Path ids for a level, read from <levelDir>/paths/level_info.json - falls back to scanning
    // <levelDir>/paths/*/path.json if the manifest is missing or unreadable.
    QVector<int> DiscoverPathIds(const QString& levelDir) const;

    // Records pathId in <levelDir>/paths/level_info.json (creating the level's paths/ dir and
    // the manifest file as needed). No-op if it's already recorded.
    void RecordPathId(const QString& levelDir, int pathId) const;

    // Creates <levelDir>/paths/ under this mod if it doesn't already exist. Returns whether it
    // exists (already did, or was just created) afterwards.
    bool CreateLevel(const QString& levelDir) const;

    QString LevelPathsDir(const QString& levelDir) const;
    QString PathJsonFile(const QString& levelDir, int pathId) const;

private:
    QString ModInfoFile() const;
    QString LevelInfoFile(const QString& levelDir) const;
};
