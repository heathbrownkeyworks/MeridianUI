#pragma once
#include "MeridianUIAPI/InputAPI.h"
#include "Services/ControllerInputService.h"
namespace Meridian::Controllers
{
    class InputAPIController final : public UI::Input::IInputAPI
    {
    public:
        static InputAPIController& GetSingleton()
        {
            static InputAPIController instance;
            return instance;
        }
        UI::Input::Result __cdecl ConfigureView(UI::Input::ViewHandle v, const UI::Input::ViewInputConfig* c) override
        {
            return Services::ControllerInputService::GetSingleton().Configure(v, c);
        }
        UI::Input::Result __cdecl GetState(UI::Input::ViewHandle v, UI::Input::InputState* s) const override
        {
            return Services::ControllerInputService::GetSingleton().GetState(v, s);
        }
        UI::Input::Result __cdecl RegisterShortcut(UI::Input::ViewHandle v, const UI::Input::ShortcutInfo* i, UI::Input::ShortcutHandle* h) override
        {
            return Services::ControllerInputService::GetSingleton().RegisterShortcut(v, i, h);
        }
        void __cdecl UnregisterShortcut(UI::Input::ShortcutHandle h) override
        {
            Services::ControllerInputService::GetSingleton().UnregisterShortcut(h);
        }
    };
}
