#pragma once

#include "BaseAnimatedWithPhysicsGameObject.hpp"
#include "../SaveStateBase.hpp"

class AnimationUnknown;

struct ParamiteWebSaveState final : public SaveStateBase
{
    ParamiteWebSaveState()
        : SaveStateBase(ReliveTypes::eParamiteWeb, sizeof(*this))
    { }
    s16 mSegmentsCount;
    s16 mSegmentLength;
    s32 mTtl;
    s32 mTtlRemainder;
    bool mEnabled;
};

class ParamiteWeb final : public BaseAnimatedWithPhysicsGameObject
{
public:
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pData, ResourceManager& resMan, BaseMap& map);

    ParamiteWeb(FP xpos, s32 bottom, s32 top, FP scale, ResourceManager& resMan, BaseMap& map);
    ~ParamiteWeb();

    virtual void VUpdate() override;
    virtual void VScreenChanged() override;
    virtual void VRender(OrderingTable& ot) override;
    virtual std::vector<NotStateMemory> VNotStateMemory() override;

public:
    s16 mSegmentsCount = 0;
    s16 mSegmentLength = 0;
    s32 mTtl = 0;
    s32 mTtlRemainder = 0;
    AnimationUnknown* mWebRes = nullptr;
    bool mEnabled = false;
};
