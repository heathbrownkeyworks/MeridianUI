#pragma once

#include "Common/Singleton.h"
#include "MeridianUIAPI/RenderLayerAPI.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace Meridian::Menus
{
    class NativeSurfaceMenu;
}

namespace Meridian::Controllers
{
    class RenderLayerAPIController final : public Meridian::UI::RenderLayer::IRenderLayerAPI,
                                           public Meridian::Common::Singleton<RenderLayerAPIController>
    {
        friend class Meridian::Common::Singleton<RenderLayerAPIController>;

        struct SurfaceEntry
        {
            Meridian::UI::RenderLayer::SurfaceHandle handle =
                Meridian::UI::RenderLayer::INVALID_SURFACE_HANDLE;
            std::string compositorName;
            std::shared_ptr<Meridian::Menus::NativeSurfaceMenu> surface;
        };

    public:
        Meridian::UI::RenderLayer::SurfaceHandle __cdecl CreateSurface(
            const Meridian::UI::RenderLayer::SurfaceCreateInfo* a_info) override;
        void __cdecl DestroySurface(Meridian::UI::RenderLayer::SurfaceHandle a_surface) override;
        bool __cdecl IsValid(Meridian::UI::RenderLayer::SurfaceHandle a_surface) const override;
        bool __cdecl SetRect(Meridian::UI::RenderLayer::SurfaceHandle a_surface,
                             std::int32_t a_x,
                             std::int32_t a_y,
                             std::int32_t a_width,
                             std::int32_t a_height) override;
        bool __cdecl SetZOrder(Meridian::UI::RenderLayer::SurfaceHandle a_surface,
                               std::int32_t a_zOrder) override;
        bool __cdecl SetVisible(Meridian::UI::RenderLayer::SurfaceHandle a_surface,
                                bool a_visible) override;
        bool __cdecl IsVisible(Meridian::UI::RenderLayer::SurfaceHandle a_surface) const override;

        void BeginShutdown();
        bool IsShuttingDown() const;
        std::shared_ptr<Meridian::Menus::NativeSurfaceMenu> GetNativeSurface(
            Meridian::UI::RenderLayer::SurfaceHandle a_surface) const;

    private:
        RenderLayerAPIController() = default;

        static bool IsSafeName(std::string_view a_name);
        static bool IsValidRect(std::int32_t a_x,
                                std::int32_t a_y,
                                std::int32_t a_width,
                                std::int32_t a_height);
        std::shared_ptr<SurfaceEntry> GetEntry(
            Meridian::UI::RenderLayer::SurfaceHandle a_surface) const;

        mutable std::mutex m_mutex;
        std::unordered_map<Meridian::UI::RenderLayer::SurfaceHandle,
                           std::shared_ptr<SurfaceEntry>> m_surfaces;
        std::unordered_set<std::string> m_ownedCompositorNames;
        std::atomic<Meridian::UI::RenderLayer::SurfaceHandle> m_nextHandle{1};
        std::atomic_bool m_isShuttingDown{false};
    };
}
