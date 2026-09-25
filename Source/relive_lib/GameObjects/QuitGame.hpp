#pragma once

#include "BaseGameObject.hpp"

// Quits the game: it takes over the main loop straight away and tells it to stop (see
// ModalState::eQuitGame), so the rest of the frame it was made in doesn't run.
class QuitGame final : public BaseGameObject
{
public:
    QuitGame(ResourceManagerWrapper& resMan, BaseMap& map)
        : BaseGameObject(true, 0, resMan, map)
    {
        SetSurviveDeathReset(true);
        SetUpdateDuringCamSwap(true);
        StartModal();
    }

    ModalState VModalUpdate() override
    {
        return ModalState::eQuitGame;
    }

    void VScreenChanged() override
    {
        // Must outlive any screen change until the main loop has stopped
    }
};
