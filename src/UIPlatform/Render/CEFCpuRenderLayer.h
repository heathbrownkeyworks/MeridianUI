#pragma once

#include "IRenderLayer.h"
#include "CpuTextureSurface.h"
#include "LogThrottle.h"
#include "Menus/LayerGeometryHolder.h"

namespace Meridian::Render
{
    class CEFCpuRenderLayer : public IRenderLayer, public CefRenderHandler
    {
        IMPLEMENT_REFCOUNTING(CEFCpuRenderLayer);

    public:
        explicit CEFCpuRenderLayer(std::shared_ptr<Menus::LayerGeometryHolder> geometry);
        void Prepare() override;
        void Draw() override;
        const char* GetName() override { return "CpuUpload"; }
        void BeginShutdown() override;
        void GetViewRect(CefRefPtr<CefBrowser>, CefRect& rect) override;
        void OnPaint(CefRefPtr<CefBrowser>, PaintElementType type, const RectList& dirtyRects,
                     const void* buffer, int width, int height) override;
        void OnPopupShow(CefRefPtr<CefBrowser>, bool show) override;
        void OnPopupSize(CefRefPtr<CefBrowser>, const CefRect& rect) override;

    private:
        std::shared_ptr<Menus::LayerGeometryHolder> m_geometry;
        CpuTextureSurface m_viewSurface, m_popupSurface;
        std::atomic_bool m_stopped{false};
        std::mutex m_popupMutex;
        CefRect m_popupRect{};
        bool m_popupVisible = false;
        LogThrottle m_uploadErrors; // render thread only
        LogThrottle m_paintErrors;  // CEF UI thread only
    };
}
