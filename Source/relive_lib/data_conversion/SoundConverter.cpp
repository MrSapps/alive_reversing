#include "../stdafx.h"
#include "SoundConverter.hpp"
#include "LvlReader.hpp"
#include "../Sound/SoundFont.hpp"
#include "../Sound/VabSoundFont.hpp"
#include "../Sound/SeqMidi.hpp"
#include "../FatalError.hpp"

SoundConverter::SoundConverter(FileSystem& fs, bool isAo)
    : mFs(fs)
    , mIsAo(isAo)
{
}

static std::string Stem(const std::string& fileName)
{
    return fileName.substr(0, fileName.find('.'));
}

const std::vector<u8>& SoundConverter::SoundsDat()
{
    if (!mSoundsDatLoaded)
    {
        mSoundsDatLoaded = true;
        mSoundsDat = mFs.LoadToVec("sounds.dat");
        if (mSoundsDat.empty())
        {
            // The game played no sound at all without it
            LOG_ERROR("sounds.dat is missing, AE's sound banks will have no samples");
        }
    }
    return mSoundsDat;
}

std::string SoundConverter::ConvertSoundBank(LvlReader& lvlReader, const FileSystem::Path& themeDir, const std::string& vhName, const std::string& vbName)
{
    const std::string sf2Name = Stem(vhName) + ".sf2";
    FileSystem::Path sf2Path = themeDir;
    sf2Path.Append(sf2Name);
    if (!mConvertedBanks.insert(sf2Path.GetPath()).second)
    {
        return sf2Name;
    }

    const std::optional<std::vector<u8>> vh = lvlReader.ReadFile(vhName.c_str());
    const std::optional<std::vector<u8>> vb = lvlReader.ReadFile(vbName.c_str());
    if (!vh || !vb)
    {
        ALIVE_FATAL("Missing sound bank %s/%s", vhName.c_str(), vbName.c_str());
    }

    VabSoundFont::Vab vab;
    vab.mVh = *vh;
    const bool ok = mIsAo ? VabSoundFont::ReadVbAo(*vh, *vb, vab.mSamples) : VabSoundFont::ReadVbAe(*vh, *vb, SoundsDat(), vab.mSamples);
    if (!ok)
    {
        ALIVE_FATAL("%s isn't a VAB header", vhName.c_str());
    }

    mFs.CreateDirectory(themeDir);
    mFs.Save(sf2Path, VabSoundFont::ToSoundFont(vab, Stem(vhName)).Write());
    return sf2Name;
}

std::string SoundConverter::ConvertSeq(const FileSystem::Path& themeDir, const std::string& seqName, const std::vector<u8>& seq)
{
    std::vector<u8> midi;
    std::string error;
    if (!SeqMidi::SeqToSmf(seq, midi, error))
    {
        LOG_ERROR("Can't convert %s: %s", seqName.c_str(), error.c_str());
        return {};
    }

    const std::string midiName = Stem(seqName) + ".mid";
    FileSystem::Path midiPath = themeDir;
    midiPath.Append(midiName);
    mFs.CreateDirectory(themeDir);
    mFs.Save(midiPath, midi);
    return midiName;
}
