#pragma once

#include <memory>
#include "../../relive_lib/Collisions.hpp"

class CollisionObject final
{
public:
    explicit CollisionObject(int id)
        : mId(id)
    { }

    CollisionObject(const CollisionObject&) = delete;

    // For when copied via the clipboard, id is always 0 in this case
    // I guess because its set later on the "paste" (?)
    CollisionObject(int id, const CollisionObject& rhs);

    int X1() const
    {
        return mLine.mRect.x;
    }

    void SetX1(int x1)
    {
        mLine.mRect.x = x1;
    }

    int Y1() const
    {
        return mLine.mRect.y;
    }

    void SetY1(int y1)
    {
        mLine.mRect.y = y1;
    }

    int X2() const
    {
        return mLine.mRect.w;
    }

    void SetX2(int x2)
    {
        mLine.mRect.w = x2;
    }

    int Y2() const
    {
        return mLine.mRect.h;
    }

    void SetY2(int y2)
    {
        mLine.mRect.h = y2;
    }

    void SetNext(int next)
    {
        mLine.mNext = next;
    }

    int Next() const
    {
        return mLine.mNext;
    }

    int Previous() const
    {
        return mLine.mPrevious;
    }

    void SetPrevious(int previous)
    {
        mLine.mPrevious = previous;
    }

    void CalculateLength()
    {
        mLine.mLineLength = abs(X2() - X1() + Y2() - Y1());
    }

    // The previous/next in the collision data is the index of the next/previous line. Removing or adding line
    // will cause this to break so we remap to a generated Id that then gets normalized back to indicies on save
    // by looking up the index of the line with the given Id.
    int mId = 0;

    PathLine mLine = {};
};
using UP_CollisionObject = std::unique_ptr<CollisionObject>;
