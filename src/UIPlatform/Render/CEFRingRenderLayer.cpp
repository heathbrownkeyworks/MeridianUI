#include "CEFRingRenderLayer.h"

namespace Meridian::Render
{
    CEFRingRenderLayer::CEFRingRenderLayer(std::shared_ptr<Menus::LayerGeometryHolder> a_geometry)
        : m_geometry(std::move(a_geometry))
    {
        ThrowIfNullptr(CEFRingRenderLayer, m_geometry);
    }

    void CEFRingRenderLayer::Init(RenderData* a_renderData)
    {
        IRenderLayer::Init(a_renderData);

        if (a_renderData->platformDevice == nullptr)
        {
            spdlog::error("{}: no platform device — ring renderer cannot start", NameOf(CEFRingRenderLayer));
            return;
        }

        const auto geometry = m_geometry->Get();
        if (m_transport.Initialize(*a_renderData->platformDevice,
                                   a_renderData->device,
                                   Menus::CompositorMath::LogicalWidth(geometry),
                                   Menus::CompositorMath::LogicalHeight(geometry)))
        {
            m_transportReady.store(true, std::memory_order_release);
        }
        else
        {
            spdlog::error("{}: transport initialization failed", NameOf(CEFRingRenderLayer));
        }

        m_popupTransportReady.store(m_popupTransport.Initialize(*a_renderData->platformDevice, a_renderData->device, 1, 1), std::memory_order_release);
    }

    void CEFRingRenderLayer::Draw()
    {
        if (!m_isVisible.load(std::memory_order_acquire) ||
            !m_transportReady.load(std::memory_order_acquire))
        {
            return;
        }

        const auto geometry = m_geometry->Get();
        const auto srv = m_transport.ConsumeSRV();
        if (srv == nullptr)
        {
            return;
        }

        const RECT destination{geometry.x, geometry.y,
                               geometry.x + geometry.width,
                               geometry.y + geometry.height};
        m_renderData->spriteBatch->Draw(srv.Get(), destination, ::DirectX::Colors::White);

        bool popupVisible = false;
        CefRect popupRect{};
        {
            std::lock_guard lock(m_popupMutex);
            popupVisible = m_popupVisible;
            popupRect = m_popupRect;
        }

        if (popupVisible && m_popupTransportReady.load(std::memory_order_acquire))
        {
            const auto popupSrv = m_popupTransport.ConsumeSRV();
            if (popupSrv != nullptr)
            {
                // Popup rect is in browser-logical coordinates; map back to screen.
                const auto popupScreenRect = Menus::CompositorMath::BrowserRectToScreen(
                    geometry, popupRect.x, popupRect.y, popupRect.width, popupRect.height);
                const RECT popupDestination{popupScreenRect.left, popupScreenRect.top,
                                            popupScreenRect.right, popupScreenRect.bottom};
                m_renderData->spriteBatch->Draw(popupSrv.Get(), popupDestination, ::DirectX::Colors::White);
            }
        }
    }

    void CEFRingRenderLayer::AfterDraw()
    {
        m_transport.ReleaseConsumedFrame();
        m_popupTransport.ReleaseConsumedFrame();
    }

    const char* CEFRingRenderLayer::GetName()
    {
        return "RingBuffer";
    }

    void CEFRingRenderLayer::BeginShutdown()
    {
        IRenderLayer::BeginShutdown();
        m_transport.RequestStop();
        m_popupTransport.RequestStop();
    }

    void CEFRingRenderLayer::GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect)
    {
        if (m_geometry != nullptr)
        {
            const auto geometry = m_geometry->Get();
            rect.Set(0, 0,
                     Menus::CompositorMath::LogicalWidth(geometry),
                     Menus::CompositorMath::LogicalHeight(geometry));
        }
        else
        {
            rect.Set(0, 0, 800, 600);
        }
    }

    void CEFRingRenderLayer::OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type,
                                     const RectList& dirtyRects, const void* buffer,
                                     int width, int height)
    {
        spdlog::error("{}: unexpected software OnPaint", NameOf(CEFRingRenderLayer));
    }

    void CEFRingRenderLayer::OnAcceleratedPaint(CefRefPtr<CefBrowser> browser, PaintElementType type,
                                                const RectList& dirtyRects,
                                                const CefAcceleratedPaintInfo& info)
    {
        if (type == PaintElementType::PET_POPUP)
        {
            if (m_popupTransportReady.load(std::memory_order_acquire) &&
                m_renderData != nullptr && m_renderData->platformDevice != nullptr)
            {
                Microsoft::WRL::ComPtr<ID3D11Texture2D> popupTexture;
                if (SUCCEEDED(m_renderData->platformDevice->Device()->OpenSharedResource1(
                        info.shared_texture_handle, IID_PPV_ARGS(popupTexture.GetAddressOf()))))
                {
                    m_popupTransport.ProduceFrame(popupTexture.Get());
                }
            }
            return;
        }

        if (!m_transportReady.load(std::memory_order_acquire) ||
            m_renderData == nullptr ||
            m_renderData->platformDevice == nullptr)
        {
            return;
        }

        Microsoft::WRL::ComPtr<ID3D11Texture2D> cefTexture;
        const auto hr = m_renderData->platformDevice->Device()->OpenSharedResource1(
            info.shared_texture_handle, IID_PPV_ARGS(cefTexture.GetAddressOf()));
        if (FAILED(hr))
        {
            spdlog::error("{}: OpenSharedResource1 on CEF frame failed, code {:X}", NameOf(CEFRingRenderLayer), hr);
            return;
        }

        m_transport.ProduceFrame(cefTexture.Get());
    }

    void CEFRingRenderLayer::OnPopupShow(CefRefPtr<CefBrowser> browser, bool show)
    {
        std::lock_guard lock(m_popupMutex);
        m_popupVisible = show;
    }

    void CEFRingRenderLayer::OnPopupSize(CefRefPtr<CefBrowser> browser, const CefRect& rect)
    {
        std::lock_guard lock(m_popupMutex);
        m_popupRect = rect;
    }
}
