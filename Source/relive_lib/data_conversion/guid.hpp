#pragma once

#include "../Types.hpp"
#include <string>

class Guid final
{
public:
    struct TlvOffsetLevelIdPathId final
    {
        u16 tlvOffset;
        u8 levelId;
        u8 pathId;
    };

    static Guid NewGuidFromTlvInfo(u32 tlvInfo);
    static Guid NewGuid();

    // The id of the object a TLV makes, and of the ones its constructor makes, are worked out from the TLV's id
    // instead of the counter, so making them again (a camera loading after a quicksave restore) gives the same
    // ids and uses none of the counter. While a scope is open the first NewGuid() is firstId, and the others
    // come from firstId and how many have been asked for.
    class CreationScope;

    // Whether NewGuid gave it out of the counter (not a TLV's or a scope's) at or after that counter value
    bool IsFromCounterAtOrAfter(u32 counterValue) const;

    // The counter value this id was given out at, only for ids IsFromCounterAtOrAfter(0) is true for
    u32 CounterValue() const;

    // Whether it is the id of the first object a TLV made, see FromTlvId
    bool IsFirstOfTlv() const;

    // The id for the first object a TLV makes
    static Guid FromTlvId(const Guid& tlvId);
    // NewGuid hands out a counter, which is world state: a quicksave keeps it
    static u32 GetNextGuidCounter();
    static void SetNextGuidCounter(u32 value);
    static Guid FromString(const std::string& str);
    std::string ToString() const;
    bool IsValid() const;
    TlvOffsetLevelIdPathId GetTlvInfo() const;
    bool operator<(const Guid& rhs) const;
    bool operator==(const Guid& rhs) const;
    bool operator!=(const Guid& rhs) const;

private:

    struct GuidInternal final
    {
        // 128 bit globally unique number (provided it was generated via the correct API)
        union TlvInfoOrGuid
        {
            // When converted from an OG lvl we keep the original TLV info in the first 32 bits
            // as this allows us to convert saves as they containing TLV info records
            TlvOffsetLevelIdPathId mTlvInfo;
            u32 mData;
        };
        TlvInfoOrGuid mTlvInfoOrData1; // first 32 bits
        u32 mData2[3]; // remaining 96 bits
    };
    GuidInternal mGuid = {};

};

class Guid::CreationScope final
{
public:
    explicit CreationScope(const Guid& firstId);
    ~CreationScope();
    CreationScope(const CreationScope&) = delete;
    CreationScope& operator=(const CreationScope&) = delete;

private:
    bool mPreviousActive = false;
    Guid mPreviousFirst;
    bool mPreviousFirstUsed = false;
    u32 mPreviousChildren = 0;
};

