#pragma once

#include "PCH.h"
#include "Config/IniConfig.h"

namespace Meridian::Render
{
    enum class CompositorTarget;
}

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
        static bool Install(Meridian::Config::CompositorTiming a_timing);
        static bool IsInstalled();

    protected:
        static void CompositeSafely(Meridian::Render::CompositorTarget a_target) noexcept;
        static void Detour(std::uint32_t a_p1);

        static inline REL::Relocation<void __fastcall(std::uint32_t)> s_original;
        static inline std::atomic_bool s_installed{false};
        static inline std::atomic<Meridian::Config::CompositorTiming> s_timing{
            Meridian::Config::CompositorTiming::AfterRendererEnd};
    };
}
