#include "EditorMod.hpp"
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <nlohmann/json.hpp>

namespace
{
    QString GameToString(GameType game)
    {
        return game == GameType::eAo ? "AO" : "AE";
    }

    GameType GameFromString(const std::string& s)
    {
        return s == "AE" ? GameType::eAe : GameType::eAo;
    }

    // PathLine.mLineType-style leniency: the real data_conversion.cpp writer stores "path_id" as
    // a JSON string ("0"), not a number, in level_info.json - unlike path.json's own "path_id"
    // (an int, see Model.cpp's ReadNumber usage). Accept either so a mod created here stays
    // readable if real converted level data (or a hand-edited manifest) is later dropped in
    // alongside it.
    bool TryReadPathId(const nlohmann::json& entry, int& outId)
    {
        if (!entry.contains("path_id"))
        {
            return false;
        }
        const nlohmann::json& v = entry.at("path_id");
        if (v.is_number_integer())
        {
            outId = v.get<int>();
            return true;
        }
        if (v.is_string())
        {
            bool ok = false;
            const int id = QString::fromStdString(v.get<std::string>()).toInt(&ok);
            if (ok)
            {
                outId = id;
                return true;
            }
        }
        return false;
    }

    bool ReadJsonFile(const QString& fileName, nlohmann::json& outJson)
    {
        QFile f(fileName);
        if (!f.open(QIODevice::ReadOnly))
        {
            return false;
        }
        QTextStream stream(&f);
        const std::string contents = stream.readAll().toStdString();
        try
        {
            outJson = nlohmann::json::parse(contents);
        }
        catch (const nlohmann::json::exception&)
        {
            return false;
        }
        return true;
    }

    bool WriteJsonFile(const QString& fileName, const nlohmann::json& json)
    {
        QFile f(fileName);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            return false;
        }
        QTextStream stream(&f);
        stream << QString::fromStdString(json.dump(4));
        return true;
    }
}

QString EditorMod::ModInfoFile() const
{
    return mDirectory + "/modinfo.json";
}

QString EditorMod::LevelDir(const QString& levelDir) const
{
    return mDirectory + "/levels/" + levelDir;
}

QString EditorMod::LevelInfoFile(const QString& levelDir) const
{
    return LevelDir(levelDir) + "/level_info.json";
}

QString EditorMod::PathJsonFile(const QString& levelDir, int pathId) const
{
    return LevelDir(levelDir) + "/" + QString::number(pathId) + "/path.json";
}

std::unique_ptr<EditorMod> EditorMod::CreateNew(const QString& dir, const QString& name, const QString& author, GameType targetGame)
{
    // Allow an already-existing directory as long as it's empty - lets a caller offer "browse to
    // an existing folder, or create+select a new one" via QFileDialog::getExistingDirectory
    // (which can only pick directories that already exist), not just brand new paths.
    if (QDir(dir).exists() && !QDir(dir).isEmpty())
    {
        return nullptr;
    }
    if (!QDir().mkpath(dir))
    {
        return nullptr;
    }

    auto mod = std::make_unique<EditorMod>();
    mod->mDirectory = dir;
    mod->mName = name;
    mod->mAuthor = author;
    mod->mTargetGame = targetGame;

    if (!mod->SaveModInfo())
    {
        return nullptr;
    }
    return mod;
}

std::unique_ptr<EditorMod> EditorMod::LoadFromDirectory(const QString& dir)
{
    auto mod = std::make_unique<EditorMod>();
    mod->mDirectory = dir;

    nlohmann::json json;
    if (!ReadJsonFile(mod->ModInfoFile(), json))
    {
        return nullptr;
    }

    mod->mName = QString::fromStdString(json.value("name", std::string()));
    mod->mAuthor = QString::fromStdString(json.value("author", std::string()));
    mod->mTargetGame = GameFromString(json.value("target_game", std::string("AO")));
    return mod;
}

bool EditorMod::SaveModInfo() const
{
    const nlohmann::json json = {
        {"name", mName.toStdString()},
        {"author", mAuthor.toStdString()},
        {"target_game", GameToString(mTargetGame).toStdString()},
    };
    return WriteJsonFile(ModInfoFile(), json);
}

QVector<QString> EditorMod::DiscoverLevels() const
{
    // levels/ is a dedicated subdir now (sounds/mods etc. are siblings of it, not inside it -
    // see this class's own doc comment), so every subdirectory of it is a level candidate with
    // no further filtering needed - unlike before "levels/" existed, when this had to scan the
    // mod root directly and tell a real level dir apart from anything else living there. A
    // level shows up here as soon as CreateLevel makes its directory, even before it has any
    // paths (level_info.json) yet - don't require that too, or a just-created empty level would
    // vanish from the tree until its first path is added.
    QVector<QString> levels;
    const QString levelsDir = mDirectory + "/levels";
    if (!QDir(levelsDir).exists())
    {
        // Brand new mod - nothing created under levels/ yet.
        return levels;
    }
    const QStringList entries = QDir(levelsDir).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString& entry : entries)
    {
        levels.push_back(entry);
    }
    return levels;
}

QVector<int> EditorMod::DiscoverPathIds(const QString& levelDir) const
{
    QVector<int> ids;

    nlohmann::json json;
    if (ReadJsonFile(LevelInfoFile(levelDir), json) && json.contains("paths") && json.at("paths").is_array())
    {
        for (const auto& entry : json.at("paths"))
        {
            int id = 0;
            if (TryReadPathId(entry, id))
            {
                ids.push_back(id);
            }
        }
        return ids;
    }

    // Manifest missing/unreadable - fall back to scanning for <id>/path.json subdirectories
    // directly, so a hand-assembled or partially-converted level still shows its paths.
    const QStringList entries = QDir(LevelDir(levelDir)).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString& entry : entries)
    {
        bool ok = false;
        const int id = entry.toInt(&ok);
        if (ok && QFile::exists(LevelDir(levelDir) + "/" + entry + "/path.json"))
        {
            ids.push_back(id);
        }
    }
    return ids;
}

void EditorMod::RecordPathId(const QString& levelDir, int pathId) const
{
    CreateLevel(levelDir);

    nlohmann::json json;
    if (!ReadJsonFile(LevelInfoFile(levelDir), json) || !json.contains("paths") || !json.at("paths").is_array())
    {
        json = nlohmann::json::object();
        json["paths"] = nlohmann::json::array();
    }

    for (const auto& entry : json.at("paths"))
    {
        int existingId = 0;
        if (TryReadPathId(entry, existingId) && existingId == pathId)
        {
            return;
        }
    }

    json["paths"].push_back({{"path_id", QString::number(pathId).toStdString()}});
    WriteJsonFile(LevelInfoFile(levelDir), json);
}

bool EditorMod::CreateLevel(const QString& levelDir) const
{
    QDir().mkpath(LevelDir(levelDir));
    return QDir(LevelDir(levelDir)).exists();
}

bool EditorMod::CopyLevelsFrom(const QString& sourceDir, const QString& destModDir)
{
    const QString sourceLevelsDir = sourceDir + "/levels";
    if (!QDir(sourceLevelsDir).exists())
    {
        return false;
    }

    const QString destLevelsDir = destModDir + "/levels";
    if (!QDir().mkpath(destLevelsDir))
    {
        return false;
    }

    // Subdirectories first (QDirIterator::Subdirectories walks depth-first, but doesn't
    // guarantee a directory is yielded before its own contents) - two passes keeps this simple
    // and correct rather than relying on iteration order: create every directory, then copy
    // every file into an already-fully-created tree.
    QDirIterator dirWalker(sourceLevelsDir, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (dirWalker.hasNext())
    {
        const QString sourceSubDir = dirWalker.next();
        const QString relative = QDir(sourceLevelsDir).relativeFilePath(sourceSubDir);
        if (!QDir().mkpath(destLevelsDir + "/" + relative))
        {
            return false;
        }
    }

    QDirIterator fileWalker(sourceLevelsDir, QDir::Files, QDirIterator::Subdirectories);
    while (fileWalker.hasNext())
    {
        const QString sourceFile = fileWalker.next();
        const QString relative = QDir(sourceLevelsDir).relativeFilePath(sourceFile);
        const QString destFile = destLevelsDir + "/" + relative;
        if (QFile::exists(destFile))
        {
            QFile::remove(destFile);
        }
        if (!QFile::copy(sourceFile, destFile))
        {
            return false;
        }
    }

    return true;
}
