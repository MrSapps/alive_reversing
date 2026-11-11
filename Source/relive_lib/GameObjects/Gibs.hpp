#pragma once

#include "../relive_lib/GameObjects/BaseAnimatedWithPhysicsGameObject.hpp"
#include "../relive_lib/SaveStateBase.hpp"

struct GibPart final
{
    FP x;
    FP y;
    FP z;
    FP dx;
    FP dy;
    FP dz;
    Animation mAnimation;
};

enum class GibType : s32
{
    eAbe,
    eSlig,
    eSlog,
    eMud,
    eBlindMud,
    eMetal,
    eGlukkon,
    eAslik,
    eDripik,
    ePhleg,
    eFleech,
    eElum,
};

struct GibsSaveState final : public SaveStateBase
{
    GibsSaveState()
        : SaveStateBase(ReliveTypes::eGibs, sizeof(*this))
    { }
    // What it was made with
    s32 mGibType;
    FP mBaseScale;
    bool mMakeSmaller;
    AnimatedObjectSaveState mObject;
    FP mZ;
    FP mDz;
    s32 mAliveTimer;
    s16 mPartsUsedCount;
    struct Part final
    {
        FP mX;
        FP mY;
        FP mZ;
        FP mDx;
        FP mDy;
        FP mDz;
        AnimationSaveState mAnim;
    };
    Part mParts[7];
};

class Gibs final : public ::BaseAnimatedWithPhysicsGameObject
{
public:
    Gibs(GibType gibType, FP xpos, FP ypos, FP xOff, FP yOff, FP scale, bool bMakeSmaller, ResourceManager& resMan, BaseMap& map);
    ~Gibs();

    void LoadAnimations(AnimId head, AnimId arm, AnimId body);

    virtual void VUpdate() override;
    virtual void VRender(OrderingTable& ot) override;
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pData, ResourceManager& resMan, BaseMap& map);
    virtual std::vector<NotStateMemory> VNotStateMemory() override;

private:
    // What it was made with, to make it again
    GibType mGibType = GibType::eAbe;
    FP mBaseScale = {};
    FP mZ = {};
    FP mDz = {};
    s32 mAliveTimer = 0;
    GibPart mGibParts[7] = {};
    s16 mPartsUsedCount = 0;
    bool mMakeSmaller = false;
};
