#include "CEFCpuRenderLayer.h"
#include <array>

namespace Meridian::Render
{
    CEFCpuRenderLayer::CEFCpuRenderLayer(std::shared_ptr<Menus::LayerGeometryHolder> geometry)
        : m_geometry(std::move(geometry))
    {
        ThrowIfNullptr(CEFCpuRenderLayer, m_geometry);
    }

    void CEFCpuRenderLayer::Prepare()
    {
        if (m_stopped.load() || !m_isVisible.load() || !m_renderData) return;
        for (auto* surface : {&m_viewSurface, &m_popupSurface})
        {
            const auto hr = surface->UploadLatest(m_renderData->device, m_renderData->deviceContext);
            std::uint32_t suppressed = 0;
            if (FAILED(hr) && m_uploadErrors.ShouldLog(suppressed))
                LOG_ERROR("CpuUpload: texture upload failed ({:#010x}, {} suppressed)", std::uint32_t(hr), suppressed);
        }
    }

    void CEFCpuRenderLayer::Draw()
    {
        if (m_stopped.load() || !m_isVisible.load() || !m_renderData) return;
        auto* view = m_viewSurface.View();
        if (!view) return;
        const auto geometry = m_geometry->Get();
        const RECT destination{geometry.x, geometry.y, geometry.x + geometry.width, geometry.y + geometry.height};
        m_renderData->spriteBatch->Draw(view, destination, ::DirectX::Colors::White);
        std::lock_guard lock(m_popupMutex);
        if (m_popupVisible)
        {
            if (auto* popup = m_popupSurface.View())
            {
                const auto mapped = Menus::CompositorMath::BrowserRectToScreen(
                    geometry, m_popupRect.x, m_popupRect.y, m_popupRect.width, m_popupRect.height);
                const RECT popupDestination{mapped.left, mapped.top, mapped.right, mapped.bottom};
                m_renderData->spriteBatch->Draw(popup, popupDestination, ::DirectX::Colors::White);
            }
        }
    }

    void CEFCpuRenderLayer::GetViewRect(CefRefPtr<CefBrowser>, CefRect& rect)
    {
        const auto geometry = m_geometry->Get();
        rect.Set(0, 0, Menus::CompositorMath::LogicalWidth(geometry), Menus::CompositorMath::LogicalHeight(geometry));
    }

    void CEFCpuRenderLayer::OnPaint(CefRefPtr<CefBrowser>, PaintElementType type, const RectList& dirtyRects,
                                   const void* buffer, int width, int height)
    {
        if (m_stopped.load()) return;
        // A fragmented paint falls back to one full copy without allocating in
        // the CEF callback. Submit treats an empty dirty list as a full update.
        std::array<PixelRect, 64> dirty;
        const auto count = dirtyRects.size() <= dirty.size() ? dirtyRects.size() : 0;
        for (std::size_t i = 0; i < count; ++i)
            dirty[i] = {dirtyRects[i].x, dirtyRects[i].y, dirtyRects[i].width, dirtyRects[i].height};
        auto& surface = type == PET_POPUP ? m_popupSurface : m_viewSurface;
        std::uint32_t suppressed = 0;
        if (!surface.Frames().Submit(buffer, width, height, std::span(dirty).first(count)) && m_paintErrors.ShouldLog(suppressed))
            LOG_WARN("CpuUpload: rejected {}x{} paint ({} suppressed)", width, height, suppressed);
    }

    void CEFCpuRenderLayer::OnPopupShow(CefRefPtr<CefBrowser>, bool show)
    {
        std::lock_guard lock(m_popupMutex);
        m_popupSurface.Frames().Reset();
        m_popupVisible = show;
    }

    void CEFCpuRenderLayer::OnPopupSize(CefRefPtr<CefBrowser>, const CefRect& rect)
    {
        std::lock_guard lock(m_popupMutex);
        m_popupRect = rect;
    }

    void CEFCpuRenderLayer::BeginShutdown()
    {
        m_stopped.store(true);
        IRenderLayer::BeginShutdown();
        m_viewSurface.Frames().Stop();
        m_popupSurface.Frames().Stop();
    }
}
