#include "SpriteFx.hpp"
#include <algorithm>

void SpriteFx::ComputeLuma(const std::vector<u8>& rgba, f32 (&luma)[kLumaPoints])
{
    std::vector<bool> seen(32768, false);
    std::vector<f32> lumas;
    for (std::size_t i = 0; i + 3 < rgba.size(); i += 4)
    {
        if (rgba[i + 3] < 128)
        {
            continue;
        }

        const u32 key = (rgba[i] >> 3) | ((rgba[i + 1] >> 3) << 5) | ((rgba[i + 2] >> 3) << 10);
        if (!seen[key])
        {
            seen[key] = true;
            lumas.push_back((rgba[i] * 0.299f + rgba[i + 1] * 0.587f + rgba[i + 2] * 0.114f) / 255.0f);
        }
    }

    if (lumas.empty())
    {
        return;
    }

    std::sort(lumas.begin(), lumas.end());
    for (u32 i = 0; i < kLumaPoints; i++)
    {
        const f32 position = static_cast<f32>(i) * (lumas.size() - 1) / (kLumaPoints - 1);
        luma[i] = lumas[static_cast<std::size_t>(position + 0.5f)];
    }
}
