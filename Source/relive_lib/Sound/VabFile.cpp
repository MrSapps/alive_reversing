#include "VabFile.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>

// A tone whose priority and volume the PC data changed: the PC values (and the tone's keys and
// sample, to be sure it's the same tone), then the PS1 ones. Made by comparing every VH of the PS1
// and PC versions; AE's are all the same.
struct Ps1ToneValues final
{
    GameType mGame;
    const char_type* mVhFile;
    u8 mProgram;
    u8 mTone; // Slot 0-15 in the program
    u8 mMinKey;
    u8 mMaxKey;
    u8 mVag;
    u8 mPcPriority;
    u8 mPcVol;
    u8 mPs1Priority;
    u8 mPs1Vol;
};

static const Ps1ToneValues kPs1ToneValues[] = {
    {GameType::eAo, "D1SNDFX.VH", 81, 0, 0, 127, 41, 70, 70, 127, 127},
    {GameType::eAo, "D1SNDFX.VH", 82, 0, 0, 126, 69, 95, 95, 127, 127},
    {GameType::eAo, "D1SNDFX.VH", 82, 1, 0, 126, 69, 95, 95, 91, 127},
    {GameType::eAo, "D1SNDFX.VH", 85, 0, 0, 127, 69, 95, 95, 101, 127},
    {GameType::eAo, "D2ENDER.VH", 81, 0, 0, 127, 18, 70, 70, 127, 127},
    {GameType::eAo, "D2ENDER.VH", 82, 0, 0, 127, 42, 95, 95, 101, 127},
    {GameType::eAo, "D2ENDER.VH", 85, 0, 0, 127, 42, 95, 95, 101, 127},
    {GameType::eAo, "D2SNDFX.VH", 81, 0, 0, 127, 51, 70, 70, 127, 127},
    {GameType::eAo, "D2SNDFX.VH", 82, 0, 0, 127, 85, 95, 95, 101, 127},
    {GameType::eAo, "D2SNDFX.VH", 85, 0, 0, 127, 85, 95, 95, 101, 127},
    {GameType::eAo, "E1SNDFX.VH", 74, 0, 0, 127, 51, 100, 100, 0, 127},
    {GameType::eAo, "E1SNDFX.VH", 76, 0, 0, 127, 51, 100, 100, 79, 127},
    {GameType::eAo, "E1SNDFX.VH", 81, 0, 0, 127, 51, 70, 70, 127, 127},
    {GameType::eAo, "E2SNDFX.VH", 76, 0, 0, 127, 52, 100, 100, 79, 127},
    {GameType::eAo, "E2SNDFX.VH", 81, 0, 0, 127, 52, 70, 70, 127, 127},
    {GameType::eAo, "F1SNDFX.VH", 81, 0, 0, 127, 92, 70, 70, 127, 127},
    {GameType::eAo, "F2ENDER.VH", 81, 0, 0, 127, 27, 70, 70, 127, 127},
    {GameType::eAo, "F2SNDFX.VH", 7, 3, 36, 43, 6, 40, 40, 2, 127},
    {GameType::eAo, "F2SNDFX.VH", 7, 4, 44, 45, 10, 40, 40, 2, 127},
    {GameType::eAo, "F2SNDFX.VH", 7, 5, 46, 46, 11, 40, 40, 2, 127},
    {GameType::eAo, "F2SNDFX.VH", 7, 6, 47, 47, 26, 60, 60, 2, 127},
    {GameType::eAo, "F2SNDFX.VH", 7, 7, 48, 48, 27, 60, 60, 2, 127},
    {GameType::eAo, "F2SNDFX.VH", 7, 8, 49, 55, 40, 60, 60, 2, 127},
    {GameType::eAo, "F2SNDFX.VH", 7, 9, 56, 57, 49, 40, 40, 2, 127},
    {GameType::eAo, "F2SNDFX.VH", 7, 10, 65, 67, 53, 40, 40, 2, 127},
    {GameType::eAo, "F2SNDFX.VH", 7, 12, 69, 75, 59, 40, 40, 2, 127},
    {GameType::eAo, "F2SNDFX.VH", 81, 0, 0, 127, 46, 70, 70, 127, 127},
    {GameType::eAo, "MLSNDFX.VH", 81, 0, 0, 127, 130, 70, 70, 127, 127},
    {GameType::eAo, "OPTSNDFX.VH", 2, 0, 0, 127, 9, 70, 70, 79, 127},
    {GameType::eAo, "RFENDER.VH", 49, 1, 61, 61, 67, 127, 127, 64, 127},
    {GameType::eAo, "RFENDER.VH", 67, 0, 60, 60, 103, 127, 127, 127, 100},
    {GameType::eAo, "RFENDER.VH", 67, 1, 61, 61, 104, 127, 127, 127, 100},
    {GameType::eAo, "RFENDER.VH", 67, 2, 62, 62, 105, 127, 127, 127, 100},
    {GameType::eAo, "RFENDER.VH", 67, 3, 63, 63, 106, 127, 127, 127, 100},
    {GameType::eAo, "RFENDER.VH", 81, 0, 0, 127, 36, 70, 70, 127, 127},
    {GameType::eAo, "RFSNDFX.VH", 7, 1, 36, 48, 95, 60, 60, 2, 127},
    {GameType::eAo, "RFSNDFX.VH", 7, 2, 35, 35, 1, 40, 40, 2, 127},
    {GameType::eAo, "RFSNDFX.VH", 7, 3, 34, 34, 2, 40, 40, 2, 127},
    {GameType::eAo, "RFSNDFX.VH", 7, 4, 33, 33, 3, 40, 40, 2, 127},
    {GameType::eAo, "RFSNDFX.VH", 7, 5, 32, 32, 7, 40, 40, 2, 127},
    {GameType::eAo, "RFSNDFX.VH", 7, 6, 31, 31, 9, 40, 40, 2, 127},
    {GameType::eAo, "RFSNDFX.VH", 7, 7, 30, 30, 12, 60, 60, 2, 127},
    {GameType::eAo, "RFSNDFX.VH", 7, 8, 49, 51, 16, 40, 40, 2, 127},
    {GameType::eAo, "RFSNDFX.VH", 7, 9, 52, 63, 17, 40, 40, 2, 127},
    {GameType::eAo, "RFSNDFX.VH", 49, 1, 61, 61, 97, 127, 127, 64, 127},
    {GameType::eAo, "RFSNDFX.VH", 81, 0, 0, 127, 44, 70, 70, 127, 127},
};

static bool CaseInsensitiveEquals(const std::string& a, const char_type* pB)
{
    const std::string b = pB;
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y)
    {
        return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
    });
}

void VabFile::RestorePs1Tones(GameType game, const std::string& vhFile, std::vector<u8>& vhData)
{
    if (vhData.size() < sizeof(VabHeader))
    {
        return;
    }

    VabHeader header = {};
    memcpy(&header, vhData.data(), sizeof(VabHeader));
    const size_t numTones = static_cast<size_t>(std::clamp<s32>(header.field_12_num_progs, 0, 128)) * 16;
    if (vhData.size() < sizeof(VabHeader) + numTones * sizeof(VagAtr))
    {
        return;
    }

    for (const Ps1ToneValues& entry : kPs1ToneValues)
    {
        if (entry.mGame != game || !CaseInsensitiveEquals(vhFile, entry.mVhFile))
        {
            continue;
        }

        for (size_t i = 0; i < numTones; i++)
        {
            VagAtr tone = {};
            u8* pTone = vhData.data() + sizeof(VabHeader) + i * sizeof(VagAtr);
            memcpy(&tone, pTone, sizeof(VagAtr));
            if (tone.field_14_prog == entry.mProgram && i % 16 == entry.mTone && static_cast<u8>(tone.field_6_min) == entry.mMinKey
                && static_cast<u8>(tone.field_7_max) == entry.mMaxKey && (tone.field_16_vag & 0xFF) == entry.mVag
                && static_cast<u8>(tone.field_0_priority) == entry.mPcPriority && static_cast<u8>(tone.field_2_vol) == entry.mPcVol)
            {
                tone.field_0_priority = static_cast<s8>(entry.mPs1Priority);
                tone.field_2_vol = static_cast<s8>(entry.mPs1Vol);
                memcpy(pTone, &tone, sizeof(VagAtr));
            }
        }
    }
}

