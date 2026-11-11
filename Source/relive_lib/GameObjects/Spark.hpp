#pragma once

#include "../FixedPoint.hpp"
#include "BaseGameObject.hpp"
#include "../SaveStateBase.hpp"
#include "../Primitives.hpp"
#include "../Layer.hpp"

struct SparkRes final
{
    FP mX0;
    FP mY0;
    FP mX1;
    FP mY1;
    u8 mAng;
    // 3 byte pad
    FP mRadius;
    FP mLen;
    Line_G2 mLineG2;
};

enum class SparkType : s16
{
    eSmallChantParticle_0 = 0,
    eBigChantParticle_1 = 1
};

struct SparkSaveState final : public SaveStateBase
{
    SparkSaveState()
        : SaveStateBase(ReliveTypes::eSpark, sizeof(*this))
    { }
    FP mXPos;
    FP mYPos;
    FP mSpriteScale;
    s16 mRed;
    s16 mGreen;
    s16 mBlue;
    s32 mLayer;
    s32 mSparkCount;
    // mSparkCount goes down as it fades, the sparks it was made with don't
    s32 mSparkAllocCount;
    s32 mTimer;
    s32 mSparkType;
    struct Spark final
    {
        FP mX0;
        FP mY0;
        FP mX1;
        FP mY1;
        u8 mAng;
        FP mRadius;
        FP mLen;
    };
    Spark mSparks[16];
};

class Spark final : public BaseGameObject
{
public:
    Spark(FP xpos, FP ypos, FP scale, s32 count, s32 minAngle, s32 maxAngle, SparkType type, ResourceManager& resMan, BaseMap& map);
    ~Spark();

    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pData, ResourceManager& resMan, BaseMap& map);

    virtual void VUpdate() override;
    virtual void VRender(OrderingTable& ot) override;
    virtual void VScreenChanged() override;

private:
    // For a save: the object with nothing made yet, not the particle with it
    struct RestoreTag final
    { };
    Spark(RestoreTag, ResourceManager& resMan, BaseMap& map);

    FP mXPos = {};
    FP mYPos = {};
    FP mSpriteScale = {};
    s16 mRed = 0;
    s16 mGreen = 0;
    s16 mBlue = 0;
    Layer mLayer = Layer::eLayer_0;
    SparkRes* mSparkRes = nullptr;
    s32 mSparkCount = 0;
    // mSparkCount goes down as it fades, the array doesn't
    s32 mSparkAllocCount = 0;
    s32 mTimer = 0;
    SparkType mSparkType = SparkType::eSmallChantParticle_0;
};
