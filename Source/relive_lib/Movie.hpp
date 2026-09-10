#pragma once

#include "GameObjects/BaseGameObject.hpp"
#include <string>

struct CdlLOC;

s8 DDV_Play(const char_type* pDDVName);
bool AreMovieSkippingInputsHeld();

class Movie final : public BaseGameObject
{
public:
    virtual void VUpdate() override;
    virtual void VScreenChanged() override;

    explicit Movie(const char_type* pName, ResourceManagerWrapper& resMan, BaseMap& map);
    
    static s32 gMovieRefCount;
private:
    void Init();
    void DeInit();

    std::string mName;
};

