#pragma once

#include "PCH.h"
#include "MeridianUIAPI/NifSceneAPI.h"
#include "MeridianUIAPI/NifViewAPI.h"
#include "Render/FrameTransport.h"
#include "Render/NifMaterialTint.h"
#include "Render/NifPreviewMesh.h"
#include "Render/NifTextureOverride.h"
#include "Render/RenderData.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <wrl/client.h>

namespace DirectX
{
    inline namespace DX11
    {
        class CommonStates;
    }
}

namespace Meridian::Render::NifPreview
{
    struct SceneSubmissionObject
    {
        Meridian::UI::NifScene::ObjectHandle object =
            Meridian::UI::NifScene::INVALID_OBJECT_HANDLE;
        RE::NiPointer<RE::NiNode> scene;
        RE::NiPointer<RE::NiNode> weightScene;
        std::vector<RE::NiPointer<RE::NiAVObject>> liveActorRoots;
        std::uint32_t liveActorFormID = 0;
        std::string lowModelPath;
        std::string highModelPath;
        std::vector<ModelTextureOverride> textureOverrides;
        ActorMaterialTintOverride materialTint{};
        float weight = 0.0f;
        bool visible = true;
    };

    class NifPreviewRenderer
    {
    public:
        NifPreviewRenderer();
        ~NifPreviewRenderer();

        NifPreviewRenderer(const NifPreviewRenderer&) = delete;
        NifPreviewRenderer& operator=(const NifPreviewRenderer&) = delete;

        void BeginLoad(std::uint64_t a_generation);
        void SubmitScene(std::uint64_t a_generation,
                         RE::NiPointer<RE::NiNode> a_scene,
                         bool a_frameOnLoad);
        void SubmitSceneBatch(std::uint64_t a_generation,
                              std::vector<SceneSubmissionObject> a_scenes,
                              bool a_frameOnLoad);
        void FailLoad(std::uint64_t a_generation, Meridian::UI::NifView::Status a_status);
        void Clear(std::uint64_t a_generation);
        void BeginShutdown();

        Meridian::UI::NifView::Status GetStatus() const;
        bool SetCamera(const Meridian::UI::NifView::CameraState& a_camera);
        bool SetObjectVisible(Meridian::UI::NifScene::ObjectHandle a_object,
                              bool a_visible);
        bool FrameModel();

        void Prepare(RenderData& a_renderData, std::uint32_t a_width, std::uint32_t a_height);
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetSurfaceView();

    private:
        struct PendingSceneBatch
        {
            std::uint64_t generation = 0;
            std::vector<SceneSubmissionObject> scenes;
            bool frameOnLoad = true;
        };

        struct LoadedDrawRange
        {
            Meridian::UI::NifScene::ObjectHandle sceneObject =
                Meridian::UI::NifScene::INVALID_OBJECT_HANDLE;
            std::uint32_t startIndex = 0;
            std::uint32_t indexCount = 0;
            PreviewMaterial material{};
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> diffuseTexture;
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> normalTexture;
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> specularTexture;
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> environmentTexture;
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> environmentMaskTexture;
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> faceTintTexture;
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> faceDetailTexture;
        };

        bool InitializeGraphics(RenderData& a_renderData,
                                std::uint32_t a_width,
                                std::uint32_t a_height);
        bool BuildMeshResources(ID3D11Device* a_device, PreviewMesh a_mesh);
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> LoadTextureResource(
            ID3D11Device* a_device,
            std::string_view a_path,
            bool a_srgb,
            bool a_requireCube);
        bool EnsureTargets(ID3D11Device* a_device, std::uint32_t a_width, std::uint32_t a_height);
        bool RenderFrame(std::uint32_t a_width, std::uint32_t a_height);
        void ResetModelResources();

        std::atomic_bool m_isShuttingDown{false};
        std::atomic<std::uint64_t> m_requestedGeneration{0};
        std::atomic<Meridian::UI::NifView::Status> m_status{Meridian::UI::NifView::Status::Empty};

        mutable std::mutex m_pendingMutex;
        PendingSceneBatch m_pendingSceneBatch{};
        bool m_hasPendingSceneBatch = false;
        bool m_clearPending = false;
        std::unordered_map<Meridian::UI::NifScene::ObjectHandle, bool> m_pendingVisibility;

        mutable std::mutex m_cameraMutex;
        Meridian::UI::NifView::CameraState m_camera{};
        std::atomic_bool m_cameraDirty{false};

        PreviewBounds m_bounds{};
        std::uint32_t m_indexCount = 0;
        std::uint32_t m_targetWidth = 0;
        std::uint32_t m_targetHeight = 0;

        std::shared_ptr<RenderDevice> m_platformDevice;
        FrameTransport m_transport;
        bool m_transportReady = false;
        bool m_graphicsInitializationAttempted = false;
        std::unique_ptr<::DirectX::CommonStates> m_states;
        Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;
        Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vertexShader;
        Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pixelShader;
        Microsoft::WRL::ComPtr<ID3D11Buffer> m_sceneConstantBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer> m_materialConstantBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer> m_vertexBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer> m_indexBuffer;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_fallbackWhiteTexture;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_fallbackNormalTexture;
        std::vector<LoadedDrawRange> m_draws;
        std::unordered_map<Meridian::UI::NifScene::ObjectHandle, bool> m_sceneVisibility;
        std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>>
            m_textureCache;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> m_colorTexture;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_renderTargetView;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> m_depthTexture;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_depthStencilView;
    };
}
