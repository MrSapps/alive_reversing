#include "Menu.hpp"
#include "data_conversion/relive_tlvs.hpp"
#include "logger.hpp"
#include "FatalError.hpp"
#include "../AliveLibAE/Input.hpp" // TODO: Needs merging between games

namespace relive
{
Menu::Menu(relive::Path_TLV* /*pTlv*/, const Guid& /*tlvId*/)
 : BaseGameObject(true, 0)
{
    SetUpdateDuringCamSwap(true);

    mMenuConfig = LoadMenuConfig("relive_data/ae/menu/main_menu.json");

    
    mLoadedAnims.push_back(ResourceManagerWrapper::LoadAnimation(AnimId::Slog_Scratch));
    
    mCircleButtonAnim.Init(ResourceManagerWrapper::LoadAnimation(AnimId::Slog_Scratch), this);
    
    mCircleButtonAnim.SetRGB(127, 127, 127);
    mCircleButtonAnim.SetRenderLayer(Layer::eLayer_MainMenuButtonBees_38);
    mCircleButtonAnim.SetBlendMode(relive::TBlendModes::eBlend_1);
    
    //   mCircleButtonAnim.SetAnimate(true);
    
    gObjListDrawables->Push_Back(this);
    this->SetDrawable(true);
    
    GotoCamera(mMenuConfig.start_camera);
}

Menu::~Menu()
{
    gObjListDrawables->Remove_Item(this);
}

void Menu::VUpdate()
{
    switch (mScreenState)
    {
        case MenuScreenState::OnEnter:
        {
            //LOG_INFO("Menu OnEnter");
            mScreenState = MenuScreenState::RunningSequnce;
            RunSequnce();
            break;
        }

        case MenuScreenState::RunningSequnce:
        {
            //LOG_INFO("Menu RunningSequnce");
            RunSequnce();
            break;
        }

        case MenuScreenState::ShowMenu:
        {
            //LOG_INFO("Menu ShowMenu");
            const u32 pressed = Input().mPads[0].mPressed;
            if (pressed & (InputCommands::eUp))
            {
                NavigateButton(-1);
            }
            else if (pressed & InputCommands::eDown)
            {
                NavigateButton(1);
            }
            else if (pressed & InputCommands::eUnPause_OrConfirm)
            {
                if (mCurrentButton)
                {
                    LOG_INFO("Button pressed: %s", mCurrentButton->id.c_str());
                    switch (mCurrentButton->action.type)
                    {
                        case ButtonActionType::StartGame:
                        {
                            LOG_INFO("Start game");
                            break;
                        }

                        case ButtonActionType::ExitGame:
                        {
                            LOG_INFO("Exit game");
                            break;
                        }

                        case ButtonActionType::GotoCamera:
                        {
                            GotoCamera(mCurrentButton->action.camera);
                            break;
                        }
                    }
                }
            }

            // TODO: Time out logic
            break;
        }

        case MenuScreenState::OnExit:
        {
            LOG_INFO("Menu OnExit");
            break;
        }
    }
}

void Menu::VRender(OrderingTable& ot)
{
    switch (mScreenState)
    {
        case MenuScreenState::ShowMenu:
        {
            if (mCurrentButton)
            {
                //LOG_INFO("Current button: %s at %d, %d", mCurrentButton->id.c_str(), mCurrentButton->position.x, mCurrentButton->position.y);
                mCircleButtonAnim.VRender(mCurrentButton->position.x, mCurrentButton->position.y, ot, 0, 0);
            }
        }
        break;
    }
}

void Menu::VScreenChanged()
{

}

void Menu::GotoCamera(const std::string& cameraName)
{
    LOG_INFO("Switch to camera: %s", cameraName.c_str());

    auto it = mMenuConfig.cameras.find(cameraName);
    if (it != mMenuConfig.cameras.end())
    {
        mCurrentCamera = &it->second;
        mScreenState = MenuScreenState::OnEnter;

        mCurrentButtonIndex = 0;
        mCurrentButton = nullptr;
        if (mCurrentCamera->menu && !mCurrentCamera->menu->default_button.empty())
        {
            SetCurrentButton(*mCurrentCamera->menu, mCurrentCamera->menu->default_button);
        }

        mCurrentEnterEvent = nullptr;
        if (!mCurrentCamera->on_enter.empty())
        {
            mCurrentEnterEvent = &mCurrentCamera->on_enter[0];
        }
    }
    else
    {
        ALIVE_FATAL("Camera %s not found", cameraName.c_str());
    }
}

void Menu::SetCurrentButton(const MenuButtons& buttons, const std::string& buttonId)
{
    LOG_INFO("Set current button: %s", buttonId.c_str());

    for (const Button& button : buttons.buttons)
    {
        if (button.id == buttonId)
        {
            mCurrentButton = &button;
            return;
        }
    }
    ALIVE_FATAL("Button %s not found", buttonId.c_str());
}

void Menu::RunSequnce()
{
    if (mCurrentEnterEvent)
    {    
        switch (mCurrentEnterEvent->type)
        {
            case EventType::Fmv:
            {
                LOG_INFO("Play FMV: %s", mCurrentEnterEvent->id.c_str());
                SetNextOnEnterSequnce();
                break;
            }

            case EventType::Animation:
            {
                LOG_INFO("Play Animation: %s", mCurrentEnterEvent->id.c_str());
                SetNextOnEnterSequnce();
                break;
            }

            case EventType::ShowMenu:
            {
                LOG_INFO("Show menu");
                mScreenState = MenuScreenState::ShowMenu;
                break;
            }
            break;
/*            
    case EventType::Effect,
    
    case EventType::ConditionalFmv,
*/
            case EventType::GotoCamera:
            {
                GotoCamera(mCurrentEnterEvent->camera);
                break;
            }
        }
    }
}

void Menu::SetNextOnEnterSequnce()
{
    mCurrentEnterEventIdx++;
    if (mCurrentEnterEventIdx < mCurrentCamera->on_enter.size())
    {
        mCurrentEnterEvent = &mCurrentCamera->on_enter[mCurrentEnterEventIdx];
    }
    else
    {
        mCurrentEnterEvent = nullptr;
    }
}

void Menu::NavigateButton(s32 toIndex)
{
    if (mCurrentCamera->menu)
    {
        std::size_t maxButtons = mCurrentCamera->menu->buttons.size();
        s32 tmpButtonIndex = static_cast<s32>(mCurrentButtonIndex) + toIndex;
        if (tmpButtonIndex < 0)
        {
            tmpButtonIndex = static_cast<s32>(maxButtons) - 1;
        }
        else if (tmpButtonIndex >= static_cast<s32>(maxButtons))
        {
            tmpButtonIndex = 0;
        }

        mCurrentButtonIndex = static_cast<u32>(tmpButtonIndex);
        mCurrentButton = &mCurrentCamera->menu->buttons[mCurrentButtonIndex];
    }
}

}
