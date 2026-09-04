#pragma once

#include "PCH.h"

namespace Meridian::Hooks
{
    /// Runs immediately before BSInputDeviceManager dispatches an input batch.
    /// This is the only point that can repair sink ordering before any late or
    /// competing SKSE sink observes the event.
    class InputDispatchHook
    {
    public:
        static bool Install();
        static bool RegisterOutermostInstall();
        static bool IsInstalled();

    private:
        using Dispatch = void(RE::BSTEventSource<RE::InputEvent*>*, RE::InputEvent* const*);

        static bool InstallOutermost();
        static std::uintptr_t CallSiteAddress();
        static void PriorityDetour(RE::BSTEventSource<RE::InputEvent*>* a_source,
                                   RE::InputEvent* const* a_events);
        static void OutermostDetour(RE::BSTEventSource<RE::InputEvent*>* a_source,
                                    RE::InputEvent* const* a_events);

        static inline REL::Relocation<Dispatch> s_priorityOriginal;
        static inline REL::Relocation<Dispatch> s_outerOriginal;
        static inline std::atomic_bool s_priorityInstalled{false};
        static inline std::atomic_bool s_outerInstalled{false};
    };
}
