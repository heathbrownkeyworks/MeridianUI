#pragma once

#include "../PCH.h"
#include "CallSiteGuard.h"

namespace Meridian::Hooks
{
    class ShutdownHook
    {
    public:
        static inline std::atomic_bool IsGameClosing{false};
        static inline sigslot::signal<> OnShutdown;

        static void Shutdown()
        {
            bool expected = false;
            if (IsGameClosing.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
            {
                OnShutdown();
            }
            _Shutdown();
        }

        static inline REL::Relocation<decltype(&Shutdown)> _Shutdown;

        static bool Install()
        {
            try
            {
                // Call to Main::Shutdown inside WinMain (SE: 35545+0x35, AE: 36544+0x1AE)
                static REL::Relocation<std::uintptr_t> target{RELOCATION_ID(35545, 36544), REL::VariantOffset(0x35, 0x1AE, 0)};
                if (!IsExpectedCallSite(target.address(), CallEncoding::Relative5))
                {
                    spdlog::error(
                        "{}: install refused at {:X}: expected E8 rel32 call for Skyrim {}",
                        NameOf(ShutdownHook),
                        target.address(),
                        REL::Module::get().version().string());
                    return false;
                }
                auto& trampoline = SKSE::GetTrampoline();
                _Shutdown = trampoline.write_call<5>(target.address(), &Shutdown); // Main::Shutdown
                s_installed.store(true, std::memory_order_release);
                spdlog::info("{}: installed at {:X}", NameOf(ShutdownHook), target.address());
                return true;
            }
            catch (const std::exception& e)
            {
                spdlog::error(
                    "{}: install FAILED ({}) - platform initialization will be refused",
                    NameOf(ShutdownHook),
                    e.what());
                return false;
            }
        }

        static bool IsInstalled()
        {
            return s_installed.load(std::memory_order_acquire);
        }

    private:
        static inline std::atomic_bool s_installed{false};
    };
}
