#pragma once

#include "PCH.h"

namespace Meridian::Menus
{
    /// <summary>
    /// Keeps the vanilla CursorMenu alive where losing it strands the user,
    /// and suppresses its movie while Meridian owns focus so Meridian's
    /// present-time cursor remains the only visible cursor.
    /// </summary>
    class CursorMenuEx : public RE::CursorMenu
    {
    public:
        static void Install();

    protected:
        void AdvanceMovie_Hook(float a_interval, std::uint32_t a_currentTime);
        RE::UI_MESSAGE_RESULTS ProcessMessage_Hook(RE::UIMessage& a_message);

        using AdvanceMovie_t = decltype(&RE::CursorMenu::AdvanceMovie);
        static inline REL::Relocation<AdvanceMovie_t> s_advanceMovie;

        using ProcessMessage_t = decltype(&RE::CursorMenu::ProcessMessage);
        static inline REL::Relocation<ProcessMessage_t> s_processMessage;
    };
}
