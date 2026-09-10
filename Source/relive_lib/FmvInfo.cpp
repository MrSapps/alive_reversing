#include "stdafx.h"
#include "FmvInfo.hpp"

#include <algorithm>

namespace relive
{
std::vector<std::string> CollectUniqueFmvNames(std::initializer_list<FmvInfoArray> arrays)
{
    std::vector<std::string> names;

    for (const auto& arrayInfo : arrays)
    {
        for (size_t i = 0; i < arrayInfo.mCount; i++)
        {
            const char_type* pName = arrayInfo.mArray[i].mName;
            if (pName && pName[0] && std::find(names.begin(), names.end(), pName) == names.end())
            {
                names.emplace_back(pName);
            }
        }
    }

    return names;
}
} // namespace relive
