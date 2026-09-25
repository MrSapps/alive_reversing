#pragma once

#include "../Types.hpp"
#include <string>
#include <vector>
#include <memory>
#include "FG1PngReader.hpp"

class ChunkedLvlFile;
class CameraImageAndLayers;
class FG1PngReader;

class CamConverter final
{
public:
    std::pair<std::unique_ptr<FG1PngReader>, u32> Convert(const ChunkedLvlFile& camFile, const std::string& baseName, bool isAo);

    CamConverter() = default;
    CamConverter(const ChunkedLvlFile& camFile, CameraImageAndLayers& outData);

    static u32 CamBitsIdFromName(const std::string& pCamName);
};

std::string RGB565ToBase64PngString(const u16* pRgb565Buffer);
void RGB565ToPngBuffer(const u16* camBuffer, std::vector<u8>& outPngData);

