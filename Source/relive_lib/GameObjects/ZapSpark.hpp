#pragma once

#include "BaseAnimatedWithPhysicsGameObject.hpp"
#include "../SaveStateBase.hpp"

struct ZapSparkSaveState final : public SaveStateBase
{
    ZapSparkSaveState()
        : SaveStateBase(ReliveTypes::eZapSpark, sizeof(*this))
    { }
    AnimatedObjectSaveState mObject;
    s16 mSparkTimer;
};

class ZapSpark final : public BaseAnimatedWithPhysicsGameObject
{
public:
    ZapSpark(FP xpos, FP ypos, FP scale, ResourceManager& resMan, BaseMap& map);
    
    void LoadAnimationsAO();
    void LoadAnimationsAE();

    virtual void VUpdate() override;
    virtual void VScreenChanged() override;
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pData, ResourceManager& resMan, BaseMap& map);

private:
    s16 mSparkTimer = 0;
};
