#pragma once

class IGridPointSnapper
{
public:
    virtual ~IGridPointSnapper() = default;
    virtual int SnapX(bool enabled, int x) = 0;
    virtual int SnapY(bool enabled, int y) = 0;

};
