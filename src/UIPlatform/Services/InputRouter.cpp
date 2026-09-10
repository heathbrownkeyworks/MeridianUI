#include "InputRouter.h"

#include "Common/InputSinkPriority.h"
#include "Menus/FocusArbiter.h"
#include "Render/RenderHost.h"
#include "Services/InputLangSwitchService.h"
#include "Services/ControllerInputService.h"

#include <vector>

namespace Meridian::Services
{
    thread_local std::uint32_t InputRouter::s_preprocessedDispatchDepth = 0;

    InputRouter::PreprocessedDispatchScope::PreprocessedDispatchScope()
    {
        ++InputRouter::s_preprocessedDispatchDepth;
    }

    InputRouter::PreprocessedDispatchScope::~PreprocessedDispatchScope()
    {
        --InputRouter::s_preprocessedDispatchDepth;
    }

    InputRouter& InputRouter::GetSingleton()
    {
        static InputRouter instance;
        return instance;
    }

    void InputRouter::Register()
    {
        if (m_registered.load(std::memory_order_acquire))
        {
            spdlog::warn("{}: Register() called more than once — ignoring", NameOf(InputRouter));
            return;
        }

        const auto inputEventSource = RE::BSInputDeviceManager::GetSingleton();
        inputEventSource->lock.Lock();
        Meridian::Utils::PushFront<RE::BSTEventSink<RE::InputEvent*>>(inputEventSource->sinks, this);
        inputEventSource->lock.Unlock();
        m_registered.store(true, std::memory_order_release);
    }

    void InputRouter::SetShuttingDown(bool a_value)
    {
        m_isShuttingDown.store(a_value, std::memory_order_release);
    }

    void InputRouter::PrioritizeForDispatch(
        RE::BSTEventSource<RE::InputEvent*>* a_eventSource)
    {
        if (!m_registered.load(std::memory_order_acquire) || a_eventSource == nullptr)
        {
            return;
        }

        using Sink = RE::BSTEventSink<RE::InputEvent*>;
        auto* router = static_cast<Sink*>(this);
        auto* languageSwitch = static_cast<Sink*>(&InputLangSwitchService::GetSingleton());

        RE::BSSpinLockGuard lock(a_eventSource->lock);
        Meridian::Common::PromoteInputSinks(
            a_eventSource->sinks,
            router,
            languageSwitch);
    }

    std::unordered_set<RE::InputEvent*> InputRouter::RouteBatch(RE::InputEvent* events)
    {
        if (m_isShuttingDown.load())
            return {};
        auto consumed = ControllerInputService::GetSingleton().Route(events);
        auto& compositor = Meridian::Render::RenderHost::GetSingleton().GetCompositor();
        const auto menus = compositor.SortedSnapshot();
        auto& focus = Meridian::Menus::FocusArbiter::GetSingleton();
        for (auto* event = events; event; event = event->next)
        {
            if (event->GetDevice() == RE::INPUT_DEVICE::kGamepad)
                continue;
            bool handled = false;
            if (event->GetEventType() == RE::INPUT_EVENT_TYPE::kButton)
            {
                auto button = event->AsButtonEvent();
                for (auto it = menus.rbegin(); it != menus.rend(); ++it)
                    handled = (*it)->ProcessToggleKeys(button) || handled;
                if (handled)
                {
                    consumed.insert(event);
                    continue;
                }
                for (auto it = menus.rbegin(); it != menus.rend(); ++it)
                    if ((*it)->ProcessButton(button))
                    {
                        handled = true;
                        break;
                    }
                const bool keyboard = event->GetDevice() == RE::INPUT_DEVICE::kKeyboard ||
                                      event->GetDevice() == RE::INPUT_DEVICE::kFlatVirtualKeyboard;
                if (keyboard && button->IsUp() && focus.ConsumeOpeningKeyRelease(button->GetIDCode()))
                    handled = false;
            }
            else if (event->GetEventType() == RE::INPUT_EVENT_TYPE::kMouseMove)
            {
                for (auto it = menus.rbegin(); it != menus.rend(); ++it)
                    if ((*it)->ProcessMouseMove(event->AsMouseMoveEvent()))
                    {
                        handled = true;
                        break;
                    }
            }
            else if (event->GetEventType() == RE::INPUT_EVENT_TYPE::kChar && focus.HasOwner())
            {
                handled = true;
            }
            if (handled)
                consumed.insert(event);
        }
        return consumed;
    }
    RE::BSEventNotifyControl InputRouter::ProcessEvent(RE::InputEvent* const* events,
                                                       [[maybe_unused]] RE::BSTEventSource<RE::InputEvent*>* source)
    {
        if (s_preprocessedDispatchDepth)
            return RE::BSEventNotifyControl::kContinue;
        return RouteBatch(events ? *events : nullptr).empty() ? RE::BSEventNotifyControl::kContinue : RE::BSEventNotifyControl::kStop;
    }
}
