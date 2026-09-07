#pragma once

#include "BaseGameObject.hpp"

class CheatController final : public BaseGameObject
{
public:
    CheatController(ResourceManagerWrapper& resMan);
    ~CheatController();

    virtual void VUpdate() override;
    virtual void VScreenChanged() override;


    static bool gEnableFartGasCheat;
    static bool gVoiceCheat;
    static bool gEnableCheatFMV;
    static bool gEnableCheatLevelSelect;
};

extern CheatController* gCheatController;
