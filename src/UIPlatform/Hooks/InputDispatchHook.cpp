#include "InputDispatchHook.h"

#include "CallSiteGuard.h"
#include "Common/InputDispatchFilter.h"
#include "Services/InputLangSwitchService.h"
#include "Services/InputRouter.h"

namespace Meridian::Hooks
{
    std::uintptr_t InputDispatchHook::CallSiteAddress()
    {
        const REL::RelocationID pollInputDevicesId(67315, 68617);
        const REL::Relocation<std::uintptr_t> callSite{
            pollInputDevicesId,
            REL::Relocate(0x7B, 0x7B, 0x81)};
        return callSite.address();
    }

    bool InputDispatchHook::Install()
    {
        try
        {
            const auto callSite = CallSiteAddress();
            if (!IsExpectedCallSite(callSite, CallEncoding::Relative5))
            {
                LOG_ERROR(
                    "{}: install refused at {:X}: expected E8 rel32 call for Skyrim {}",
                    NameOf(InputDispatchHook),
                    callSite,
                    REL::Module::get().version().string());
                return false;
            }

            s_priorityOriginal = SKSE::GetTrampoline().write_call<5>(
                callSite,
                &InputDispatchHook::PriorityDetour);
            s_priorityInstalled.store(true, std::memory_order_release);
            LOG_INFO("{}: priority layer installed at {:X}", NameOf(InputDispatchHook), callSite);
            return true;
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("{}: install FAILED ({}) — focused input ownership unavailable",
                          NameOf(InputDispatchHook),
                          e.what());
            return false;
        }
    }

    bool InputDispatchHook::RegisterOutermostInstall()
    {
        const auto messaging = SKSE::GetMessagingInterface();
        if (messaging == nullptr)
        {
            LOG_ERROR("{}: SKSE messaging unavailable; outer input guard cannot be scheduled",
                          NameOf(InputDispatchHook));
            return false;
        }

        const bool registered = messaging->RegisterListener(
            [](SKSE::MessagingInterface::Message* a_message) {
                if (a_message != nullptr &&
                    a_message->type == SKSE::MessagingInterface::kPostPostLoad)
                {
                    InputDispatchHook::InstallOutermost();
                }
            });
        if (!registered)
        {
            LOG_ERROR("{}: failed to register the post-plugin-load input guard",
                          NameOf(InputDispatchHook));
        }
        return registered;
    }

    bool InputDispatchHook::InstallOutermost()
    {
        if (s_outerInstalled.load(std::memory_order_acquire))
        {
            return true;
        }

        try
        {
            if (!s_priorityInstalled.load(std::memory_order_acquire))
            {
                LOG_ERROR("{}: outer install refused because the priority layer is unavailable",
                              NameOf(InputDispatchHook));
                return false;
            }

            const auto callSite = CallSiteAddress();
            if (!IsExpectedCallSite(callSite, CallEncoding::Relative5))
            {
                LOG_ERROR(
                    "{}: outer install refused at {:X}: expected E8 rel32 call for Skyrim {}",
                    NameOf(InputDispatchHook),
                    callSite,
                    REL::Module::get().version().string());
                return false;
            }

            // This second, distinct layer is installed only after every SKSE
            // plugin has loaded. It therefore runs before hooks such as OAR's
            // direct ImGui input handler while preserving their original chain.
            s_outerOriginal = SKSE::GetTrampoline().write_call<5>(
                callSite,
                &InputDispatchHook::OutermostDetour);
            s_outerInstalled.store(true, std::memory_order_release);
            LOG_INFO("{}: outer focused-input guard installed at {:X} after SKSE plugin loading",
                         NameOf(InputDispatchHook),
                         callSite);
            return true;
        }
        catch (const std::exception& e)
        {
            LOG_ERROR("{}: outer install FAILED ({}) — competing hooks may observe focused input",
                          NameOf(InputDispatchHook),
                          e.what());
            return false;
        }
    }

    bool InputDispatchHook::IsInstalled()
    {
        return s_priorityInstalled.load(std::memory_order_acquire) &&
               s_outerInstalled.load(std::memory_order_acquire);
    }

    void InputDispatchHook::PriorityDetour(
        RE::BSTEventSource<RE::InputEvent*>* a_source,
        RE::InputEvent* const* a_events)
    {
        Meridian::Services::InputRouter::GetSingleton().PrioritizeForDispatch(a_source);
        s_priorityOriginal(a_source, a_events);
    }

    void InputDispatchHook::OutermostDetour(
        RE::BSTEventSource<RE::InputEvent*>* a_source,
        RE::InputEvent* const* a_events)
    {
        auto& router = Meridian::Services::InputRouter::GetSingleton();
        router.PrioritizeForDispatch(a_source);

        const auto consumed = router.RouteBatch(a_events ? *a_events : nullptr);
        if (!consumed.empty())
        {
            auto& languageSwitch = Meridian::Services::InputLangSwitchService::GetSingleton();
            if (languageSwitch.IsActive())
            {
                languageSwitch.ProcessEvent(a_events, a_source);
            }
        }

        // The all-pass path preserves the original batch without touching links.
        const Meridian::Services::InputRouter::PreprocessedDispatchScope forwardingScope;
        if (consumed.empty())
        {
            s_outerOriginal(a_source, a_events);
            return;
        }
        Meridian::Common::ScopedInputFilter<RE::InputEvent> filtered(a_events ? *a_events : nullptr,
            [&consumed](RE::InputEvent* event) { return consumed.contains(event); });
        RE::InputEvent* forwardedHead=filtered.Head();

        // The original chain eventually reaches Skyrim's event source, where
        // InputRouter is still registered as a sink. Suppress that second visit
        // while forwarding so Chromium receives each physical transition once.
        s_outerOriginal(a_source, &forwardedHead);
    }
}
