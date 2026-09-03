#pragma once

#include "Common/Singleton.h"
#include "MeridianUIAPI/NifViewAPI.h"

#include <atomic>

namespace Meridian::Controllers
{
    class NifViewAPIController final : public Meridian::UI::NifView::INifViewAPI,
                                       public Meridian::Common::Singleton<NifViewAPIController>
    {
        friend class Meridian::Common::Singleton<NifViewAPIController>;

    public:
        bool __cdecl LoadModel(const Meridian::UI::NifView::NifLoadInfo* a_info) override;
        void __cdecl ClearModel(Meridian::UI::RenderLayer::SurfaceHandle a_surface) override;
        Meridian::UI::NifView::Status __cdecl GetStatus(
            Meridian::UI::RenderLayer::SurfaceHandle a_surface) const override;
        bool __cdecl SetCamera(
            Meridian::UI::RenderLayer::SurfaceHandle a_surface,
            const Meridian::UI::NifView::CameraState* a_camera) override;
        bool __cdecl FrameModel(Meridian::UI::RenderLayer::SurfaceHandle a_surface) override;

        void BeginShutdown();
        bool IsShuttingDown() const;

    private:
        NifViewAPIController() = default;

        std::atomic_bool m_isShuttingDown{false};
    };
}
