#pragma once

#include "../../relive_lib/Types.hpp"
#include <map>
#include <utility>

// Keeps the renderer's textures alive for a while after they were last used, keyed by the
// resource's unique id. A texture's lifetime is the number of DecreaseResourceLifetimes calls
// (one a frame) it survives without being used again.
template <typename TextureType>
class TextureCache final
{
public:
    ~TextureCache()
    {
        Clear();
    }

    void Clear()
    {
        mTextureCache.clear();
    }

    u32 Size() const
    {
        return static_cast<u32>(mTextureCache.size());
    }

    TextureType Add(u32 uniqueId, u32 lifetime, TextureType texture)
    {
        CachedTexture& cached = mTextureCache[uniqueId];
        cached.mTexture = std::move(texture);
        cached.mLifetime = lifetime;
        return cached.mTexture;
    }

    // The cached texture, or nullptr. A bump above 0 resets its lifetime to bump.
    TextureType GetCachedTexture(u32 uniqueId, s32 bump)
    {
        auto it = mTextureCache.find(uniqueId);

        if (it == mTextureCache.end())
        {
            return nullptr;
        }

        if (bump > 0)
        {
            // Bump!
            it->second.mLifetime = bump;
        }

        return it->second.mTexture;
    }

    // The cached texture with its lifetime reset, or else the one create() makes, which is added
    template <typename CreateFn>
    TextureType GetOrAdd(u32 uniqueId, s32 lifetime, CreateFn&& create)
    {
        TextureType texture = GetCachedTexture(uniqueId, lifetime);
        if (!texture)
        {
            texture = Add(uniqueId, lifetime, create());
        }
        return texture;
    }

    void DecreaseResourceLifetimes()
    {
        // Check texture lifetimes
        auto it = mTextureCache.begin();
        while (it != mTextureCache.end())
        {
            if (it->second.mLifetime-- <= 0)
            {
                it = mTextureCache.erase(it);
            }
            else
            {
                it++;
            }
        }
    }

private:
    struct CachedTexture final
    {
        TextureType mTexture = {};
        s32 mLifetime = 0;
    };
    std::map<u32, CachedTexture> mTextureCache;
};
