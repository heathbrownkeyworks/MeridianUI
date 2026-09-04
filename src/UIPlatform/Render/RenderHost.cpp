#include "RenderHost.h"

#include "Menus/FocusArbiter.h"
#include "Render/CursorRenderer.h"
#include "Menus/CursorPolicy.h"

namespace Meridian::Render
{
    namespace
    {
        const char* ToString(CompositorTarget a_target) noexcept
        {
            return a_target == CompositorTarget::BoundGameRenderTarget ?
                "BoundGameRenderTarget" :
                "SwapChainBackbuffer";
        }

        Microsoft::WRL::ComPtr<IUnknown> GetComIdentity(IUnknown* a_object) noexcept
        {
            Microsoft::WRL::ComPtr<IUnknown> identity;
            if (a_object != nullptr)
            {
                a_object->QueryInterface(IID_PPV_ARGS(identity.GetAddressOf()));
            }
            return identity;
        }
    }

    RenderHost& RenderHost::GetSingleton()
    {
        static RenderHost singleton;
        return singleton;
    }

    bool RenderHost::Init(std::shared_ptr<spdlog::logger> a_logger)
    {
        if (a_logger == nullptr)
        {
            spdlog::error("{}: has null {}", NameOf(RenderHost), NameOf(a_logger));
            return false;
        }
        m_logger = a_logger;

        // Fill render data
        const auto device = reinterpret_cast<ID3D11Device*>(RE::BSGraphics::Renderer::GetDevice());
        if (device == nullptr)
        {
            m_logger->error("{}: has null {}", NameOf(RenderHost), NameOf(device));
            return false;
        }

        HRESULT hResult = 0;
        hResult = device->QueryInterface<ID3D11Device3>(m_device3.ReleaseAndGetAddressOf());
        if (FAILED(hResult))
        {
            m_logger->error("{}: failed to query interface {}", NameOf(RenderHost), NameOf(ID3D11Device3));
            return false;
        }

        m_device3->GetImmediateContext3(m_immediateContext.ReleaseAndGetAddressOf());
        if (m_immediateContext == nullptr)
        {
            m_logger->error("{}: has null ID3D11DeviceContext3", NameOf(RenderHost));
            return false;
        }

        m_immediateContext->GetDevice(m_gameDevice.ReleaseAndGetAddressOf());
        if (m_gameDevice == nullptr)
        {
            m_logger->error("{}: immediate context returned no native D3D11 device", NameOf(RenderHost));
            return false;
        }

        const auto rendererDeviceIdentity = GetComIdentity(device);
        const auto gameDeviceIdentity = GetComIdentity(m_gameDevice.Get());
        m_logger->info(
            "{}: normalized render device rendererInterface={:p} queriedDevice3={:p} contextDevice={:p} rendererIdentity={:p} contextIdentity={:p} sameIdentity={}",
            NameOf(RenderHost),
            static_cast<void*>(device),
            static_cast<void*>(m_device3.Get()),
            static_cast<void*>(m_gameDevice.Get()),
            static_cast<void*>(rendererDeviceIdentity.Get()),
            static_cast<void*>(gameDeviceIdentity.Get()),
            rendererDeviceIdentity != nullptr && rendererDeviceIdentity.Get() == gameDeviceIdentity.Get());

        const auto nativeMenuRenderData = RE::BSGraphics::Renderer::GetSingleton()->GetRuntimeData().renderTargets[RE::RENDER_TARGETS::kMENUBG];
        D3D11_TEXTURE2D_DESC textDesc;
        reinterpret_cast<ID3D11Texture2D*>(nativeMenuRenderData.texture)->GetDesc(&textDesc);

        m_renderData.device = m_gameDevice.Get();
        m_renderData.deviceContext = m_immediateContext.Get();
        m_renderData.spriteBatch = std::make_shared<::DirectX::SpriteBatch>(m_immediateContext.Get());
        m_renderData.commonStates = std::make_shared<::DirectX::CommonStates>(m_gameDevice.Get());
        m_renderData.width = textDesc.Width;
        m_renderData.height = textDesc.Height;

        // Private device for the ring-buffer frame transport. Failure is
        // non-fatal: clients fall back to the SyncCopy renderer.
        auto platformDevice = std::make_shared<Meridian::Render::RenderDevice>();
        if (platformDevice->Create(m_gameDevice.Get()))
        {
            m_renderData.platformDevice = std::move(platformDevice);
        }
        else
        {
            m_logger->warn("{}: platform render device unavailable, ring renderer disabled", NameOf(RenderHost));
        }

        // CommonLibSSE-NG's RE::BSGraphics::Renderer exposes the swap chain as
        // REX::W32::IDXGISwapChain* (its own COM-layout reimplementation), not
        // the real Windows IDXGISwapChain*. Same pattern as GetDevice() above
        // (which returns REX::W32::ID3D11Device*): reinterpret_cast to the real
        // interface type.
        const auto& runtime = RE::BSGraphics::Renderer::GetSingleton()->GetRuntimeData();
        m_swapChain = reinterpret_cast<IDXGISwapChain*>(runtime.renderWindows[0].swapChain);
        if (m_swapChain == nullptr)
        {
            m_logger->error("{}: no swap chain — cannot render", NameOf(RenderHost));
            return false;
        }
        m_inited.store(true, std::memory_order_release);
        return true;
    }

    bool RenderHost::RefreshSwapChain()
    {
        const auto* renderer = RE::BSGraphics::Renderer::GetSingleton();
        if (renderer == nullptr)
        {
            std::uint32_t suppressed = 0;
            if (m_getBufferFailThrottle.ShouldLog(suppressed))
            {
                m_logger->error("{}: renderer unavailable while refreshing swap chain ({} more suppressed in the last window)", NameOf(RenderHost), suppressed);
            }
            return false;
        }

        const auto& runtime = renderer->GetRuntimeData();
        auto* currentSwapChain = reinterpret_cast<IDXGISwapChain*>(runtime.renderWindows[0].swapChain);
        if (currentSwapChain == nullptr)
        {
            std::uint32_t suppressed = 0;
            if (m_getBufferFailThrottle.ShouldLog(suppressed))
            {
                m_logger->error("{}: current runtime swap chain is null ({} more suppressed in the last window)", NameOf(RenderHost), suppressed);
            }
            return false;
        }

        if (currentSwapChain != m_swapChain.Get())
        {
            auto* previousSwapChain = m_swapChain.Get();
            m_swapChain = currentSwapChain;
            m_targetLogPending = true;
            m_logger->info(
                "{}: runtime swap chain changed {:p} -> {:p}",
                NameOf(RenderHost),
                static_cast<void*>(previousSwapChain),
                static_cast<void*>(currentSwapChain));
        }
        return true;
    }

    bool RenderHost::EnsureBackbufferTarget(Microsoft::WRL::ComPtr<ID3D11Texture2D>& a_backbuffer)
    {
        if (!RefreshSwapChain())
        {
            return false;
        }

        if (FAILED(m_swapChain->GetBuffer(0, IID_PPV_ARGS(a_backbuffer.GetAddressOf()))))
        {
            std::uint32_t suppressed = 0;
            if (m_getBufferFailThrottle.ShouldLog(suppressed))
            {
                m_logger->error("{}: swap chain GetBuffer failed ({} more suppressed in the last window)", NameOf(RenderHost), suppressed);
            }
            return false;
        }

        return ValidateTarget(
            a_backbuffer.Get(),
            CompositorTarget::SwapChainBackbuffer,
            m_swapChain.Get());
    }

    bool RenderHost::EnsureBoundGameTarget(Microsoft::WRL::ComPtr<ID3D11RenderTargetView>& a_targetView)
    {
        m_immediateContext->OMGetRenderTargets(1, a_targetView.ReleaseAndGetAddressOf(), nullptr);
        if (a_targetView == nullptr)
        {
            std::uint32_t suppressed = 0;
            if (m_getBufferFailThrottle.ShouldLog(suppressed))
            {
                m_logger->error("{}: no game render target is bound before renderer end ({} more suppressed in the last window)", NameOf(RenderHost), suppressed);
            }
            return false;
        }

        Microsoft::WRL::ComPtr<ID3D11Resource> resource;
        a_targetView->GetResource(resource.GetAddressOf());
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
        if (resource == nullptr || FAILED(resource.As(&texture)) || texture == nullptr)
        {
            std::uint32_t suppressed = 0;
            if (m_getBufferFailThrottle.ShouldLog(suppressed))
            {
                m_logger->error("{}: bound game render target is not a D3D11 texture ({} more suppressed in the last window)", NameOf(RenderHost), suppressed);
            }
            return false;
        }

        return ValidateTarget(
            texture.Get(),
            CompositorTarget::BoundGameRenderTarget,
            a_targetView.Get());
    }

    bool RenderHost::ValidateTarget(
        ID3D11Texture2D* a_texture,
        CompositorTarget a_target,
        void* a_ownerIdentity)
    {
        D3D11_TEXTURE2D_DESC desc{};
        a_texture->GetDesc(&desc);
        Microsoft::WRL::ComPtr<ID3D11Device> targetDevice;
        a_texture->GetDevice(targetDevice.GetAddressOf());
        const auto targetDeviceIdentity = GetComIdentity(targetDevice.Get());
        const auto renderDeviceIdentity = GetComIdentity(m_renderData.device);
        const bool sameDevice =
            targetDeviceIdentity != nullptr &&
            targetDeviceIdentity.Get() == renderDeviceIdentity.Get();
        const bool targetDescriptionChanged =
            desc.Width != m_lastTargetWidth ||
            desc.Height != m_lastTargetHeight ||
            desc.Format != m_lastTargetFormat;
        const bool targetSourceChanged =
            !m_lastTargetSource.has_value() || *m_lastTargetSource != a_target;

        if (m_targetLogPending || targetDescriptionChanged || targetSourceChanged)
        {
            m_logger->info(
                "{}: compositor target source={} owner={:p} texture={:p} dimensions={}x{} format={} targetDevice={:p} renderDevice={:p} targetIdentity={:p} renderIdentity={:p} sameDevice={}",
                NameOf(RenderHost),
                ToString(a_target),
                a_ownerIdentity,
                static_cast<void*>(a_texture),
                desc.Width,
                desc.Height,
                static_cast<std::uint32_t>(desc.Format),
                static_cast<void*>(targetDevice.Get()),
                static_cast<void*>(m_renderData.device),
                static_cast<void*>(targetDeviceIdentity.Get()),
                static_cast<void*>(renderDeviceIdentity.Get()),
                sameDevice);
            m_lastTargetWidth = desc.Width;
            m_lastTargetHeight = desc.Height;
            m_lastTargetFormat = desc.Format;
            m_lastTargetSource = a_target;
            m_targetLogPending = false;
        }

        if (!sameDevice)
        {
            std::uint32_t suppressed = 0;
            if (m_getBufferFailThrottle.ShouldLog(suppressed))
            {
                m_logger->error("{}: refusing {} from a different D3D11 device ({} more suppressed in the last window)", NameOf(RenderHost), ToString(a_target), suppressed);
            }
            return false;
        }

        if (desc.Width != m_renderData.width || desc.Height != m_renderData.height)
        {
            const int oldW = static_cast<int>(m_renderData.width);
            const int oldH = static_cast<int>(m_renderData.height);
            m_renderData.width = desc.Width;
            m_renderData.height = desc.Height;
            m_logger->info("{}: resolution changed {}x{} -> {}x{}", NameOf(RenderHost), oldW, oldH, desc.Width, desc.Height);
            if (oldW > 0 && oldH > 0)
            {
                m_compositor.ForEach([&](Meridian::Menus::ISubMenu& a_menu) {
                    a_menu.OnResolutionChanged(oldW, oldH, static_cast<int>(desc.Width), static_cast<int>(desc.Height));
                });
            }
        }
        return true;
    }

    void RenderHost::OnPresent(CompositorTarget a_target)
    {
        if (!m_inited.load(std::memory_order_acquire) ||
            m_isShuttingDown.load(std::memory_order_acquire))
        {
            return;
        }

        const auto menus = m_compositor.SortedSnapshot();
        if (menus.empty())
        {
            return;
        }

        Microsoft::WRL::ComPtr<ID3D11Texture2D> backbuffer;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> compositorRTV;
        if (a_target == CompositorTarget::BoundGameRenderTarget)
        {
            if (!EnsureBoundGameTarget(compositorRTV))
            {
                return;
            }
        }
        else
        {
            if (!EnsureBackbufferTarget(backbuffer))
            {
                return;
            }

            // Created fresh every present, never cached on the host: a cached
            // RTV holds an outstanding reference to the backbuffer, and DXGI
            // requires every backbuffer reference released before
            // IDXGISwapChain::ResizeBuffers will succeed.
            if (FAILED(m_renderData.device->CreateRenderTargetView(backbuffer.Get(), nullptr, compositorRTV.GetAddressOf())))
            {
                m_logger->error("{}: CreateRenderTargetView failed", NameOf(RenderHost));
                return;
            }
        }

        // Native layers render their off-screen content before the shared
        // SpriteBatch begins. A failing producer must not prevent Chromium or
        // other already-valid surfaces from being composited this frame.
        for (const auto& subMenu : menus)
        {
            try
            {
                subMenu->Prepare();
            }
            catch (const std::exception& error)
            {
                m_logger->error("{}: native layer Prepare failed: {}", NameOf(RenderHost), error.what());
            }
            catch (...)
            {
                m_logger->error("{}: unknown exception in native layer Prepare", NameOf(RenderHost));
            }
        }

        MERIDIAN_PROBE_SCOPE_BEGIN();

        auto* ctx = m_renderData.deviceContext;

        ID3D11RenderTargetView* savedRTVs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
        ID3D11DepthStencilView* savedDSV = nullptr;
        ctx->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, savedRTVs, &savedDSV);
        UINT savedViewportCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
        D3D11_VIEWPORT savedViewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {};
        ctx->RSGetViewports(&savedViewportCount, savedViewports);

        // Install cleanup before changing any state or beginning SpriteBatch.
        // This closes the former exception window between state capture and
        // guard construction.
        struct PresentStateGuard
        {
            RenderData& renderData;
            ID3D11DeviceContext3* ctx;
            ID3D11RenderTargetView** savedRTVs;
            ID3D11DepthStencilView* savedDSV;
            UINT savedViewportCount;
            D3D11_VIEWPORT* savedViewports;
            const std::vector<std::shared_ptr<Meridian::Menus::ISubMenu>>& menus;
            std::shared_ptr<spdlog::logger> logger;
            bool spriteBatchBegun = false;
            bool drawLockHeld = false;

            ~PresentStateGuard() noexcept
            {
                bool submitted = false;
                if (spriteBatchBegun)
                {
                    try
                    {
                        renderData.spriteBatch->End();
                        submitted = true;
                    }
                    catch (...)
                    {
                        if (logger != nullptr)
                        {
                            logger->error("RenderHost: SpriteBatch::End failed during present cleanup");
                        }
                    }
                }
                if (drawLockHeld)
                {
                    renderData.drawLock.Unlock();
                }

                if (submitted)
                {
                    for (const auto& menu : menus)
                    {
                        try
                        {
                            menu->AfterDraw();
                        }
                        catch (...)
                        {
                            if (logger != nullptr)
                            {
                                logger->error("RenderHost: layer AfterDraw failed during present cleanup");
                            }
                        }
                    }
                }

                ctx->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, savedRTVs, savedDSV);
                for (UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
                {
                    if (savedRTVs[i] != nullptr) savedRTVs[i]->Release();
                }
                if (savedDSV != nullptr) savedDSV->Release();
                ctx->RSSetViewports(savedViewportCount, savedViewports);
            }
        };

        PresentStateGuard stateGuard{
            m_renderData, ctx, savedRTVs, savedDSV, savedViewportCount, savedViewports, menus, m_logger};

        ID3D11RenderTargetView* rtv = compositorRTV.Get();
        ctx->OMSetRenderTargets(1, &rtv, nullptr);
        D3D11_VIEWPORT viewport{0.0f, 0.0f,
                                static_cast<float>(m_renderData.width),
                                static_cast<float>(m_renderData.height),
                                0.0f, 1.0f};
        ctx->RSSetViewports(1, &viewport);

        m_renderData.spriteBatch->Begin(::DirectX::SpriteSortMode_Deferred, m_renderData.commonStates->NonPremultiplied());
        stateGuard.spriteBatchBegun = true;
        m_renderData.drawLock.Lock();
        stateGuard.drawLockHeld = true;

        try
        {
            for (const auto& subMenu : menus)
            {
                subMenu->Draw();
            }

            const auto cursorDecision = Meridian::Menus::CursorPolicy::Evaluate(
                Meridian::Menus::FocusArbiter::GetSingleton().HasOwner());
            if (cursorDecision.drawMeridianCursor)
            {
                Meridian::Render::CursorRenderer::GetSingleton().Draw(m_renderData);
            }

            ctx->Flush1(D3D11_CONTEXT_TYPE::D3D11_CONTEXT_TYPE_COPY, nullptr);
        }
        catch (const std::exception& err)
        {
            m_logger->error("{}: {}", NameOf(RenderHost), err.what());
        }
        catch (...)
        {
            m_logger->error("{}: unknown exception in sub-menu Draw()", NameOf(RenderHost));
        }

        MERIDIAN_PROBE_SCOPE_END(m_perfProbe, menus.size());
    }

    RenderData* RenderHost::GetRenderData()
    {
        return &m_renderData;
    }

    Meridian::Menus::Compositor& RenderHost::GetCompositor()
    {
        return m_compositor;
    }

    bool RenderHost::AddSubMenu(std::string_view a_menuName, std::shared_ptr<Meridian::Menus::ISubMenu> a_subMenu)
    {
        if (m_isShuttingDown.load(std::memory_order_acquire))
        {
            return false;
        }

        if (!m_compositor.Add(a_menuName, a_subMenu))
        {
            return false;
        }

        try
        {
            a_subMenu->Init(&m_renderData);
        }
        catch (...)
        {
            // Do not leave a half-initialized layer discoverable after Init
            // fails. The name was unique when Add succeeded, so this removes
            // exactly the layer registered above.
            std::shared_ptr<Meridian::Menus::ISubMenu> removed;
            if (m_compositor.Remove(std::string(a_menuName), removed) && removed != nullptr)
            {
                removed->BeginShutdown();
            }
            throw;
        }
        return true;
    }

    std::shared_ptr<Meridian::Menus::ISubMenu> RenderHost::GetSubMenu(const std::string& a_menuName)
    {
        return m_compositor.Get(a_menuName);
    }

    bool RenderHost::IsSubMenuExist(const std::string& a_menuName)
    {
        return m_compositor.Get(a_menuName) != nullptr;
    }

    bool RenderHost::RemoveSubMenu(const std::string& a_menuName)
    {
        std::shared_ptr<Meridian::Menus::ISubMenu> removed;
        if (!m_compositor.Remove(a_menuName, removed))
        {
            return false;
        }

        removed->BeginShutdown();
        removed.reset();
        return true;
    }

    void RenderHost::BeginShutdown()
    {
        m_isShuttingDown.store(true, std::memory_order_release);

        std::vector<std::shared_ptr<Meridian::Menus::ISubMenu>> menus;
        m_compositor.Clear(menus);

        for (auto& menu : menus)
        {
            menu->BeginShutdown();
        }
        menus.clear();
    }

    void RenderHost::ClearAllSubMenu()
    {
        std::vector<std::shared_ptr<Meridian::Menus::ISubMenu>> menus;
        m_compositor.Clear(menus);

        for (auto& menu : menus)
        {
            menu->BeginShutdown();
        }
        menus.clear();
    }
}
