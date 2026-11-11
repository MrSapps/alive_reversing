#include "stdafx.h"
#include "guid.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

static u32 nextPesudoRandomGuidValue = 1;

Guid Guid::NewGuidFromTlvInfo(u32 tlvInfo)
{
    Guid r;
    r.mGuid.mTlvInfoOrData1.mData = tlvInfo;
    return r;
}

// The scope Guid::CreationScope has open, if any
static bool sScopeActive = false;
static Guid sScopeFirst;
static bool sScopeFirstUsed = false;
static u32 sScopeChildren = 0;

// A 128 bit id worked out from another and a number. Marked in the first 32 bits so it can't be a TLV info, and
// differs from the counter's marker.
static Guid DerivedGuid(const Guid& from, u32 number, Guid (*make)(u32, u32, u32))
{
    u64 state = 0xcbf29ce484222325ull;
    auto mix = [&state](u32 value)
    {
        for (u32 i = 0; i < 4; i++)
        {
            state = (state ^ ((value >> (i * 8)) & 0xff)) * 0x100000001b3ull;
        }
    };
    const std::string text = from.ToString();
    for (const char c : text)
    {
        state = (state ^ static_cast<u8>(c)) * 0x100000001b3ull;
    }
    mix(number);
    const u32 a = static_cast<u32>(state);
    const u32 b = static_cast<u32>(state >> 32);
    mix(0x9e3779b9u);
    return make(a, b, static_cast<u32>(state));
}

bool Guid::IsFirstOfTlv() const
{
    return mGuid.mTlvInfoOrData1.mData == 0xFFFFFFFE;
}

bool Guid::IsFromCounterAtOrAfter(u32 counterValue) const
{
    return mGuid.mTlvInfoOrData1.mData == 0xFFFFFFFF && mGuid.mData2[0] >= counterValue;
}

u32 Guid::CounterValue() const
{
    return mGuid.mData2[0];
}

Guid Guid::FromTlvId(const Guid& tlvId)
{
    return DerivedGuid(tlvId, 0, [](u32 a, u32 b, u32 c)
    {
        Guid r;
        r.mGuid.mTlvInfoOrData1.mData = 0xFFFFFFFE;
        r.mGuid.mData2[0] = a;
        r.mGuid.mData2[1] = b;
        r.mGuid.mData2[2] = c;
        return r;
    });
}

Guid::CreationScope::CreationScope(const Guid& firstId)
    : mPreviousActive(sScopeActive)
    , mPreviousFirst(sScopeFirst)
    , mPreviousFirstUsed(sScopeFirstUsed)
    , mPreviousChildren(sScopeChildren)
{
    sScopeActive = true;
    sScopeFirst = firstId;
    sScopeFirstUsed = false;
    sScopeChildren = 0;
}

Guid::CreationScope::~CreationScope()
{
    sScopeActive = mPreviousActive;
    sScopeFirst = mPreviousFirst;
    sScopeFirstUsed = mPreviousFirstUsed;
    sScopeChildren = mPreviousChildren;
}

u32 Guid::GetNextGuidCounter()
{
    return nextPesudoRandomGuidValue;
}

void Guid::SetNextGuidCounter(u32 value)
{
    nextPesudoRandomGuidValue = value;
}

Guid Guid::NewGuid()
{
    if (sScopeActive)
    {
        if (!sScopeFirstUsed)
        {
            sScopeFirstUsed = true;
            return sScopeFirst;
        }

        sScopeChildren++;
        return DerivedGuid(sScopeFirst, sScopeChildren, [](u32 a, u32 b, u32 c)
        {
            Guid r;
            r.mGuid.mTlvInfoOrData1.mData = 0xFFFFFFFD;
            r.mGuid.mData2[0] = a;
            r.mGuid.mData2[1] = b;
            r.mGuid.mData2[2] = c;
            return r;
        });
    }

    Guid r;

    r.mGuid.mTlvInfoOrData1.mData = 0xFFFFFFFF; // A TLVInfo can never be this, and it will never set data2 fields
    for (u32 i = 0; i < 3; i++)
    {
        r.mGuid.mData2[i] = nextPesudoRandomGuidValue++; // 4 billion * 4 values
    }

    return r;
}

Guid Guid::FromString(const std::string& str)
{
    Guid r;
    if (str.length() > 2 && str[0] == '{' && str[str.length() - 1] == '}')
    {
        // Iterate the data between the braces
        const char* pStart = &str[1];
        char* pEnd = nullptr;
        r.mGuid.mTlvInfoOrData1.mData = ::strtoul(pStart, &pEnd, 16);
        if (*pEnd != '-')
        {
            return {};
        }
        pStart = pEnd + 1;

        for (u32 i = 0; i < 3; i++)
        {
            r.mGuid.mData2[i] = ::strtoul(pStart, &pEnd, 16);
            if (*pEnd == '}')
            {
                return r;
            }

            if (*pEnd != '-')
            {
                return {};
            }
            pStart = pEnd + 1;
        }
    }

    return r;
}

std::string Guid::ToString() const
{
    char buffer[128] = {};
    ::sprintf(buffer, "{%08X-%08X-%08X-%08X}", mGuid.mTlvInfoOrData1.mData, mGuid.mData2[0], mGuid.mData2[1], mGuid.mData2[2]);
    return buffer;
}

bool Guid::IsValid() const
{
    // A "not defaut constructed" check (any components non zero)
    return mGuid.mTlvInfoOrData1.mData != 0 || mGuid.mData2[0] != 0 || mGuid.mData2[1] != 0 || mGuid.mData2[2];
}

Guid::TlvOffsetLevelIdPathId Guid::GetTlvInfo() const
{
    return mGuid.mTlvInfoOrData1.mTlvInfo;
}

bool Guid::operator < (const Guid& rhs) const
{
    return std::tie(mGuid.mTlvInfoOrData1.mData, mGuid.mData2[0], mGuid.mData2[1], mGuid.mData2[2]) < std::tie(rhs.mGuid.mTlvInfoOrData1.mData, rhs.mGuid.mData2[0], rhs.mGuid.mData2[1], rhs.mGuid.mData2[2]);
}

bool Guid::operator==(const Guid& rhs) const
{
    if (this != &rhs)
    {
        return mGuid.mTlvInfoOrData1.mData == rhs.mGuid.mTlvInfoOrData1.mData && 
            mGuid.mData2[0] == rhs.mGuid.mData2[0] &&
            mGuid.mData2[1] == rhs.mGuid.mData2[1] && 
            mGuid.mData2[2] == rhs.mGuid.mData2[2];
    }
    return true;
}

bool Guid::operator!=(const Guid& rhs) const
{
    return !(*this == rhs);
}
