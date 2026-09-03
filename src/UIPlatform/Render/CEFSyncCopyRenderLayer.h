#pragma once

#include "PCH.h"
#include "IRenderLayer.h"
#include "Hooks/ShutdownHook.hpp"
#include "Menus/LayerGeometryHolder.h"

#include <condition_variable>

namespace Meridian::Render
{
    class CEFSyncCopyRenderLayer : public IRenderLayer,
                                   public CefRenderHandler
    {
        IMPLEMENT_REFCOUNTING(CEFSyncCopyRenderLayer);

    protected:
        enum class FrameState
        {
            Idle,
            Pending,
            Consuming,
        };

        std::shared_ptr<Menus::LayerGeometryHolder> m_geometry;

        Microsoft::WRL::ComPtr<ID3D11Device1> m_device1 = nullptr;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> m_cefTexture = nullptr;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_cefSRV = nullptr;
        int m_cefWidth = 0;
        int m_cefHeight = 0;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> m_popupTexture = nullptr;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_popupSRV = nullptr;
        int m_popupWidth = 0;
        int m_popupHeight = 0;
        std::mutex m_popupMutex;
        CefRect m_popupRect{};
        bool m_popupVisible = false;

        std::mutex m_frameMutex;
        std::condition_variable m_frameCondition;
        FrameState m_frameState = FrameState::Idle;
        const CefAcceleratedPaintInfo* m_acceleratedPaintInfo = nullptr;
        PaintElementType m_pendingType = PaintElementType::PET_VIEW;
        std::atomic_bool m_isClosing{false};

        void CopySharedTexture(const CefAcceleratedPaintInfo& a_paintInfo,
                               Microsoft::WRL::ComPtr<ID3D11Texture2D>& a_texture,
                               Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& a_srv,
                               int& a_width,
                               int& a_height);
        void FinishFrame();

    public:
        explicit CEFSyncCopyRenderLayer(std::shared_ptr<Menus::LayerGeometryHolder> a_geometry);
        virtual ~CEFSyncCopyRenderLayer() override = default;

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
        virtual void OnPopupShow(CefRefPtr<CefBrowser> browser, bool show) override;
        virtual void OnPopupSize(CefRefPtr<CefBrowser> browser, const CefRect& rect) override;
    };
}
