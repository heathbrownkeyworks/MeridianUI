#pragma once

#include "PCH.h"
#include "IRenderLayer.h"
#include "Common/SpinLock.h"

namespace Meridian::Render
{
    class CEFCopyRenderLayer : public IRenderLayer,
                               public CefRenderHandler
    {
        IMPLEMENT_REFCOUNTING(CEFCopyRenderLayer);

    protected:
        HANDLE m_sharedTextureHandle = nullptr;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> m_cefTexture;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_cefSRV;
        Microsoft::WRL::ComPtr<ID3D11Device1> m_device1 = nullptr;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_deferredContext;
        std::atomic_bool m_isClosing{false};

    public:
        ~CEFCopyRenderLayer() override = default;

        // IRenderLayer
        virtual void Init(RenderData* a_renderData) override;
        virtual void Draw() override;
        virtual const char* GetName() override;
        void BeginShutdown() override;

        // CefRenderHandler
        virtual void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override;
        virtual void OnPaint(CefRefPtr<CefBrowser> browser,
                             PaintElementType type,
                             const RectList& dirtyRects,
                             const void* buffer,
                             int width,
                             int height) override;
        virtual void OnAcceleratedPaint(CefRefPtr<CefBrowser> browser,
                                        PaintElementType type,
                                        const RectList& dirtyRects,
                                        const CefAcceleratedPaintInfo& info) override;
    };
}
