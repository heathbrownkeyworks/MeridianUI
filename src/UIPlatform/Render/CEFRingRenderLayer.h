#pragma once

#include "PCH.h"
#include "IRenderLayer.h"
#include "FrameTransport.h"
#include "Menus/LayerGeometryHolder.h"

namespace Meridian::Render
{
    /// <summary>
    /// Default renderer: frames arrive on CEF's thread, cross to the game
    /// device through the FrameTransport ring, and Draw only ever samples an
    /// already-completed texture. Popup (dropdown) surfaces ride a second,
    /// independent transport.
    /// </summary>
    class CEFRingRenderLayer : public IRenderLayer,
                               public CefRenderHandler
    {
        IMPLEMENT_REFCOUNTING(CEFRingRenderLayer);

    protected:
        std::shared_ptr<Menus::LayerGeometryHolder> m_geometry;
        FrameTransport m_transport;
        std::atomic_bool m_transportReady{false};
        FrameTransport m_popupTransport;
        std::atomic_bool m_popupTransportReady{false};
        std::mutex m_popupMutex;
        CefRect m_popupRect{};
        bool m_popupVisible = false;

    public:
        explicit CEFRingRenderLayer(std::shared_ptr<Menus::LayerGeometryHolder> a_geometry);
        virtual ~CEFRingRenderLayer() override = default;

        FrameTransport& Transport() { return m_transport; }

        // IRenderLayer
        virtual void Init(RenderData* a_renderData) override;
        virtual void Draw() override;
        void AfterDraw() override;
        virtual const char* GetName() override;
        void BeginShutdown() override;

        // CefRenderHandler
        virtual void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override;
        virtual void OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type,
                             const RectList& dirtyRects, const void* buffer,
                             int width, int height) override;
        virtual void OnAcceleratedPaint(CefRefPtr<CefBrowser> browser, PaintElementType type,
                                        const RectList& dirtyRects,
                                        const CefAcceleratedPaintInfo& info) override;
        virtual void OnPopupShow(CefRefPtr<CefBrowser> browser, bool show) override;
        virtual void OnPopupSize(CefRefPtr<CefBrowser> browser, const CefRect& rect) override;
    };
}
