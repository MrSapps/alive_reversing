#pragma once

#include "BaseGameObject.hpp"
#include "../data_conversion/guid.hpp"
#include "../MenuConfig.hpp"
#include "Animation.hpp"

namespace relive
{
    struct Path_TLV;

    class Menu final : public BaseGameObject
    {
    public:
        Menu(relive::Path_TLV* pTlv, const Guid& tlvId);
        ~Menu();
    private:
        void VUpdate() override;
        void VRender(OrderingTable& ot) override;
        void VScreenChanged() override;

        void GotoCamera(const std::string& cameraName);
        void SetCurrentButton(const MenuButtons& buttons, const std::string& buttonId);

        void RunSequnce();
        void SetNextOnEnterSequnce();

        enum class MenuScreenState
        {
            OnEnter,
            RunningSequnce,
            ShowMenu,
            OnExit
        };
        MenuScreenState mScreenState = MenuScreenState::OnEnter;

        void NavigateButton(s32 toIndex);

        Animation mCircleButtonAnim;

        MenuConfig mMenuConfig;
        Camera* mCurrentCamera = nullptr;

        u32 mCurrentButtonIndex = 0;
        const Button* mCurrentButton = nullptr;

        u32 mCurrentEnterEventIdx = 0;
        const Event* mCurrentEnterEvent = nullptr;
    };
}
