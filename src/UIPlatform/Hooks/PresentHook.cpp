#include "PresentHook.h"

#include "CallSiteGuard.h"
#include "Render/RenderHost.h"

namespace Meridian::Hooks
{
    bool PresentHook::Install()
    {
        try
        {
            const REL::RelocationID presentId(75461, 77246);
            const REL::Relocation<std::uintptr_t> callSite{presentId, 0x9};
            if (!IsExpectedCallSite(callSite.address(), CallEncoding::Relative5))
            {
                spdlog::error(
                    "{}: install refused at {:X}: expected E8 rel32 call for Skyrim {}",
                    NameOf(PresentHook),
                    callSite.address(),
                    REL::Module::get().version().string());
                return false;
            }
            auto& trampoline = SKSE::GetTrampoline();
            s_original = trampoline.write_call<5>(callSite.address(), &PresentHook::Detour);
            s_installed.store(true, std::memory_order_release);
            spdlog::info("{}: installed at {:X}", NameOf(PresentHook), callSite.address());
            return true;
        }
        catch (const std::exception& e)
        {
            spdlog::error("{}: install FAILED ({}) — platform rendering unavailable, browser creation will be refused", NameOf(PresentHook), e.what());
            return false;
        }
    }

    bool PresentHook::IsInstalled()
    {
        return s_installed.load(std::memory_order_acquire);
    }

    void PresentHook::Detour(std::uint32_t a_p1)
    {
        s_original(a_p1);
        Meridian::Render::RenderHost::GetSingleton().OnPresent();
    }
}
