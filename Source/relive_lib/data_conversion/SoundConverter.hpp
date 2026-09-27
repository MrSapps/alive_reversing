#pragma once

#include "file_system.hpp"
#include <set>
#include <string>
#include <vector>

class LvlReader;

// Converts the games' sound data to the formats the engine loads (see Sound/SOUND_FORMATS.md):
// each VAB (VH + VB, and AE's sounds.dat) to an SF2, and each SEQ to a MIDI file.
//
// Every path of a sound theme converts the theme's sounds again, so a VAB is only converted the
// first time its theme asks for it, and AE's sounds.dat is only read once.
class SoundConverter final
{
public:
    SoundConverter(FileSystem& fs, bool isAo);

    // Writes <themeDir>/<VH name>.sf2 (MINES.VH -> MINES.sf2) if this hasn't yet, returns its file
    // name
    std::string ConvertSoundBank(LvlReader& lvlReader, const FileSystem::Path& themeDir, const std::string& vhName, const std::string& vbName);

    // Writes a SEQ as <themeDir>/<SEQ name>.mid (OPTAMB.SEQ -> OPTAMB.mid), returns its file name.
    // Empty if seq isn't a SEQ.
    std::string ConvertSeq(const FileSystem::Path& themeDir, const std::string& seqName, const std::vector<u8>& seq);

private:
    const std::vector<u8>& SoundsDat();

    FileSystem& mFs;
    bool mIsAo = false;
    bool mSoundsDatLoaded = false;
    std::vector<u8> mSoundsDat;
    std::set<std::string> mConvertedBanks;
};
