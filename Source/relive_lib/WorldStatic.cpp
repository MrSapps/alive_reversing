#include "stdafx.h"
#include "WorldStatic.hpp"
#include "FatalError.hpp"
#include "logger.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

struct WorldStaticEntry final
{
    std::string mName;
    void* mData;
    std::size_t mSize;
};

// Made when first used, since the variables register as the program starts
static std::vector<WorldStaticEntry>& Entries()
{
    static std::vector<WorldStaticEntry> sEntries;
    return sEntries;
}

static u32 HashName(const std::string& name)
{
    u32 hash = 2166136261u;
    for (const char c : name)
    {
        hash = (hash ^ static_cast<u8>(c)) * 16777619u;
    }
    return hash;
}

void WorldStatics::Register(const char* name, void* pData, std::size_t size)
{
    if (size > WorldStaticsSaveState::kMaxSize)
    {
        ALIVE_FATAL("World static %s is %zu bytes, a save has room for %u", name, size, WorldStaticsSaveState::kMaxSize);
    }
    for (const WorldStaticEntry& entry : Entries())
    {
        if (entry.mName == name || HashName(entry.mName) == HashName(name))
        {
            ALIVE_FATAL("Two world statics are called %s", name);
        }
    }
    if (Entries().size() >= WorldStaticsSaveState::kMaxStatics)
    {
        ALIVE_FATAL("A save has room for %u world statics", WorldStaticsSaveState::kMaxStatics);
    }
    Entries().push_back({name, pData, size});
}

WorldStaticsSaveState WorldStatics::GetSaveState()
{
    WorldStaticsSaveState state = {};
    state.mCount = static_cast<u32>(Entries().size());
    for (u32 i = 0; i < state.mCount; i++)
    {
        const WorldStaticEntry& entry = Entries()[i];
        state.mStatics[i].mNameHash = HashName(entry.mName);
        state.mStatics[i].mSize = static_cast<u32>(entry.mSize);
        std::memcpy(state.mStatics[i].mData, entry.mData, entry.mSize);
    }
    return state;
}

void WorldStatics::RestoreFromSaveState(const WorldStaticsSaveState& state)
{
    for (u32 i = 0; i < state.mCount && i < WorldStaticsSaveState::kMaxStatics; i++)
    {
        for (const WorldStaticEntry& entry : Entries())
        {
            if (HashName(entry.mName) == state.mStatics[i].mNameHash && entry.mSize == state.mStatics[i].mSize)
            {
                std::memcpy(entry.mData, state.mStatics[i].mData, entry.mSize);
                break;
            }
        }
    }
}

std::vector<u8> WorldStatics::Snapshot()
{
    std::vector<u8> snapshot;
    for (const WorldStaticEntry& entry : Entries())
    {
        const u8* p = static_cast<const u8*>(entry.mData);
        snapshot.insert(snapshot.end(), p, p + entry.mSize);
    }
    return snapshot;
}

void WorldStatics::RestoreSnapshot(const std::vector<u8>& snapshot)
{
    std::size_t pos = 0;
    for (const WorldStaticEntry& entry : Entries())
    {
        if (pos + entry.mSize > snapshot.size())
        {
            return;
        }
        std::memcpy(entry.mData, snapshot.data() + pos, entry.mSize);
        pos += entry.mSize;
    }
}

std::string WorldStatics::Describe()
{
    std::vector<const WorldStaticEntry*> sorted;
    for (const WorldStaticEntry& entry : Entries())
    {
        sorted.push_back(&entry);
    }
    std::sort(sorted.begin(), sorted.end(), [](const WorldStaticEntry* pLeft, const WorldStaticEntry* pRight) { return pLeft->mName < pRight->mName; });

    std::string text;
    for (const WorldStaticEntry* pEntry : sorted)
    {
        text += pEntry->mName + "=";
        const u8* p = static_cast<const u8*>(pEntry->mData);
        for (std::size_t i = 0; i < pEntry->mSize; i++)
        {
            char hex[4] = {};
            snprintf(hex, sizeof(hex), "%02x", p[i]);
            text += hex;
        }
        text += "\n";
    }
    return text;
}

void WorldStatics::LogChangedSince(const std::string& before, const char* what)
{
    const std::string now = Describe();
    std::size_t a = 0;
    std::size_t b = 0;
    while (a < before.size() && b < now.size())
    {
        const std::size_t endA = before.find('\n', a);
        const std::size_t endB = now.find('\n', b);
        const std::string left = before.substr(a, endA - a);
        const std::string right = now.substr(b, endB - b);
        if (left != right)
        {
            LOG_WARNING("%s: a restore's objects changed %s, put back to %s", what, left.c_str(), right.c_str());
        }
        a = endA + 1;
        b = endB + 1;
    }
}
