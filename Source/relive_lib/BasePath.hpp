#pragma once

#include "BinaryPath.hpp"

class BaseMap;
enum class ReliveTypes : s16;

namespace relive
{
    class Factory;
}

// Common base of AliveLibAE::Path and AliveLibAO::Path. Both engines walk TLV
// lists identically here - they only differ in how the underlying path
// resource data (mPathData/mBinaryPath) is loaded and laid out, which stays
// in the derived, engine-specific Path classes.
class BasePath
{
public:
    explicit BasePath(BaseMap& map, relive::Factory& factory)
        : mMap(map)
        , mFactory(factory)
    {

    }

    TlvIterator Get_First_TLV_For_Offsetted_Camera(s16 cam_x_idx, s16 cam_y_idx);
    TlvIterator TLV_First_Of_Type_In_Camera(ReliveTypes objectType, s16 camX);

    static TlvIterator TLV_Next_Of_Type(TlvIterator tlvIterator, ReliveTypes type);

    BaseMap& mMap;
    relive::Factory& mFactory;
};
