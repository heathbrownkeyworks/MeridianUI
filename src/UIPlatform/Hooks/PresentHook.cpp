#include "PresentHook.h"

#include "CallSiteGuard.h"
#include "Render/RenderHost.h"

namespace Meridian::Hooks
{
    bool PresentHook::Install(Meridian::Config::CompositorTiming a_timing)
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
            s_timing.store(a_timing, std::memory_order_release);
            s_original = trampoline.write_call<5>(callSite.address(), &PresentHook::Detour);
            s_installed.store(true, std::memory_order_release);
            spdlog::info(
                "{}: installed at {:X}; compositor timing={}",
                NameOf(PresentHook),
                callSite.address(),
                Meridian::Config::ToString(a_timing));
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

    void PresentHook::CompositeSafely(Meridian::Render::CompositorTarget a_target) noexcept
    {
        try
        {
            Meridian::Render::RenderHost::GetSingleton().OnPresent(a_target);
        }
        catch (const std::exception& error)
        {
            spdlog::error("{}: compositor failed: {}", NameOf(PresentHook), error.what());
        }
        catch (...)
        {
            spdlog::error("{}: compositor failed with an unknown exception", NameOf(PresentHook));
        }
    }

    void PresentHook::Detour(std::uint32_t a_p1)
    {
        const auto timing = s_timing.load(std::memory_order_acquire);
        if (timing == Meridian::Config::CompositorTiming::BeforeRendererEnd)
        {
            CompositeSafely(Meridian::Render::CompositorTarget::BoundGameRenderTarget);
        }

        s_original(a_p1);

        if (timing == Meridian::Config::CompositorTiming::AfterRendererEnd)
        {
            CompositeSafely(Meridian::Render::CompositorTarget::SwapChainBackbuffer);
        }
    }
}
