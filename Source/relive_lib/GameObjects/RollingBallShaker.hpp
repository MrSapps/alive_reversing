#pragma once

#include "BaseGameObject.hpp"
#include "../SaveStateBase.hpp"
#include "../Primitives.hpp"

struct RollingBallShakerSaveState final : public SaveStateBase
{
    RollingBallShakerSaveState()
        : SaveStateBase(ReliveTypes::eRollingBallShaker, sizeof(*this))
    { }
    s16 mShakeTableIdx;
    bool mStopShaking;
};

class RollingBallShaker final : public ::BaseGameObject
{
public:
    virtual void VGetSaveState(SerializedObjectData& pSaveBuffer) override;
    static void CreateFromSaveState(SerializedObjectData& pData, ResourceManager& resMan, BaseMap& map);

    RollingBallShaker(ResourceManager& resMan, BaseMap& map);
    ~RollingBallShaker();

    virtual void VUpdate() override;
    virtual void VRender(OrderingTable& ot) override;
    virtual std::vector<NotStateMemory> VNotStateMemory() override;

    Prim_ScreenOffset mPrimScreenOffset = {};
    s16 mShakeTableIdx = 0;
    bool mStopShaking = false;
};
