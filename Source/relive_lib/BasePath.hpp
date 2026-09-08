#pragma once

#include "BinaryPath.hpp"

class BaseMap;
class Guid;
struct FixedPoint;
using FP = FixedPoint;
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

    virtual ~BasePath()
    {

    }

    // Engine specific - the two games lay out their path grids differently.
    virtual TlvIterator VTLV_Get_At_Of_Type(s16 xpos, s16 ypos, s16 width, s16 height, ReliveTypes typeToFind) = 0;
    virtual TlvIterator TLV_Get_At(TlvIterator tlvIterator, FP xpos, FP ypos, FP width, FP height) = 0;

    TlvIterator Get_First_TLV_For_Offsetted_Camera(s16 cam_x_idx, s16 cam_y_idx);
    TlvIterator TLV_First_Of_Type_In_Camera(ReliveTypes objectType, s16 camX);

    static TlvIterator TLV_Next_Of_Type(TlvIterator tlvIterator, ReliveTypes type);

    // Derived adds clearing of its own engine specific mPathData.
    virtual void Free();

    void TLV_Reset(const Guid& tlvId, s16 hiFlags = -1);
    void TLV_Persist(const Guid& tlvId, s16 hiFlags = -1);
    void TLV_Delete(const Guid& tlvId, s16 hiFlags = -1);
    void Set_TLVData(const Guid& tlvId, s16 hiFlags, s8 bSetCreated, s8 bSetDestroyed);

    BaseMap& mMap;
    relive::Factory& mFactory;

    EReliveLevelIds mLevelId = EReliveLevelIds::eNone;
    u16 mPathId = 0;
    s16 mCameraId = 0;
    BinaryPath* mBinaryPath = nullptr; // Non owning ptr
};
