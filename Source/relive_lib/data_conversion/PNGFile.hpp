#pragma once

#include "../Types.hpp"
#include <vector>
#include <string>

class FileSystem;

class PNGFile final
{
public:
    std::vector<u8> Encode(const u32* pixelData, u32 width, u32 height);
    void Decode(const std::string& fileName, const std::vector<u8>& pngData, std::vector<u8>& rawPixels, u32& width, u32& height);
    void Load(FileSystem& fs, const char_type* pFileName, std::vector<u8>& pixelData, u32& width, u32& height);

    // Sprite sheets are true colour: 8 bit RGBA, the alpha is how much of the pixel is there. Any
    // other kind of PNG is converted to that when it's loaded. Returns false if the file can't be
    // opened.
    bool LoadSprite(FileSystem& fs, const char_type* pFileName, std::vector<u8>& rgba, u32& width, u32& height);
    void SaveSprite(FileSystem& fs, const char_type* pFileName, const std::vector<u8>& rgba, u32 width, u32 height);

    // A sheet's optional mask is next to it, see SpriteMask.hpp: sheet.png has sheet_mask.png
    static std::string MaskPathFor(const std::string& sheetPath);
    // Loads the mask of the sheet at sheetPath, if there is one. The mask has to be the size of
    // the sheet. Returns false (and leaves mask empty) if there isn't one.
    bool LoadMask(FileSystem& fs, const std::string& sheetPath, std::vector<u8>& mask, u32 sheetWidth, u32 sheetHeight);

    u32 mOffset = 0;
    u8* mPtr = nullptr;
    std::vector<u8> mOutBuffer;
    bool mWrite = false;
};
