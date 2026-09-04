#pragma once

#include "PCH.h"
#include "Common/Singleton.h"
#include "Converters/KeyInputConverter.h"

namespace Meridian::Services
{
    class InputLangSwitchService : public Meridian::Common::Singleton<InputLangSwitchService>,
                                   public RE::BSTEventSink<RE::InputEvent*>
    {
    private:
        friend class Meridian::Common::Singleton<InputLangSwitchService>;
        std::mutex m_switchActiveMutex;
        bool m_isSwitchActive = false;

    public:
        void SetActive(bool a_value);
        bool IsActive();

        // RE::BSTEventSink<RE::InputEvent*>
        RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>* a_eventSource) override;
    };
}
