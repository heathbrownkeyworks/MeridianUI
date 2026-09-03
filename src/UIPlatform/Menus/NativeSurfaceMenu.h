#pragma once

#include "Menus/ISubMenu.h"
#include "Render/NifPreviewRenderer.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <wrl/client.h>

namespace Meridian::Menus
{
    class NativeSurfaceMenu final : public ISubMenu
    {
        IMPLEMENT_REFCOUNTING(NativeSurfaceMenu);

    public:
        NativeSurfaceMenu(int a_x,
                          int a_y,
                          int a_width,
                          int a_height,
                          int a_zOrder,
                          bool a_initiallyVisible);
        ~NativeSurfaceMenu() override;

        void Init(Meridian::Render::RenderData* a_renderData) override;
        void Prepare() override;
        void Draw() override;
        void BeginShutdown() override;

        bool CanProcess(RE::InputEvent*) override;

        SubMenuType GetMenuType() override;
        std::shared_ptr<LayerGeometryHolder> GetGeometryHolder() const override;
        void OnResolutionChanged(int a_oldW, int a_oldH, int a_newW, int a_newH) override;

        bool IsReady() const;

        void BeginNifLoad(std::uint64_t a_generation);
        void SubmitNifScene(std::uint64_t a_generation,
                            RE::NiPointer<RE::NiNode> a_scene,
                            bool a_frameOnLoad);
        void SubmitNifSceneBatch(
            std::uint64_t a_generation,
            std::vector<Meridian::Render::NifPreview::SceneSubmissionObject> a_scenes,
            bool a_frameOnLoad);
        void FailNifLoad(std::uint64_t a_generation, Meridian::UI::NifView::Status a_status);
        void ClearNif(std::uint64_t a_generation);
        Meridian::UI::NifView::Status GetNifStatus() const;
        bool SetNifCamera(const Meridian::UI::NifView::CameraState& a_camera);
        bool SetNifObjectVisible(Meridian::UI::NifScene::ObjectHandle a_object,
                                 bool a_visible);
        bool FrameNif();

    private:
        std::shared_ptr<LayerGeometryHolder> m_geometryHolder;
        mutable std::mutex m_resourceMutex;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> m_texture;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_checkerboardView;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_shaderResourceView;
        Meridian::Render::NifPreview::NifPreviewRenderer m_nifPreviewRenderer;
        std::atomic_bool m_ready{false};
    };
}
