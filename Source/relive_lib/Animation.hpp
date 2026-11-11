#pragma once

#include "AnimationBase.hpp"
#include "Primitives.hpp"
#include <vector>

enum class AnimId;
struct AnimRecord;

const AnimRecord PerGameAnimRec(AnimId id);
const AnimRecord PerGameBgAnimRec(s32 toFindResId);
class BaseGameObject;
class BaseMap;

using TFrameCallBackType = void(*)(BaseGameObject*, u32& pointIdx, const IndexedPoint&, ResourceManager& resMan, BaseMap& map);

// The part of an animation a save has to keep for it to carry on from where it was
struct AnimationSaveState final
{
    // False for a save that has none, e.g. one converted from the original game
    bool mValid = false;
    s32 mFrame = 0;
    u32 mFrameChangeCounter = 0;
    u32 mFrameDelay = 0;
    bool mAnimate = false;
    bool mLoop = false;
    bool mFlipX = false;
    bool mRender = false;
    bool mIsLastFrame = false;
    bool mForwardLoopCompleted = false;
    // The rest of what an animation is drawn with. An object that sets these in its constructor has them back
    // already; a particle, which a factory function sets up, doesn't.
    bool mFlipY = false;
    bool mSwapXY = false;
    bool mSemiTrans = false;
    bool mBlending = false;
    bool mLoopBackwards = false;
    bool mIgnorePosOffset = false;
    s32 mBlendMode = 0;
    s32 mRenderLayer = 0;
    s16 mRed = 0;
    s16 mGreen = 0;
    s16 mBlue = 0;
    FP mSpriteScale = {};
    // Which animation it is, an AnimId
    s32 mAnimId = 0;
};

class Animation final : public AnimationBase
{
public:
    virtual void VDecode() override;
    virtual void VRender(s32 xpos, s32 ypos, OrderingTable& ot, s16 width, s32 height) override;
    virtual void VCleanUp() override;

    s16 Set_Animation_Data(AnimResource& pAnimRes);
    void Init(const AnimResource& ppAnimData, BaseGameObject* pGameObj);

    void SetFrame(s32 newFrame);
    u32 Get_Frame_Count();
    u32 MaxHeight();
    const PerFrameInfo* Get_FrameHeader(s32 frame);
    void LoadPal(const PalResource& pal);
    void ReloadPal();
    // How many entries the sprite's original palette had: 16, 64 or 256
    u32 PaletteDepth() const;
    SpriteFx& Fx() { return mAnimRes.mFx; }
    void Get_Frame_Rect(PSX_RECT* pRect);
    void Get_Frame_Width_Height(s16* pWidth, s16* pHeight);
    void Get_Frame_Offset(s16* pBoundingX, s16* pBoundingY);
    void Invoke_CallBacks();
    bool DecodeCommon();
    void DecompressFrame();

    u32 GetFrameDelay() const { return mFrameDelay; }
    void SetFrameDelay(u32 val) { mFrameDelay = val; }
    s32 GetCurrentFrame() const { return mCurrentFrame; }
    // Kept to the animation's frames (or -1, before the first): a save state can pair a motion
    // with a frame from the animation that was playing before it, see Abe::VOnTlvCollision
    void SetCurrentFrame(s32 val);
    AnimationSaveState GetSaveState() const;
    // For comparing worlds: a save leaves the colour out (it is set as the animation is drawn, see RenderOnlyMemory)
    static void SetSaveStateWithoutColour(bool without);

    // The memory of this animation that is drawing output or set as it is drawn (its poly, resource caches, colour,
    // scale), which is not state. For comparing objects' memory.
    struct Memory final
    {
        const void* mStart;
        std::size_t mSize;
    };
    std::vector<Memory> RenderOnlyMemory();
    // For the animation the object has been given again: sets where it was, not which one
    void RestoreSaveState(const AnimationSaveState& state);
    TFrameCallBackType* GetFnPtrArray() const { return mFnPtrArray; }
    void SetFnPtrArray(TFrameCallBackType* val) { mFnPtrArray = val; }
    FP GetSpriteScale() const { return mSpriteScale; }
    void SetSpriteScale(FP val) { mSpriteScale = val; }

    Poly_FT4 mPoly = {};

    // The object it was given to (it is looked for in the animation list by who has it)
    BaseGameObject* GetOwner() const { return mGameObj; }

private:
    u32 mFrameDelay = 0;
    s32 mCurrentFrame = 0;
    TFrameCallBackType* mFnPtrArray = nullptr;
    FP mSpriteScale = {};
    BaseGameObject* mGameObj = nullptr;

};

inline bool IsLastFrame(const Animation* pAnim)
{
    return pAnim->GetCurrentFrame() == static_cast<s32>(pAnim->mAnimRes.mJsonPtr->mFrames.size() - 1);
}
