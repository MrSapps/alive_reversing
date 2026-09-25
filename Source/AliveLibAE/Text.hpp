#pragma once

#include "../relive_lib/GameObjects/BaseGameObject.hpp"
#include "Font.hpp"

enum class MessageType : s32
{
    eSkipDemo_2 = 2,
};

class Text final : public BaseGameObject
{
public:
    Text(const char_type* pMessage, s32 renderCount, s32 bShadow, ResourceManagerWrapper& resMan, BaseMap& map);
    ~Text();

    virtual void VUpdate() override;
    virtual void VRender(OrderingTable& ot) override;
    virtual void VScreenChanged() override;

    void SetYPos(s32 /*not_used*/, s16 ypos);

private:
    FontContext mFontContext;
    PalResource mPal;
    AliveFont field_20_font = {};
    s16 field_58_k0_unused = 0;
    s16 field_5A_k4_unused = 0;
    s16 field_5C_xpos = 0;
    s16 field_5E_ypos = 0;
    s16 field_60_bShadow = 0;
    s32 field_64_render_count = 0;
    char_type field_68_txt_buffer[60] = {};
};

// Shows a full screen message until Enter, Escape or a timeout, the game frozen behind it (see
// BaseGameObject::StartModal). While recording or playing back it closes straight away, in
// the constructor. Once closed it waits for whoever made it to read Result() and kill it.
class FullScreenMessage final : public BaseGameObject
{
public:
    FullScreenMessage(MessageType messageType, ResourceManagerWrapper& resMan, BaseMap& map);
    ~FullScreenMessage();

    virtual ModalState VModalUpdate() override;

    enum class Result
    {
        eOpen,
        eEnterOrTimeout,
        eEscape,
    };

    Result GetResult() const
    {
        return mResult;
    }

private:
    void Close();

    enum class State
    {
        eWaitForKey,
        eWaitForEscapeRelease,
        eWaitForReturnRelease,
    };

    State mState = State::eWaitForKey;
    Text* mText = nullptr;
    Text* mText2 = nullptr;
    u32 mDisplayUntil = 0;
    Result mResult = Result::eOpen;
    // What mResult becomes when it closes
    Result mClosedResult = Result::eEnterOrTimeout;
};
