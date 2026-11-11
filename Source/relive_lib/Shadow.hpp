#pragma once

#include "Animation.hpp"

enum class Scale : s16;
class BaseMap;

class BaseGameObject;

class Shadow final
{
public:
    // owner: whose shadow it is (the animation list is put back in the order of a save by who owns what)
    Shadow(ResourceManager& resMan, BaseMap& map, BaseGameObject* owner);
    ~Shadow();

    // ownerLayer is the layer the owner was drawn on, just before its shadow
    void Calculate_Position(FP xpos, FP ypos, PSX_RECT* frameRect, FP spriteScale, Scale scale, Layer ownerLayer);
    void Render(OrderingTable& ot);

private:
    s16 mX1 = 0;
    s16 mY1 = 0;
    s16 mX2 = 0;
    s16 mY2 = 0;
    FP mXPos = {};
    FP mYPos = {};
    FP mSpriteScale = {};

public:
    bool mShadowAtBottom = false;
    bool mEnabled = false;
    Animation mAnim = {};
};
