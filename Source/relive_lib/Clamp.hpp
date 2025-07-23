#pragma once

#include "Types.hpp"
#include <limits>

template<class T> T Clamp(T val, T lowerBound, T upperBound)
{
    return val > upperBound ? upperBound : val < lowerBound ? lowerBound : val;
}

template<class T> T ClampedAdd(T val, T operand)
{
    if (operand == 0)
    {
        return val;
    }

    if (std::numeric_limits<T>().max() - operand < val)
    {
        return std::numeric_limits<T>().max();
    }

    return val + operand;
}

template<class T> T ClampedSub(T val, T operand)
{
    if (operand == 0)
    {
        return val;
    }

    if (std::numeric_limits<T>().min() + operand > val)
    {
        return std::numeric_limits<T>().min();
    }

    return val - operand;
}

template<class T> T ClampedAdd(T val, T operand, T lowerBound, T upperBound)
{
    return Clamp(ClampedAdd(val, operand), lowerBound, upperBound);
}
