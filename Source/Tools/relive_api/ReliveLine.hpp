#pragma once

#include "JsonModelTypes.hpp"
#include "PropertyCollection.hpp"
#include "../../relive_lib/Collisions.hpp"
#include "TlvObjectBaseMacros.hpp"

namespace ReliveAPI {
class ReliveLine final : public PropertyCollection
{
public:
    ReliveLine(TypesCollectionBase& globalTypes, const ::PathLine* line = nullptr)
    {
        if (line)
        {
            mLine = *line;
        }

        ADD("x1", mLine.mRect.x);
        ADD("y1", mLine.mRect.y);

        ADD("x2", mLine.mRect.w);
        ADD("y2", mLine.mRect.h);

        ADD("Type", mLine.mLineType);

        ADD("Next", mLine.mNext);
        ADD("Previous", mLine.mPrevious);

        ADD("Length", mLine.mLineLength);
    }

    PathLine mLine = {};
};
} // namespace ReliveAPI
