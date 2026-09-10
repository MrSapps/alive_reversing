#pragma once

#include "../../relive_lib/Types.hpp"
#include <string>
#include <vector>
#include <nlohmann/json_fwd.hpp>

namespace AO {
class PathLineAO;
enum class TlvTypes : s16;
} // namespace AO


class PathLineAE;
enum class TlvTypes : s16;

namespace ReliveAPI {
enum class Game;
class TypesCollectionBase;
class TypesCollectionAO;
class TypesCollectionAE;
class CameraImageAndLayers;

class CameraImageAndLayers final
{
public:
    std::string mCameraImage;
    std::string mForegroundLayer;
    std::string mBackgroundLayer;
    std::string mForegroundWellLayer;
    std::string mBackgroundWellLayer;

    bool HaveFG1Layers() const
    {
        return !mForegroundLayer.empty() || !mBackgroundLayer.empty() || !mForegroundWellLayer.empty() || !mBackgroundWellLayer.empty();
    }
};

struct MapRootInfo final
{
    s32 mVersion = 0;
    std::string mGame;
};

} // namespace ReliveAPI