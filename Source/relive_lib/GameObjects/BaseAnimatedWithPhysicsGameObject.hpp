#pragma once

#include "BaseGameObject.hpp"
#include "../FixedPoint.hpp"
#include "../Animation.hpp"
#include "BaseGameObject.hpp"
#include "../MapWrapper.hpp"
#include "../Animation.hpp"
#include "../RGB16.hpp"

namespace relive {
    class Path_TLV;
}

class Shadow;

enum class CameraPos : s16;

struct TintEntry
{
    EReliveLevelIds field_0_level;
    u8 field_1_r;
    u8 field_2_g;
    u8 field_3_b;
};

enum class Scale : s16
{
    Bg = 0,
    Fg = 1,
};

class BaseAnimatedWithPhysicsGameObject : public BaseGameObject
{
public:
    static void MakeArray();
    static void FreeArray();

    explicit BaseAnimatedWithPhysicsGameObject(s16 resourceArraySize, ResourceManagerWrapper& resMan, BaseMap& map);
    ~BaseAnimatedWithPhysicsGameObject();

    virtual void VRender(OrderingTable& ot) override;

    virtual PSX_RECT VGetBoundingRect();
    virtual bool VIsObjNearby(FP radius, BaseAnimatedWithPhysicsGameObject* pObj);
    virtual bool VIsObj_GettingNear_On_X(BaseAnimatedWithPhysicsGameObject* pObj);
    virtual bool VIsFacingMe(BaseAnimatedWithPhysicsGameObject* pOther);
    virtual bool VOnSameYLevel(BaseAnimatedWithPhysicsGameObject* pOther);
    virtual void VStackOnObjectsOfType(ReliveTypes typeToFind);
    virtual void VOnAbeInteraction();
    virtual void VOnThrowableHit(BaseGameObject* pFrom);

    // The tables end at the credits entry, not a terminator, so the size is what stops a level that
    // isn't in the table from reading past the end. It gets the last entry.
    template <size_t N>
    void SetTint(const TintEntry (&tintTable)[N], EReliveLevelIds levelId)
    {
        SetTint(tintTable, N, levelId);
    }
    void SetTint(const TintEntry* pTintArray, size_t count, EReliveLevelIds levelId);
    // For a table whose size isn't known here: it must end with an eNone entry, which a level that
    // isn't in the table gets
    void SetTintTerminated(const TintEntry* pTintArray, EReliveLevelIds levelId);

    // Note inlined in AO everywhere
    CameraPos Is_In_Current_Camera();

    void Animation_Init(const AnimResource& res);

protected:
    void DeathSmokeEffect(bool bPlaySound);

    enum class BetweenCamPos : s16
    {
        None_0 = 0,
        Left_1 = 1,
        Right_2 = 2,
    };

    BetweenCamPos BetweenCameras_418500();

public:
    FP mXPos = {};
    FP mYPos = {};
    s16 mCurrentPath = 0;
    EReliveLevelIds mCurrentLevel = EReliveLevelIds::eNone;
    FP mVelX = {};
    FP mVelY = {};

    RGB16 mRGB;
    s16 mYOffset = 0;
    s16 mXOffset = 0;

    bool GetApplyShadowZoneColour() const
    {
        return mApplyShadowZoneColour;
    }

    void SetApplyShadowZoneColour(bool val)
    {
        mApplyShadowZoneColour = val;
    }

    bool GetDoPurpleLightEffect() const
    {
        return mDoPurpleLightEffect;
    }

    void SetDoPurpleLightEffect(bool val)
    {
        mDoPurpleLightEffect = val;
    }

    const Shadow* GetShadow() const
    {
        return mShadow;
    }

    Shadow* GetShadow()
    {
        return mShadow;
    }

    const Scale& GetScale() const
    {
        return mScale;
    }

    void SetScale(Scale val)
    {
        mScale = val;
    }

    const FP& GetSpriteScale() const
    {
        return mSpriteScale;
    }

    void SetSpriteScale(FP val)
    {
        mSpriteScale = val;
    }

    Animation& GetAnimation()
    {
        return mAnim;
    }

    void CreateShadow();

private:
    Animation mAnim = {};
    Scale mScale = Scale::Fg;
    FP mSpriteScale = {};
    bool mApplyShadowZoneColour = false;
    bool mDoPurpleLightEffect = false;
protected:
    Shadow* mShadow = nullptr;
};

extern DynamicArrayT<BaseGameObject>* gObjListDrawables;
