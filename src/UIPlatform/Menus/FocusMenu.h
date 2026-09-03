#pragma once

#include "PCH.h"

namespace Meridian::Menus
{
    /// <summary>
    /// Transient focus menu: open ONLY while a browser holds focus. Exists
    /// for its flags (kUsesCursor gives cursor tracking, kMenuMode captures
    /// input without pausing). Its movie is the vanilla cursormenu, loaded
    /// valid then hidden — the menu never draws anything.
    /// </summary>
    class FocusMenu : public RE::IMenu
    {
    public:
        static constexpr std::string_view MENU_NAME = "MeridianUI_FocusMenu";

        static SKSE::stl::owner<RE::IMenu*> Creator();
        static void Open(bool a_pauseGame = false);
        static void Close();

        /// <summary>
        /// Registers the MenuOpenCloseEvent sink that hides the vanilla
        /// cursor movie once this menu actually opens (event-driven
        /// replacement for the old tick-budget retry chain). Call once,
        /// from UIPlatformService::Init alongside the Creator Register call.
        /// </summary>
        static void RegisterOpenCloseSink();

        void AdvanceMovie(float a_interval, std::uint32_t a_currentTime) override;
        RE::UI_MESSAGE_RESULTS ProcessMessage(RE::UIMessage& a_message) override;

        bool IsValid() const { return m_view != nullptr; }

    protected:
        FocusMenu();

        RE::GPtr<RE::GFxMovieView> m_view;
    };
}
