#pragma once
#include "PCH.h"
#include "Input/ControllerState.h"
#include "Input/InputPolicy.h"
#include <unordered_map>
#include <unordered_set>
#include <nlohmann/json.hpp>

namespace Meridian::CEF
{
    class DefaultBrowser;
}
namespace Meridian::Services
{
    class ControllerInputService
    {
    public:
        static ControllerInputService& GetSingleton();
        UI::Input::Result Configure(UI::Input::ViewHandle, const UI::Input::ViewInputConfig*);
        UI::Input::Result GetState(UI::Input::ViewHandle, UI::Input::InputState*) const;
        UI::Input::Result RegisterShortcut(UI::Input::ViewHandle, const UI::Input::ShortcutInfo*, UI::Input::ShortcutHandle*);
        void UnregisterShortcut(UI::Input::ShortcutHandle);
        void RemoveView(UI::Input::ViewHandle);
        void PageRequest(UI::Input::ViewHandle, const std::string&);
        void CancelBrowser(CEF::DefaultBrowser*, bool navigation = false);
        std::unordered_set<RE::InputEvent*> Route(RE::InputEvent*);
        void Shutdown();
        bool DrawCursor() const
        {
            return m_drawCursor.load();
        }

    private:
        struct Entry
        {
            UI::Input::ViewInputConfig config;
            std::weak_ptr<CEF::DefaultBrowser> browser;
            std::string page;
            UI::Input::Mode mode = UI::Input::Mode::Navigation;
            std::uint64_t epoch = 0;
            bool stateDirty = true;
        };
        struct Shortcut
        {
            UI::Input::ViewHandle view;
            UI::Input::ShortcutInfo info;
        };
        mutable std::mutex m_mutex;
        std::unordered_map<UI::Input::ViewHandle, Entry> m_views;
        std::unordered_map<UI::Input::ShortcutHandle, Shortcut> m_shortcuts;
        Input::ControllerState m_state;
        std::uint64_t m_nextShortcut = 1, m_sequence = 0;
        bool m_connected = false, m_shutdown = false, m_initialized = false;
        bool m_gamepadPresentation = false;
        bool m_shortcutPending = false;
        double m_lastTime = 0;
        double m_lastTrace = 0;
        std::uint32_t m_traceDispatches = 0, m_traceEmpty = 0;
        std::atomic_bool m_drawCursor{true};
    };
}
