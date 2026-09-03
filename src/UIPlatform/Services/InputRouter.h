#pragma once

#include "PCH.h"

namespace Meridian::Services
{
    /// <summary>
    /// Front-of-queue input sink dispatching to browser layers, topmost
    /// first. Straight extraction of MultiLayerMenu's input half — the sink
    /// never needed the menu. A toggle pass runs across every browser before
    /// the focused-browser capture walk, so focus/visibility hotkeys work
    /// regardless of who currently holds focus — see the pass in
    /// ProcessEvent.
    /// </summary>
    class InputRouter : public RE::BSTEventSink<RE::InputEvent*>
    {
    public:
        static InputRouter& GetSingleton();

        void Register();
        void SetShuttingDown(bool a_value);

        RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event,
                                              RE::BSTEventSource<RE::InputEvent*>* a_eventSource) override;

    protected:
        std::atomic_bool m_isShuttingDown{false};
        bool m_registered = false;
    };
}
