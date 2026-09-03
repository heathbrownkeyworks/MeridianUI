#include "InputRouter.h"

#include "Menus/FocusArbiter.h"
#include "Render/RenderHost.h"

#include <vector>

namespace Meridian::Services
{
    InputRouter& InputRouter::GetSingleton()
    {
        static InputRouter instance;
        return instance;
    }

    void InputRouter::Register()
    {
        if (m_registered)
        {
            spdlog::warn("{}: Register() called more than once — ignoring", NameOf(InputRouter));
            return;
        }

        const auto inputEventSource = RE::BSInputDeviceManager::GetSingleton();
        inputEventSource->lock.Lock();
        Meridian::Utils::PushFront<RE::BSTEventSink<RE::InputEvent*>>(inputEventSource->sinks, this);
        inputEventSource->lock.Unlock();
        m_registered = true;
    }

    void InputRouter::SetShuttingDown(bool a_value)
    {
        m_isShuttingDown.store(a_value, std::memory_order_release);
    }

    RE::BSEventNotifyControl InputRouter::ProcessEvent(RE::InputEvent* const* a_event,
                                                       RE::BSTEventSource<RE::InputEvent*>* a_eventSource)
    {
        if (a_event == nullptr || *a_event == nullptr ||
            m_isShuttingDown.load(std::memory_order_acquire))
        {
            return RE::BSEventNotifyControl::kContinue;
        }

        auto& compositor = Meridian::Render::RenderHost::GetSingleton().GetCompositor();
        if (compositor.Empty())
        {
            return RE::BSEventNotifyControl::kContinue;
        }

        auto inputEvent = *a_event;
        auto result = RE::BSEventNotifyControl::kContinue;
        const auto menus = compositor.SortedSnapshot();

        // Focus may be claimed by a modifier chord (for example Shift+Z).
        // The down event has already reached Skyrim before the claim, but a
        // focused browser would normally stop the matching release event.
        // Pass through only a batch whose button events are exclusively
        // releases of keys held at the first focus claim. This balances the
        // game's key state without leaking ordinary focused input.
        auto& focusArbiter = Meridian::Menus::FocusArbiter::GetSingleton();
        std::vector<std::uint32_t> openingKeyReleases;
        bool releaseOnlyBatch = true;
        for (auto* candidate = *a_event; candidate != nullptr; candidate = candidate->next)
        {
            if (candidate->GetEventType() != RE::INPUT_EVENT_TYPE::kButton)
            {
                continue;
            }

            const auto* button = candidate->AsButtonEvent();
            const auto device = button->GetDevice();
            const bool keyboardEvent = device == RE::INPUT_DEVICE::kKeyboard ||
                                       device == RE::INPUT_DEVICE::kFlatVirtualKeyboard;
            if (!keyboardEvent || !button->IsUp() ||
                !focusArbiter.IsOpeningKeyHeld(button->GetIDCode()))
            {
                releaseOnlyBatch = false;
                break;
            }
            openingKeyReleases.push_back(button->GetIDCode());
        }

        bool passThroughOpeningKeyReleases = releaseOnlyBatch && !openingKeyReleases.empty();
        if (passThroughOpeningKeyReleases)
        {
            for (const auto scanCode : openingKeyReleases)
            {
                if (!focusArbiter.ConsumeOpeningKeyRelease(scanCode))
                {
                    passThroughOpeningKeyReleases = false;
                    break;
                }
            }
        }

        while (inputEvent != nullptr)
        {
            if (inputEvent->GetEventType() == RE::INPUT_EVENT_TYPE::kButton)
            {
                // Toggle pass runs for EVERY browser before any focused
                // browser can swallow the event: focus hotkeys work
                // regardless of who currently holds focus. Documented
                // contract: while a registered toggle chord is held, any
                // button-down that completes or re-satisfies it is consumed
                // and never reaches a page; key-ups and lone chord-halves
                // are not intercepted.
                bool toggled = false;
                for (auto it = menus.rbegin(); it != menus.rend(); ++it)
                {
                    if ((*it)->ProcessToggleKeys(inputEvent->AsButtonEvent()))
                    {
                        toggled = true;
                    }
                }
                if (toggled)
                {
                    result = RE::BSEventNotifyControl::kStop;
                    inputEvent = inputEvent->next;
                    continue;
                }
            }

            for (auto it = menus.rbegin(); it != menus.rend(); ++it)
            {
                switch (inputEvent->GetEventType())
                {
                case RE::INPUT_EVENT_TYPE::kMouseMove:
                    if ((*it)->ProcessMouseMove(inputEvent->AsMouseMoveEvent()))
                    {
                        result = RE::BSEventNotifyControl::kStop;
                        continue;
                    }
                    break;
                case RE::INPUT_EVENT_TYPE::kButton:
                    if ((*it)->ProcessButton(inputEvent->AsButtonEvent()))
                    {
                        result = RE::BSEventNotifyControl::kStop;
                        continue;
                    }
                    break;
                default:
                    break;
                }
            }

            inputEvent = inputEvent->next;
        }

        return passThroughOpeningKeyReleases ?
            RE::BSEventNotifyControl::kContinue : result;
    }
}
