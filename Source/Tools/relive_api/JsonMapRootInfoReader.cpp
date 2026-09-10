#include "JsonMapRootInfoReader.hpp"
#include "relive_api_exceptions.hpp"
#include "JsonMapRootInfoReader.hpp"
#include <nlohmann/json.hpp>
#include "JsonReadUtils.hpp"
#include "../../relive_lib/data_conversion/file_system.hpp"

namespace ReliveAPI {
void JsonMapRootInfoReader::Read(FileSystem& fs, const std::string& fileName)
{
    AutoFILE inputFileStream = fs.OpenFile(fileName.c_str(), "r");
    if (!inputFileStream.GetFile())
    {
        throw ReliveAPI::IOReadException(fileName.c_str());
    }

    std::string& jsonStr = getStaticStringBuffer();
    inputFileStream.ReadInto(jsonStr);

    nlohmann::json rootObj = nlohmann::json::parse(jsonStr);
    if (rootObj.is_discarded())
    {
        throw ReliveAPI::InvalidJsonException();
    }

    mMapRootInfo.mVersion = ReadNumber(rootObj, "api_version");
    mMapRootInfo.mGame = ReadString(rootObj, "game");

    if (mMapRootInfo.mGame == "AO")
    {
        return;
    }

    if (mMapRootInfo.mGame == "AE")
    {
        return;
    }

    /*
    if (mMapRootInfo.mVersion != AliveAPI::GetApiVersion())
    {
        // TODO: auto upgrade
    }*/


    throw ReliveAPI::InvalidGameException(mMapRootInfo.mGame);
}

std::string& getStaticStringBuffer()
{
    static std::string result;
    return result;
}
} // namespace ReliveAPI
