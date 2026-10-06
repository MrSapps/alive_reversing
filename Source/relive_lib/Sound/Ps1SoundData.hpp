#pragma once

#include "../Types.hpp"
#include <map>
#include <string>
#include <vector>

class FileSystem;

// The PS1 games' own sound data (-ps1_data=<dir>), read straight from their LVL files (the same
// container as the PC's), so -ps1_data can play the PS1's VH, VB and SEQ files instead of the PC's.
// The PC's samples are the PS1 encoder's input, so they are close but not the same: the PS1's are
// ADPCM (decoded here), some at lower rates, and a few tones' priorities, volumes and centres
// differ. Comparing with the real PS1 needs the same data on both sides.
//
// <dir> is searched for *.LVL files at any depth: AO's disc, or either (or both) of AE's.
class Ps1SoundData final
{
public:
    explicit Ps1SoundData(FileSystem& fs);

    // Indexes the sound files (VH, VB and BSQ) of every LVL below dir. False if there are none.
    bool Open(const std::string& dir);

    // True if the data has this VH, VB or BSQ (names as in the PC data, e.g. MINES.VH)
    bool Has(const std::string& fileName) const;

    // The file's bytes, empty if it isn't there
    std::vector<u8> ReadFile(const std::string& fileName) const;

    // A SEQ named as the PC data's SEQ files are (e.g. MINESAMB.SEQ): the BSQ chunk with its id,
    // from the BSQ of the LVL that holds vhFile (a few SEQ ids are in several levels' BSQs with
    // different SEQs). Empty if there is none.
    std::vector<u8> ReadSeq(const std::string& seqName, const std::string& vhFile) const;

    // One VAG decoded from SPU ADPCM
    struct Sample final
    {
        std::vector<s16> mPcm;
        bool mLoop = false;
        u32 mLoopStart = 0; // In samples
    };

    // Decodes a VAB body. vh is the VAB header, which ends with the table of VAG sizes (in 8 byte
    // units, entry 0 unused). Returns the VAGs in order: index i is VAG number i + 1, the number
    // the tones use.
    static std::vector<Sample> DecodeVab(const std::vector<u8>& vh, const std::vector<u8>& vb);

private:
    struct Entry final
    {
        std::string mLvlPath;
        std::string mLvlName; // The LVL's file name, upper case
        std::string mFileName; // Inside the LVL
    };

    // Upper case, for the case-insensitive lookups
    static std::string Upper(const std::string& name);

    FileSystem& mFs;
    std::multimap<std::string, Entry> mFiles; // By upper case file name
};
