#pragma once

#include "BaseAliveGameObject.hpp"

class BaseAbe : public ::BaseAliveGameObject
{
public:
    explicit BaseAbe(s16 resourceArraySize, ResourceManager& resMan, BaseMap& map);
    virtual bool IsEnteringOrExitingDoor() const = 0;
    virtual s32 DoorId() const = 0;
};

BaseAbe* GetAbe();
