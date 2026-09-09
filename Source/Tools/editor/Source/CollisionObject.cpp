#include "CollisionObject.hpp"

CollisionObject::CollisionObject(int id, const CollisionObject& rhs)
    : mId(id)
{
    mLine = rhs.mLine;
}
