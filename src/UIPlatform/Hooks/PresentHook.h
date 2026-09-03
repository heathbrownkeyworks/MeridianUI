#pragma once

#include "PCH.h"

namespace Meridian::Hooks
{
    /// <summary>
    /// Game render-loop call at SE id 75461 / AE id 77246, offset 0x9 —
    /// the frame is complete when it returns, so drawing after the original
    /// call lands on top of everything (same site and order as shipping
    /// overlay frameworks). VR is unsupported.
    /// </summary>
    class PresentHook
    {
    public:
        static bool Install();
        static bool IsInstalled();

    protected:
        static void Detour(std::uint32_t a_p1);

        static inline REL::Relocation<void __fastcall(std::uint32_t)> s_original;
        static inline std::atomic_bool s_installed{false};
    };
}
