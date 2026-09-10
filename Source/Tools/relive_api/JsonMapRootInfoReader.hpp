#pragma once

#include "JsonModelTypes.hpp"

class FileSystem;

namespace ReliveAPI {

    // Reads the root fields to read the version/game type (we need to know this so we can create a game specific reader/do an upgrade of the json).
class JsonMapRootInfoReader final
{
public:
    void Read(FileSystem& fs, const std::string& fileName);
    MapRootInfo mMapRootInfo;
};

std::string& getStaticStringBuffer();
} // namespace ReliveAPI
