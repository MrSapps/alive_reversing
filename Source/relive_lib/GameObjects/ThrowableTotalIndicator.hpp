#pragma once

#include "../relive_lib/GameObjects/BaseGameObject.hpp"
#include "../SaveStateBase.hpp"
#include "../WorldStatic.hpp"
#include "../relive_lib/Layer.hpp"
#include "../relive_lib/Primitives.hpp"
#include "../RGB16.hpp"

enum class ThrowableTotalIndicatorState : s16
{
    eCreated = 0,
    eFading = 1,
    eVanishing = 2
};

struct ThrowableTotalIndicatorSaveState final : public SaveStateBase
{
    ThrowableTotalIndicatorSaveState()
        : SaveStateBase(ReliveTypes::eThrowableTotalIndicator, sizeof(*this))
    { }
    FP mStartXPos;
    FP mStartYPos;
    FP mXPos;
    FP mYPos;
    FP mSpeedX;
    FP mSpeedY;
    FP mSpriteScale;
    s32 mOtLayer;
    s16 mRed;
    s16 mGreen;
    s16 mBlue;
    s32 mNumToShow;
    s32 mState;
    s16 mFade;
};

class ThrowableTotalIndicator final : public BaseGameObject
{
public:
    virtual std::vector<NotStateMemory> VNotStateMemory() override;
    // For a save: the indicator count is put back with the rest of the variables
    struct RestoreTag final
    { };
    ThrowableTotalIndicator(RestoreTag, ResourceManager& resMan, BaseMap& map);

    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pData, ResourceManager& resMan, BaseMap& map);

    ThrowableTotalIndicator(FP xpos, FP ypos, Layer layer, FP scale, s32 count, bool bFade, ResourceManager& resMan, BaseMap& map);
    ~ThrowableTotalIndicator();

    virtual void VScreenChanged() override;
    virtual void VUpdate() override;
    virtual void VRender(OrderingTable& ot) override;

private:
    FP mStartXPos = {};
    FP mStartYPos = {};
    FP mXPos = {};
    FP mYPos = {};
    FP mSpeedX = {};
    FP mSpeedY = {};
    FP mSpriteScale = {};
    Layer mOtLayer = Layer::eLayer_0;
    RGB16 mRGB;
    s32 mNumToShow = 0;

    Line_G2 mLines[8] = {};
    ThrowableTotalIndicatorState mState = ThrowableTotalIndicatorState::eCreated;
    s16 mFade = 0;
};

extern WorldStatic<u16> gThrowableIndicatorExists;
