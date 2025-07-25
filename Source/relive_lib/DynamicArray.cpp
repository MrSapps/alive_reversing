#include "stdafx.h"
#include "DynamicArray.hpp"
#include "FatalError.hpp"

DynamicArray::DynamicArray(s32 startingSize)
{
    if (startingSize == 0)
    {
        startingSize = 1;
    }

    mArray = new (std::nothrow) void*[startingSize]; // TODO: Needs to be strongly typed
    mUsedSize = 0;
    mMaxSize = 0;

    if (mArray)
    {
        mMaxSize = startingSize;
        mExpandSizeBy = 8;
    }
}

DynamicArray::~DynamicArray()
{
    delete[] mArray;
}

void DynamicArray::Push_Back(void* pValue)
{
    if (!mArray)
    {
        ALIVE_FATAL("DynamicArray can't be null!");
    }

    // If we have no more elements then expand the array
    if (mUsedSize == mMaxSize)
    {
        Expand(mExpandSizeBy);
    }

    mArray[mUsedSize++] = pValue;
}

void DynamicArray::Remove_Item(void* pItemToRemove)
{
    for (s32 idx = 0; idx < mUsedSize; idx++)
    {
        void* pCurrentItem = mArray[idx];
        if (!pCurrentItem)
        {
            break;
        }

        if (pCurrentItem == pItemToRemove)
        {
            RemoveAt(idx);
            break;
        }
    }
}

void DynamicArray::Expand(s32 expandSize)
{
    // Calculate new size and allocate buffer
    const s32 newSize = mMaxSize + expandSize;
    void** pNewBuffer = new (std::nothrow) void*[newSize];
    if (!pNewBuffer)
    {
        ALIVE_FATAL("Couldn't allocate buffer for DynamicArray!");
    }

    // Copy in the used elements into the new buffer
    memcpy(pNewBuffer, mArray, mUsedSize * sizeof(void*));

    // Free the old array buffer
    // OG BUG: Null checked here but not at the memcpy
    if (mArray)
    {
        delete[] mArray;
    }

    // Update max size and array pointer
    mMaxSize = newSize;
    mArray = pNewBuffer;
}
