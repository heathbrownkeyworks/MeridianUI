#include "CEFSyncCopyRenderLayer.h"

namespace Meridian::Render
{
    CEFSyncCopyRenderLayer::CEFSyncCopyRenderLayer(std::shared_ptr<Menus::LayerGeometryHolder> a_geometry)
        : m_geometry(std::move(a_geometry))
    {
        ThrowIfNullptr(CEFSyncCopyRenderLayer, m_geometry);
    }

    void CEFSyncCopyRenderLayer::FinishFrame()
    {
        {
            std::lock_guard lock(m_frameMutex);
            m_acceleratedPaintInfo = nullptr;
            m_frameState = FrameState::Idle;
        }
        m_frameCondition.notify_all();
    }

    void CEFSyncCopyRenderLayer::CopySharedTexture(const CefAcceleratedPaintInfo& a_paintInfo,
                                                   Microsoft::WRL::ComPtr<ID3D11Texture2D>& a_texture,
                                                   Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& a_srv,
                                                   int& a_width,
                                                   int& a_height)
    {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> sharedTexture = nullptr;
        auto hr = m_device1->OpenSharedResource1(a_paintInfo.shared_texture_handle, IID_PPV_ARGS(sharedTexture.ReleaseAndGetAddressOf()));
        FAST_CHECK_HRESULT_LOG_AND_RETURN(hr, "CEFSyncCopyRenderLayer::CopySharedTexture() - OpenSharedResource1()");

        D3D11_TEXTURE2D_DESC sharedTextureDesc = {};
        sharedTexture->GetDesc(&sharedTextureDesc);

        if (a_texture == nullptr ||
            a_width != static_cast<int>(sharedTextureDesc.Width) ||
            a_height != static_cast<int>(sharedTextureDesc.Height))
        {
            hr = m_renderData->device->CreateTexture2D(&sharedTextureDesc, nullptr, a_texture.ReleaseAndGetAddressOf());
            FAST_CHECK_HRESULT_LOG_AND_RETURN(hr, "CEFSyncCopyRenderLayer::CopySharedTexture() - CreateTexture2D()");

            D3D11_SHADER_RESOURCE_VIEW_DESC sharedResourceViewDesc = {};
            sharedResourceViewDesc.Format = sharedTextureDesc.Format;
            sharedResourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            sharedResourceViewDesc.Texture2D.MostDetailedMip = 0;
            sharedResourceViewDesc.Texture2D.MipLevels = 1;

            hr = m_renderData->device->CreateShaderResourceView(a_texture.Get(), &sharedResourceViewDesc, a_srv.ReleaseAndGetAddressOf());
            FAST_CHECK_HRESULT_LOG_AND_RETURN(hr, "CEFSyncCopyRenderLayer::CopySharedTexture() - CreateShaderResourceView()");

            a_width = static_cast<int>(sharedTextureDesc.Width);
            a_height = static_cast<int>(sharedTextureDesc.Height);

            spdlog::info("CEFSyncCopyRenderLayer: texture created");
        }

        D3D11_BOX dirtyRect;
        dirtyRect.left = a_paintInfo.extra.capture_update_rect.x;
        dirtyRect.top = a_paintInfo.extra.capture_update_rect.y;
        dirtyRect.right = a_paintInfo.extra.capture_update_rect.x + a_paintInfo.extra.capture_update_rect.width;
        dirtyRect.bottom = a_paintInfo.extra.capture_update_rect.y + a_paintInfo.extra.capture_update_rect.height;
        dirtyRect.front = 0;
        dirtyRect.back = 1;

        m_renderData->deviceContext->CopySubresourceRegion(a_texture.Get(),
                                                           0,
                                                           a_paintInfo.extra.capture_update_rect.x,
                                                           a_paintInfo.extra.capture_update_rect.y,
                                                           0,
                                                           sharedTexture.Get(),
                                                           0,
                                                           &dirtyRect);
        // The CopyResource call is asynchronous by default
        m_renderData->deviceContext->Flush();

        D3D11_QUERY_DESC queryDesc = {};
        queryDesc.Query = D3D11_QUERY_EVENT;
        queryDesc.MiscFlags = 0;

        Microsoft::WRL::ComPtr<ID3D11Query> query;
        hr = m_device1->CreateQuery(&queryDesc, &query);
        FAST_CHECK_HRESULT_LOG_AND_RETURN(hr, "CEFSyncCopyRenderLayer::CopySharedTexture() - CreateQuery()");

        m_renderData->deviceContext->End(query.Get());

        constexpr auto queryTimeout = 250ms;
        const auto queryDeadline = std::chrono::steady_clock::now() + queryTimeout;
        HRESULT queryResult = S_FALSE;
        while ((queryResult = m_renderData->deviceContext->GetData(query.Get(), nullptr, 0, 0)) == S_FALSE)
        {
            if (m_isClosing.load(std::memory_order_acquire) ||
                Meridian::Hooks::ShutdownHook::IsGameClosing.load(std::memory_order_acquire))
            {
                return;
            }

            const auto removedReason = m_renderData->device->GetDeviceRemovedReason();
            if (FAILED(removedReason))
            {
                spdlog::error("CEFSyncCopyRenderLayer: device removed while waiting for GPU copy, code {:X}", removedReason);
                return;
            }

            if (std::chrono::steady_clock::now() >= queryDeadline)
            {
                spdlog::error("CEFSyncCopyRenderLayer: timed out waiting for GPU copy");
                return;
            }

            std::this_thread::yield();
        }

        if (FAILED(queryResult))
        {
            spdlog::error("CEFSyncCopyRenderLayer: GPU query failed, code {:X}", queryResult);
        }
    }

    void CEFSyncCopyRenderLayer::Init(RenderData* a_renderData)
    {
        IRenderLayer::Init(a_renderData);

        const auto hr = m_renderData->device->QueryInterface(IID_PPV_ARGS(&m_device1));
        if (FAILED(hr))
        {
            spdlog::error("CEFSyncCopyRenderLayer::Init() - failed QueryInterface(), code {:X}", hr);
        }
    }

    void CEFSyncCopyRenderLayer::Draw()
    {
        const CefAcceleratedPaintInfo* paintInfo = nullptr;
        PaintElementType paintType = PaintElementType::PET_VIEW;
        {
            std::lock_guard lock(m_frameMutex);
            if (m_frameState == FrameState::Pending)
            {
                m_frameState = FrameState::Consuming;
                paintInfo = m_acceleratedPaintInfo;
                paintType = m_pendingType;
            }
        }

        if (paintInfo != nullptr)
        {
            try
            {
                if (paintType == PaintElementType::PET_POPUP)
                {
                    CopySharedTexture(*paintInfo, m_popupTexture, m_popupSRV, m_popupWidth, m_popupHeight);
                }
                else
                {
                    CopySharedTexture(*paintInfo, m_cefTexture, m_cefSRV, m_cefWidth, m_cefHeight);
                }
            }
            catch (...)
            {
                FinishFrame();
                throw;
            }
            FinishFrame();
        }

        if (m_isVisible.load(std::memory_order_acquire) && m_cefSRV != nullptr)
        {
            const auto geometry = m_geometry->Get();
            const RECT destination{geometry.x, geometry.y,
                                   geometry.x + geometry.width,
                                   geometry.y + geometry.height};
            m_renderData->spriteBatch->Draw(m_cefSRV.Get(), destination, ::DirectX::Colors::White);

            bool popupVisible = false;
            CefRect popupRect{};
            {
                std::lock_guard lock(m_popupMutex);
                popupVisible = m_popupVisible;
                popupRect = m_popupRect;
            }

            if (popupVisible && m_popupSRV != nullptr)
            {
                // Popup rect is in browser-logical coordinates; map back to screen.
                const auto popupScreenRect = Menus::CompositorMath::BrowserRectToScreen(
                    geometry, popupRect.x, popupRect.y, popupRect.width, popupRect.height);
                const RECT popupDestination{popupScreenRect.left, popupScreenRect.top,
                                            popupScreenRect.right, popupScreenRect.bottom};
                m_renderData->spriteBatch->Draw(m_popupSRV.Get(), popupDestination, ::DirectX::Colors::White);
            }
        }
    }

    const char* CEFSyncCopyRenderLayer::GetName()
    {
        return "SyncCopy";
    }

    void CEFSyncCopyRenderLayer::BeginShutdown()
    {
        IRenderLayer::BeginShutdown();
        m_isClosing.store(true, std::memory_order_release);

        {
            std::lock_guard lock(m_frameMutex);
            if (m_frameState == FrameState::Pending)
            {
                m_acceleratedPaintInfo = nullptr;
                m_frameState = FrameState::Idle;
            }
        }
        m_frameCondition.notify_all();
    }

    void CEFSyncCopyRenderLayer::GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect)
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

    void CEFSyncCopyRenderLayer::OnPaint(CefRefPtr<CefBrowser> browser,
                                         PaintElementType type,
                                         const RectList& dirtyRects,
                                         const void* buffer,
                                         int width,
                                         int height)
    {
        spdlog::error("CEFCopyRenderLayer::OnPaint called");
    }

    void CEFSyncCopyRenderLayer::OnAcceleratedPaint(CefRefPtr<CefBrowser> browser,
                                                    PaintElementType type,
                                                    const RectList& dirtyRects,
                                                    const CefAcceleratedPaintInfo& info)
    {
        if (m_renderData == nullptr ||
            m_device1 == nullptr)
        {
            spdlog::error("CEFSyncCopyRenderLayer::OnAcceleratedPaint() - device or renderData is nullptr");
            return;
        }

        std::unique_lock lock(m_frameMutex);
        if (m_isClosing.load(std::memory_order_acquire) ||
            Meridian::Hooks::ShutdownHook::IsGameClosing.load(std::memory_order_acquire))
        {
            return;
        }

        m_acceleratedPaintInfo = &info;
        m_pendingType = type;
        m_frameState = FrameState::Pending;

        while (m_frameState != FrameState::Idle)
        {
            if ((m_isClosing.load(std::memory_order_acquire) ||
                 Meridian::Hooks::ShutdownHook::IsGameClosing.load(std::memory_order_acquire)) &&
                m_frameState == FrameState::Pending)
            {
                m_acceleratedPaintInfo = nullptr;
                m_frameState = FrameState::Idle;
                break;
            }

            m_frameCondition.wait_for(lock, 10ms);
        }

        lock.unlock();
        m_frameCondition.notify_all();
    }

    void CEFSyncCopyRenderLayer::OnPopupShow(CefRefPtr<CefBrowser> browser, bool show)
    {
        std::lock_guard lock(m_popupMutex);
        m_popupVisible = show;
    }

    void CEFSyncCopyRenderLayer::OnPopupSize(CefRefPtr<CefBrowser> browser, const CefRect& rect)
    {
        std::lock_guard lock(m_popupMutex);
        m_popupRect = rect;
    }
}
