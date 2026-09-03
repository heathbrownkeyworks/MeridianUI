#include "Render/NifPreviewRenderer.h"

#include "Render/NifCameraMath.h"
#include "Render/NifLighting.h"
#include "Render/NifSceneComposition.h"
#include "Render/NifWeightInterpolation.h"
#include "Render/NifTexturePath.h"
#include "Render/SkyrimNifExtractor.h"
#include "Render/SkyrimTextureLoader.h"

#include "NifMaterialPS.h"
#include "NifMaterialVS.h"

#include <algorithm>
#include <cmath>
#include <directxtk/CommonStates.h>
#include <directxtk/DDSTextureLoader.h>
#include <DirectXMath.h>
#include <cstddef>
#include <iterator>
#include <limits>
#include <numbers>
#include <vector>

namespace Meridian::Render::NifPreview
{
    namespace
    {
        constexpr std::uint32_t MIN_TARGET_SIZE = 64;
        constexpr std::uint32_t MAX_TARGET_SIZE = 2048;
        constexpr Meridian::UI::NifScene::ObjectHandle LEGACY_OBJECT_HANDLE =
            std::numeric_limits<Meridian::UI::NifScene::ObjectHandle>::max();

        constexpr std::uint32_t MATERIAL_NORMAL_MAP = 0x01u;
        constexpr std::uint32_t MATERIAL_SPECULAR = 0x02u;
        constexpr std::uint32_t MATERIAL_SPECULAR_MAP = 0x04u;
        constexpr std::uint32_t MATERIAL_ENVIRONMENT_MAP = 0x08u;
        constexpr std::uint32_t MATERIAL_ALPHA_TEST = 0x10u;
        constexpr std::uint32_t MATERIAL_PREMULTIPLIED_ALPHA = 0x20u;
        constexpr std::uint32_t MATERIAL_MODEL_SPACE_NORMAL = 0x40u;
        constexpr std::uint32_t MATERIAL_FACEGEN = 0x80u;
        constexpr std::uint32_t MATERIAL_FACEGEN_RGB_TINT = 0x100u;

        struct GpuVertex
        {
            ::DirectX::XMFLOAT3 position;
            ::DirectX::XMFLOAT3 normal;
            ::DirectX::XMFLOAT4 tangent;
            ::DirectX::XMFLOAT2 textureCoordinate;
        };

        struct alignas(16) SceneConstants
        {
            ::DirectX::XMFLOAT4X4 worldViewProjection;
            ::DirectX::XMFLOAT4 cameraPosition;
            ::DirectX::XMFLOAT4 ambientTop;
            ::DirectX::XMFLOAT4 ambientBottom;
            ::DirectX::XMFLOAT4 lightDirection[3];
            ::DirectX::XMFLOAT4 lightColor[3];
            ::DirectX::XMFLOAT4 postProcessParams;
        };

        struct alignas(16) MaterialConstants
        {
            ::DirectX::XMFLOAT4 specularColorPower;
            ::DirectX::XMFLOAT4 materialParams;
            ::DirectX::XMFLOAT4 tintColor;
            std::uint32_t materialFlags = 0;
            std::uint32_t padding[3]{};
        };

        static_assert(sizeof(SceneConstants) % 16 == 0);
        static_assert(sizeof(MaterialConstants) % 16 == 0);

        std::uint32_t ClampTargetSize(std::uint32_t a_size)
        {
            return std::clamp(a_size, MIN_TARGET_SIZE, MAX_TARGET_SIZE);
        }
    }

    NifPreviewRenderer::NifPreviewRenderer() = default;

    NifPreviewRenderer::~NifPreviewRenderer()
    {
        BeginShutdown();
        ResetModelResources();
        m_depthStencilView.Reset();
        m_depthTexture.Reset();
        m_renderTargetView.Reset();
        m_colorTexture.Reset();
        m_inputLayout.Reset();
        m_vertexShader.Reset();
        m_pixelShader.Reset();
        m_sceneConstantBuffer.Reset();
        m_materialConstantBuffer.Reset();
        m_fallbackWhiteTexture.Reset();
        m_fallbackNormalTexture.Reset();
        m_states.reset();
        m_platformDevice.reset();
    }

    void NifPreviewRenderer::BeginLoad(std::uint64_t a_generation)
    {
        if (m_isShuttingDown.load(std::memory_order_acquire))
        {
            return;
        }
        {
            std::lock_guard lock(m_pendingMutex);
            if (m_isShuttingDown.load(std::memory_order_acquire))
            {
                return;
            }
            m_requestedGeneration.store(a_generation, std::memory_order_release);
            m_pendingSceneBatch = {};
            m_hasPendingSceneBatch = false;
            m_clearPending = false;
            m_pendingVisibility.clear();
            m_status.store(Meridian::UI::NifView::Status::Loading, std::memory_order_release);
        }
    }

    void NifPreviewRenderer::SubmitScene(std::uint64_t a_generation,
                                         RE::NiPointer<RE::NiNode> a_scene,
                                         bool a_frameOnLoad)
    {
        std::vector<SceneSubmissionObject> scenes;
        scenes.push_back({
            .object = LEGACY_OBJECT_HANDLE,
            .scene = std::move(a_scene),
            .visible = true,
        });
        SubmitSceneBatch(a_generation, std::move(scenes), a_frameOnLoad);
    }

    void NifPreviewRenderer::SubmitSceneBatch(
        std::uint64_t a_generation,
        std::vector<SceneSubmissionObject> a_scenes,
        bool a_frameOnLoad)
    {
        if (m_isShuttingDown.load(std::memory_order_acquire) || a_scenes.empty() ||
            a_scenes.size() > MAX_RESOLVED_SCENE_PARTS ||
            a_generation != m_requestedGeneration.load(std::memory_order_acquire))
        {
            return;
        }
        for (const auto& scene : a_scenes)
        {
            if (scene.object == Meridian::UI::NifScene::INVALID_OBJECT_HANDLE ||
                scene.scene == nullptr ||
                (scene.weightScene != nullptr &&
                 (!std::isfinite(scene.weight) || scene.weight < 0.0f ||
                  scene.weight > 1.0f)))
            {
                return;
            }
        }
        std::lock_guard lock(m_pendingMutex);
        if (a_generation != m_requestedGeneration.load(std::memory_order_acquire))
        {
            return;
        }
        m_pendingSceneBatch = {a_generation, std::move(a_scenes), a_frameOnLoad};
        m_hasPendingSceneBatch = true;
        m_clearPending = false;
    }

    void NifPreviewRenderer::FailLoad(std::uint64_t a_generation,
                                      Meridian::UI::NifView::Status a_status)
    {
        if (m_isShuttingDown.load(std::memory_order_acquire) ||
            a_generation != m_requestedGeneration.load(std::memory_order_acquire))
        {
            return;
        }
        {
            std::lock_guard lock(m_pendingMutex);
            if (m_isShuttingDown.load(std::memory_order_acquire) ||
                a_generation != m_requestedGeneration.load(std::memory_order_acquire))
            {
                return;
            }
            m_pendingSceneBatch = {};
            m_hasPendingSceneBatch = false;
            m_clearPending = true;
            m_pendingVisibility.clear();
            m_status.store(a_status, std::memory_order_release);
        }
    }

    void NifPreviewRenderer::Clear(std::uint64_t a_generation)
    {
        if (m_isShuttingDown.load(std::memory_order_acquire))
        {
            return;
        }
        {
            std::lock_guard lock(m_pendingMutex);
            if (m_isShuttingDown.load(std::memory_order_acquire))
            {
                return;
            }
            m_requestedGeneration.store(a_generation, std::memory_order_release);
            m_pendingSceneBatch = {};
            m_hasPendingSceneBatch = false;
            m_clearPending = true;
            m_pendingVisibility.clear();
            m_status.store(Meridian::UI::NifView::Status::Empty, std::memory_order_release);
        }
    }

    void NifPreviewRenderer::BeginShutdown()
    {
        if (m_isShuttingDown.exchange(true, std::memory_order_acq_rel))
        {
            return;
        }
        {
            std::lock_guard lock(m_pendingMutex);
            m_pendingSceneBatch = {};
            m_hasPendingSceneBatch = false;
            m_clearPending = true;
            m_pendingVisibility.clear();
        }
        m_transport.RequestStop();
        m_status.store(Meridian::UI::NifView::Status::ShuttingDown, std::memory_order_release);
    }

    Meridian::UI::NifView::Status NifPreviewRenderer::GetStatus() const
    {
        return m_status.load(std::memory_order_acquire);
    }

    bool NifPreviewRenderer::SetCamera(const Meridian::UI::NifView::CameraState& a_camera)
    {
        if (m_isShuttingDown.load(std::memory_order_acquire))
        {
            return false;
        }
        {
            std::lock_guard lock(m_cameraMutex);
            m_camera = a_camera;
        }
        m_cameraDirty.store(true, std::memory_order_release);
        return true;
    }

    bool NifPreviewRenderer::SetObjectVisible(
        Meridian::UI::NifScene::ObjectHandle a_object,
        bool a_visible)
    {
        if (m_isShuttingDown.load(std::memory_order_acquire) ||
            a_object == Meridian::UI::NifScene::INVALID_OBJECT_HANDLE)
        {
            return false;
        }
        std::lock_guard lock(m_pendingMutex);
        m_pendingVisibility[a_object] = a_visible;
        return true;
    }

    bool NifPreviewRenderer::FrameModel()
    {
        if (m_isShuttingDown.load(std::memory_order_acquire) ||
            m_status.load(std::memory_order_acquire) != Meridian::UI::NifView::Status::Ready)
        {
            return false;
        }
        {
            std::lock_guard lock(m_cameraMutex);
            m_camera.distanceScale = 1.0f;
            m_camera.panX = 0.0f;
            m_camera.panY = 0.0f;
            m_camera.panZ = 0.0f;
        }
        m_cameraDirty.store(true, std::memory_order_release);
        return true;
    }

    void NifPreviewRenderer::Prepare(RenderData& a_renderData,
                                     std::uint32_t a_width,
                                     std::uint32_t a_height)
    {
        if (m_isShuttingDown.load(std::memory_order_acquire))
        {
            return;
        }

        const auto targetWidth = ClampTargetSize(a_width);
        const auto targetHeight = ClampTargetSize(a_height);

        PendingSceneBatch pending{};
        bool hasPending = false;
        bool clearPending = false;
        std::unordered_map<Meridian::UI::NifScene::ObjectHandle, bool> pendingVisibility;
        {
            std::lock_guard lock(m_pendingMutex);
            clearPending = m_clearPending;
            m_clearPending = false;
            if (m_hasPendingSceneBatch)
            {
                pending = std::move(m_pendingSceneBatch);
                m_pendingSceneBatch = {};
                m_hasPendingSceneBatch = false;
                hasPending = true;
            }
            pendingVisibility.swap(m_pendingVisibility);
        }

        if (clearPending)
        {
            ResetModelResources();
        }

        if (hasPending &&
            pending.generation == m_requestedGeneration.load(std::memory_order_acquire))
        {
            std::vector<SceneMeshInput> sceneMeshes;
            sceneMeshes.reserve(pending.scenes.size());
            std::size_t acceptedShapes = 0;
            std::size_t skippedShapes = 0;
            std::size_t matchedTextureOverrides = 0;
            std::size_t matchedMaterialTintOverrides = 0;
            std::size_t reconstructedExternalBones = 0;
            std::size_t decodedDynamicShapes = 0;
            std::size_t sceneVertices = 0;
            std::size_t sceneIndices = 0;
            const SceneCompositionLimits sceneLimits{};
            auto failure = ExtractionStatus::Success;
            auto weightFailure = WeightInterpolationError::None;
            auto weightTopologyMismatch = WeightTopologyMismatch::None;
            std::size_t weightMismatchIndex = 0;
            const auto logExtractionFailure = [](const SceneSubmissionObject& a_scene,
                                                 std::string_view a_endpoint,
                                                 std::string_view a_path,
                                                 const ExtractionResult& a_result) {
                spdlog::warn(
                    "{}: scene object {} endpoint {} path '{}' extraction status {}, accepted {} shapes, skipped {}, matched {} texture overrides, matched {} actor tint overrides, reconstructed {} external bones, decoded {} dynamic shapes, vertices {}, indices {}, first failed shape '{}'",
                    NameOf(NifPreviewRenderer),
                    a_scene.object,
                    a_endpoint,
                    a_path,
                    static_cast<int>(a_result.status),
                    a_result.acceptedShapes,
                    a_result.skippedShapes,
                    a_result.matchedTextureOverrides,
                    a_result.matchedMaterialTintOverrides,
                    a_result.reconstructedExternalBones,
                    a_result.decodedDynamicShapes,
                    a_result.mesh.vertices.size(),
                    a_result.mesh.indices.size(),
                    a_result.firstFailedShapeName.empty() ? "<none>" :
                        a_result.firstFailedShapeName);
            };
            for (auto& scene : pending.scenes)
            {
                ExtractionResult extracted{};
                if (!scene.liveActorRoots.empty())
                {
                    std::vector<RE::NiAVObject*> liveActorRoots;
                    liveActorRoots.reserve(scene.liveActorRoots.size());
                    for (const auto& root : scene.liveActorRoots)
                    {
                        liveActorRoots.push_back(root.get());
                    }
                    extracted = ExtractLiveActorMesh(
                        scene.scene.get(),
                        liveActorRoots,
                        a_renderData.device,
                        a_renderData.deviceContext);
                    const auto countMaterial = [&](PreviewMaterialFeature a_feature) {
                        return std::count_if(
                            extracted.mesh.draws.begin(),
                            extracted.mesh.draws.end(),
                            [a_feature](const PreviewDrawRange& a_draw) {
                                return a_draw.material.feature == a_feature;
                            });
                    };
                    const auto modelSpaceNormals = std::count_if(
                        extracted.mesh.draws.begin(),
                        extracted.mesh.draws.end(),
                        [](const PreviewDrawRange& a_draw) {
                            return a_draw.material.modelSpaceNormal;
                        });
                    const auto incompleteFaceGenTextures = std::count_if(
                        extracted.mesh.draws.begin(),
                        extracted.mesh.draws.end(),
                        [](const PreviewDrawRange& a_draw) {
                            return a_draw.material.feature ==
                                       PreviewMaterialFeature::FaceGen &&
                                   (a_draw.material.faceTintTexturePath.empty() ||
                                    a_draw.material.faceDetailTexturePath.empty());
                        });
                    spdlog::info(
                        "{}: live actor {:08X} accepted {} live shapes ({} skipped) from {} selected roots; FaceGen {}, RGB tint {}, hair tint {}, model-space normals {}, incomplete FaceGen texture sets {}",
                        NameOf(NifPreviewRenderer),
                        scene.liveActorFormID,
                        extracted.acceptedShapes,
                        extracted.skippedShapes,
                        scene.liveActorRoots.size(),
                        countMaterial(PreviewMaterialFeature::FaceGen),
                        countMaterial(PreviewMaterialFeature::FaceGenRgbTint),
                        countMaterial(PreviewMaterialFeature::HairTint),
                        modelSpaceNormals,
                        incompleteFaceGenTextures);
                }
                else
                {
                    extracted = ExtractModelMesh(
                        scene.scene.get(),
                        a_renderData.device,
                        a_renderData.deviceContext,
                        scene.textureOverrides,
                        &scene.materialTint);
                }
                acceptedShapes += extracted.acceptedShapes;
                skippedShapes += extracted.skippedShapes;
                matchedTextureOverrides += extracted.matchedTextureOverrides;
                matchedMaterialTintOverrides +=
                    extracted.matchedMaterialTintOverrides;
                reconstructedExternalBones += extracted.reconstructedExternalBones;
                decodedDynamicShapes += extracted.decodedDynamicShapes;
                if (!scene.textureOverrides.empty() &&
                    extracted.matchedTextureOverrides == 0)
                {
                    spdlog::warn(
                        "{}: scene object {} low endpoint '{}' matched none of its {} alternate textures",
                        NameOf(NifPreviewRenderer),
                        scene.object,
                        scene.lowModelPath,
                        scene.textureOverrides.size());
                }
                if (extracted.status != ExtractionStatus::Success)
                {
                    logExtractionFailure(scene, "low", scene.lowModelPath, extracted);
                    failure = extracted.status;
                    break;
                }
                if (scene.weightScene != nullptr && scene.liveActorRoots.empty())
                {
                    auto high = ExtractModelMesh(
                        scene.weightScene.get(),
                        a_renderData.device,
                        a_renderData.deviceContext,
                        scene.textureOverrides,
                        &scene.materialTint);
                    acceptedShapes += high.acceptedShapes;
                    skippedShapes += high.skippedShapes;
                    matchedTextureOverrides += high.matchedTextureOverrides;
                    matchedMaterialTintOverrides +=
                        high.matchedMaterialTintOverrides;
                    reconstructedExternalBones += high.reconstructedExternalBones;
                    decodedDynamicShapes += high.decodedDynamicShapes;
                    if (!scene.textureOverrides.empty() &&
                        high.matchedTextureOverrides == 0)
                    {
                        spdlog::warn(
                            "{}: scene object {} high endpoint '{}' matched none of its {} alternate textures",
                            NameOf(NifPreviewRenderer),
                            scene.object,
                            scene.highModelPath,
                            scene.textureOverrides.size());
                    }
                    if (high.status != ExtractionStatus::Success)
                    {
                        logExtractionFailure(scene, "high", scene.highModelPath, high);
                        failure = high.status;
                        break;
                    }
                    auto weighted = InterpolateWeightMeshes(
                        extracted.mesh, high.mesh, scene.weight);
                    if (!weighted)
                    {
                        spdlog::warn(
                            "{}: scene object {} weight interpolation failed between low path '{}' and high path '{}' with status {}, topology {} at {}",
                            NameOf(NifPreviewRenderer),
                            scene.object,
                            scene.lowModelPath,
                            scene.highModelPath,
                            static_cast<int>(weighted.error),
                            WeightTopologyMismatchName(weighted.topologyMismatch),
                            weighted.mismatchIndex);
                        weightFailure = weighted.error;
                        weightTopologyMismatch = weighted.topologyMismatch;
                        weightMismatchIndex = weighted.mismatchIndex;
                        failure = ExtractionStatus::MalformedGeometry;
                        break;
                    }
                    extracted.mesh = std::move(weighted.mesh);
                }
                if (extracted.mesh.vertices.size() >
                        sceneLimits.maxVertices - std::min(sceneVertices, sceneLimits.maxVertices) ||
                    extracted.mesh.indices.size() >
                        sceneLimits.maxIndices - std::min(sceneIndices, sceneLimits.maxIndices))
                {
                    failure = ExtractionStatus::MalformedGeometry;
                    break;
                }
                sceneVertices += extracted.mesh.vertices.size();
                sceneIndices += extracted.mesh.indices.size();
                sceneMeshes.push_back({
                    .object = scene.object,
                    .mesh = std::move(extracted.mesh),
                    .visible = scene.visible,
                });
            }

            auto composed = failure == ExtractionStatus::Success ?
                ComposeScene(std::move(sceneMeshes)) : SceneCompositionResult{};
            if (failure == ExtractionStatus::Success && composed &&
                InitializeGraphics(a_renderData, targetWidth, targetHeight) &&
                BuildMeshResources(m_platformDevice->Device(), std::move(composed.mesh)))
            {
                m_sceneVisibility.clear();
                for (const auto& object : composed.objects)
                {
                    m_sceneVisibility.emplace(object.object, object.visible);
                }
                m_status.store(Meridian::UI::NifView::Status::Ready, std::memory_order_release);
                if (pending.frameOnLoad)
                {
                    FrameModel();
                }
                m_cameraDirty.store(true, std::memory_order_release);
                spdlog::info("{}: composed {} objects, extracted {} shapes ({} skipped), matched {} texture overrides, matched {} actor tint overrides, reconstructed {} external bones, decoded {} dynamic shapes, {} indices uploaded",
                             NameOf(NifPreviewRenderer),
                             m_sceneVisibility.size(),
                             acceptedShapes,
                             skippedShapes,
                             matchedTextureOverrides,
                             matchedMaterialTintOverrides,
                             reconstructedExternalBones,
                             decodedDynamicShapes,
                             m_indexCount);
            }
            else
            {
                ResetModelResources();
                const bool unsupported =
                    failure == ExtractionStatus::NoSupportedGeometry ||
                    weightFailure == WeightInterpolationError::TopologyMismatch;
                const auto status = unsupported ?
                    Meridian::UI::NifView::Status::Unsupported :
                    Meridian::UI::NifView::Status::Failed;
                m_status.store(status, std::memory_order_release);
                spdlog::warn("{}: scene composition failed with extraction status {}, weight status {}, weight topology {} at {}, composition status {} ({} shapes skipped)",
                             NameOf(NifPreviewRenderer),
                             static_cast<int>(failure),
                             static_cast<int>(weightFailure),
                             WeightTopologyMismatchName(weightTopologyMismatch),
                             weightMismatchIndex,
                             static_cast<int>(composed.error),
                             skippedShapes);
            }
        }

        bool visibilityChanged = false;
        for (const auto& [object, visible] : pendingVisibility)
        {
            const auto found = m_sceneVisibility.find(object);
            if (found != m_sceneVisibility.end() && found->second != visible)
            {
                found->second = visible;
                visibilityChanged = true;
            }
            else if (found == m_sceneVisibility.end() &&
                     m_status.load(std::memory_order_acquire) ==
                         Meridian::UI::NifView::Status::Loading)
            {
                // A slot can be toggled while its atomic batch is still on the
                // Skyrim task. Preserve the intent until that object arrives;
                // do not overwrite a newer toggle queued after this snapshot.
                std::lock_guard lock(m_pendingMutex);
                m_pendingVisibility.try_emplace(object, visible);
            }
        }
        if (visibilityChanged)
        {
            m_cameraDirty.store(true, std::memory_order_release);
        }

        if (m_status.load(std::memory_order_acquire) != Meridian::UI::NifView::Status::Ready)
        {
            return;
        }

        const bool targetChanged = targetWidth != m_targetWidth || targetHeight != m_targetHeight;
        if (targetChanged || m_cameraDirty.exchange(false, std::memory_order_acq_rel))
        {
            if (!RenderFrame(targetWidth, targetHeight))
            {
                m_status.store(Meridian::UI::NifView::Status::Failed, std::memory_order_release);
            }
        }
    }

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> NifPreviewRenderer::GetSurfaceView()
    {
        return m_transportReady ? m_transport.ConsumeSRV() : nullptr;
    }

    bool NifPreviewRenderer::InitializeGraphics(RenderData& a_renderData,
                                                std::uint32_t a_width,
                                                std::uint32_t a_height)
    {
        if (m_platformDevice != nullptr && m_transportReady)
        {
            return true;
        }
        if (m_graphicsInitializationAttempted)
        {
            return false;
        }
        m_graphicsInitializationAttempted = true;

        const auto platformDevice = a_renderData.platformDevice;
        if (platformDevice == nullptr || !platformDevice->IsValid() ||
            a_renderData.device == nullptr || a_width == 0 || a_height == 0)
        {
            spdlog::error("{}: private render device unavailable", NameOf(NifPreviewRenderer));
            return false;
        }

        try
        {
            auto states = std::make_unique<::DirectX::CommonStates>(platformDevice->Device());
            Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
            Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
            if (FAILED(platformDevice->Device()->CreateVertexShader(
                    g_NifMaterialVS, sizeof(g_NifMaterialVS), nullptr, vertexShader.GetAddressOf())) ||
                FAILED(platformDevice->Device()->CreatePixelShader(
                    g_NifMaterialPS, sizeof(g_NifMaterialPS), nullptr, pixelShader.GetAddressOf())))
            {
                spdlog::error("{}: private-device material shader creation failed",
                              NameOf(NifPreviewRenderer));
                return false;
            }

            constexpr D3D11_INPUT_ELEMENT_DESC inputElements[] = {
                {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, static_cast<UINT>(offsetof(GpuVertex, position)), D3D11_INPUT_PER_VERTEX_DATA, 0},
                {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, static_cast<UINT>(offsetof(GpuVertex, normal)), D3D11_INPUT_PER_VERTEX_DATA, 0},
                {"TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, static_cast<UINT>(offsetof(GpuVertex, tangent)), D3D11_INPUT_PER_VERTEX_DATA, 0},
                {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, static_cast<UINT>(offsetof(GpuVertex, textureCoordinate)), D3D11_INPUT_PER_VERTEX_DATA, 0},
            };
            Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
            if (FAILED(platformDevice->Device()->CreateInputLayout(
                    inputElements,
                    static_cast<UINT>(std::size(inputElements)),
                    g_NifMaterialVS,
                    sizeof(g_NifMaterialVS),
                    inputLayout.GetAddressOf())))
            {
                spdlog::error("{}: private-device input layout creation failed", NameOf(NifPreviewRenderer));
                return false;
            }

            const auto createFallbackTexture = [&](std::uint32_t a_pixel,
                                                   Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& a_view) {
                D3D11_TEXTURE2D_DESC desc{};
                desc.Width = 1;
                desc.Height = 1;
                desc.MipLevels = 1;
                desc.ArraySize = 1;
                desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                desc.SampleDesc.Count = 1;
                desc.Usage = D3D11_USAGE_IMMUTABLE;
                desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                D3D11_SUBRESOURCE_DATA data{};
                data.pSysMem = &a_pixel;
                data.SysMemPitch = sizeof(a_pixel);
                Microsoft::WRL::ComPtr<ID3D11Texture2D> resource;
                return SUCCEEDED(platformDevice->Device()->CreateTexture2D(
                           &desc, &data, resource.GetAddressOf())) &&
                       SUCCEEDED(platformDevice->Device()->CreateShaderResourceView(
                           resource.Get(), nullptr, a_view.GetAddressOf()));
            };

            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> fallbackWhiteTexture;
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> fallbackNormalTexture;
            if (!createFallbackTexture(0xFFFFFFFFu, fallbackWhiteTexture) ||
                !createFallbackTexture(0xFFFF8080u, fallbackNormalTexture))
            {
                spdlog::error("{}: private-device fallback material textures failed",
                              NameOf(NifPreviewRenderer));
                return false;
            }

            const auto createConstantBuffer = [&](UINT a_size,
                                                  Microsoft::WRL::ComPtr<ID3D11Buffer>& a_buffer) {
                D3D11_BUFFER_DESC desc{};
                desc.ByteWidth = a_size;
                desc.Usage = D3D11_USAGE_DEFAULT;
                desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
                return SUCCEEDED(platformDevice->Device()->CreateBuffer(
                    &desc, nullptr, a_buffer.GetAddressOf()));
            };
            Microsoft::WRL::ComPtr<ID3D11Buffer> sceneConstantBuffer;
            Microsoft::WRL::ComPtr<ID3D11Buffer> materialConstantBuffer;
            if (!createConstantBuffer(sizeof(SceneConstants), sceneConstantBuffer) ||
                !createConstantBuffer(sizeof(MaterialConstants), materialConstantBuffer))
            {
                spdlog::error("{}: private-device material constant buffers failed",
                              NameOf(NifPreviewRenderer));
                return false;
            }

            if (!m_transport.Initialize(*platformDevice, a_renderData.device, static_cast<int>(a_width), static_cast<int>(a_height)))
            {
                spdlog::error("{}: cross-device frame transport initialization failed", NameOf(NifPreviewRenderer));
                return false;
            }

            m_platformDevice = platformDevice;
            m_states = std::move(states);
            m_inputLayout = std::move(inputLayout);
            m_vertexShader = std::move(vertexShader);
            m_pixelShader = std::move(pixelShader);
            m_sceneConstantBuffer = std::move(sceneConstantBuffer);
            m_materialConstantBuffer = std::move(materialConstantBuffer);
            m_fallbackWhiteTexture = std::move(fallbackWhiteTexture);
            m_fallbackNormalTexture = std::move(fallbackNormalTexture);
            m_transportReady = true;
        }
        catch (const std::exception& error)
        {
            spdlog::error("{}: DirectXTK initialization failed: {}", NameOf(NifPreviewRenderer), error.what());
            return false;
        }
        return true;
    }

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> NifPreviewRenderer::LoadTextureResource(
        ID3D11Device* a_device,
        std::string_view a_path,
        bool a_srgb,
        bool a_requireCube)
    {
        if (a_device == nullptr || a_path.empty())
        {
            return nullptr;
        }

        std::string normalizedPath;
        if (!NormalizeTextureResourcePath(a_path, normalizedPath))
        {
            spdlog::warn("{}: rejected material texture path '{}'",
                         NameOf(NifPreviewRenderer),
                         a_path);
            return nullptr;
        }
        const auto cacheKey = normalizedPath +
                              (a_srgb ? "|srgb" : "|linear") +
                              (a_requireCube ? "|cube" : "|2d");
        if (const auto cached = m_textureCache.find(cacheKey); cached != m_textureCache.end())
        {
            return cached->second;
        }

        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> textureView;
        const auto resource = ReadSkyrimTexture(normalizedPath);
        if (resource.error == TextureReadError::None)
        {
            ::DirectX::DDS_ALPHA_MODE ddsAlpha = ::DirectX::DDS_ALPHA_MODE_UNKNOWN;
            const auto flags = a_srgb ? ::DirectX::DDS_LOADER_FORCE_SRGB : ::DirectX::DDS_LOADER_IGNORE_SRGB;
            const auto hr = ::DirectX::CreateDDSTextureFromMemoryEx(
                a_device,
                resource.bytes.data(),
                resource.bytes.size(),
                0,
                D3D11_USAGE_DEFAULT,
                D3D11_BIND_SHADER_RESOURCE,
                0,
                0,
                flags,
                nullptr,
                textureView.GetAddressOf(),
                &ddsAlpha);
            if (FAILED(hr))
            {
                spdlog::warn("{}: DDS decode failed for '{}' (HRESULT 0x{:08X})",
                             NameOf(NifPreviewRenderer),
                             normalizedPath,
                             static_cast<unsigned>(hr));
                textureView.Reset();
            }
        }
        else
        {
            spdlog::warn("{}: texture resource '{}' could not be read (status {})",
                         NameOf(NifPreviewRenderer),
                         normalizedPath,
                         static_cast<int>(resource.error));
        }

        if (textureView != nullptr)
        {
            D3D11_SHADER_RESOURCE_VIEW_DESC desc{};
            textureView->GetDesc(&desc);
            const bool isCube =
                desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURECUBE ||
                desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;
            const bool isTwoDimensional =
                desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2D ||
                desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
            if ((a_requireCube && !isCube) || (!a_requireCube && !isTwoDimensional))
            {
                spdlog::warn("{}: texture '{}' has incompatible DDS resource dimension {}",
                             NameOf(NifPreviewRenderer),
                             normalizedPath,
                             static_cast<unsigned>(desc.ViewDimension));
                textureView.Reset();
            }
        }

        m_textureCache.emplace(cacheKey, textureView);
        return textureView;
    }

    bool NifPreviewRenderer::BuildMeshResources(ID3D11Device* a_device, PreviewMesh a_mesh)
    {
        if (a_device == nullptr || a_mesh.Empty() || m_platformDevice == nullptr)
        {
            return false;
        }

        std::vector<GpuVertex> vertices;
        vertices.reserve(a_mesh.vertices.size());
        for (const auto& vertex : a_mesh.vertices)
        {
            vertices.push_back({
                {vertex.position.x, vertex.position.y, vertex.position.z},
                {vertex.normal.x, vertex.normal.y, vertex.normal.z},
                {vertex.tangent.x, vertex.tangent.y, vertex.tangent.z, vertex.tangent.w},
                {vertex.textureCoordinate.x, vertex.textureCoordinate.y},
            });
        }

        if (vertices.size() > std::numeric_limits<UINT>::max() / sizeof(vertices.front()) ||
            a_mesh.indices.size() > std::numeric_limits<UINT>::max() / sizeof(a_mesh.indices.front()))
        {
            return false;
        }

        D3D11_BUFFER_DESC vertexDesc{};
        vertexDesc.ByteWidth = static_cast<UINT>(vertices.size() * sizeof(vertices.front()));
        vertexDesc.Usage = D3D11_USAGE_IMMUTABLE;
        vertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA vertexData{};
        vertexData.pSysMem = vertices.data();

        Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
        if (FAILED(a_device->CreateBuffer(&vertexDesc, &vertexData, vertexBuffer.GetAddressOf())))
        {
            return false;
        }

        D3D11_BUFFER_DESC indexDesc{};
        indexDesc.ByteWidth = static_cast<UINT>(a_mesh.indices.size() * sizeof(a_mesh.indices.front()));
        indexDesc.Usage = D3D11_USAGE_IMMUTABLE;
        indexDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        D3D11_SUBRESOURCE_DATA indexData{};
        indexData.pSysMem = a_mesh.indices.data();

        Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;
        if (FAILED(a_device->CreateBuffer(&indexDesc, &indexData, indexBuffer.GetAddressOf())))
        {
            return false;
        }

        m_vertexBuffer = std::move(vertexBuffer);
        m_indexBuffer = std::move(indexBuffer);
        m_indexCount = static_cast<std::uint32_t>(a_mesh.indices.size());
        m_draws.clear();
        m_textureCache.clear();
        m_draws.reserve(a_mesh.draws.size());
        for (const auto& draw : a_mesh.draws)
        {
            if (draw.indexCount == 0 || draw.startIndex > a_mesh.indices.size() ||
                draw.indexCount > a_mesh.indices.size() - draw.startIndex)
            {
                continue;
            }

            LoadedDrawRange loaded{};
            loaded.sceneObject = draw.sceneObject;
            loaded.startIndex = draw.startIndex;
            loaded.indexCount = draw.indexCount;
            loaded.material = draw.material;

            loaded.diffuseTexture = LoadTextureResource(
                a_device, draw.material.diffuseTexturePath, true, false);
            if (draw.material.tangentSpaceNormal || draw.material.modelSpaceNormal)
            {
                loaded.normalTexture = LoadTextureResource(
                    a_device, draw.material.normalTexturePath, false, false);
            }
            loaded.specularTexture = LoadTextureResource(
                a_device, draw.material.specularTexturePath, false, false);
            if (draw.material.environmentEnabled)
            {
                loaded.environmentTexture = LoadTextureResource(
                    a_device, draw.material.environmentTexturePath, true, true);
                loaded.environmentMaskTexture = LoadTextureResource(
                    a_device, draw.material.environmentMaskTexturePath, false, false);
            }
            if (draw.material.feature == PreviewMaterialFeature::FaceGen)
            {
                loaded.faceTintTexture = LoadTextureResource(
                    a_device, draw.material.faceTintTexturePath, true, false);
                loaded.faceDetailTexture = LoadTextureResource(
                    a_device, draw.material.faceDetailTexturePath, true, false);
            }
            m_draws.push_back(std::move(loaded));
        }
        if (m_draws.empty())
        {
            ResetModelResources();
            return false;
        }
        m_bounds = a_mesh.bounds;
        return true;
    }

    bool NifPreviewRenderer::EnsureTargets(ID3D11Device* a_device,
                                           std::uint32_t a_width,
                                           std::uint32_t a_height)
    {
        if (a_device == nullptr || a_width == 0 || a_height == 0)
        {
            return false;
        }
        if (m_colorTexture != nullptr && a_width == m_targetWidth && a_height == m_targetHeight)
        {
            return true;
        }

        D3D11_TEXTURE2D_DESC colorDesc{};
        colorDesc.Width = a_width;
        colorDesc.Height = a_height;
        colorDesc.MipLevels = 1;
        colorDesc.ArraySize = 1;
        // FrameTransport's shared ring is BGRA8; CopyResource requires an
        // identical source format.
        colorDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        colorDesc.SampleDesc.Count = 1;
        colorDesc.Usage = D3D11_USAGE_DEFAULT;
        colorDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> colorTexture;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTargetView;
        if (FAILED(a_device->CreateTexture2D(&colorDesc, nullptr, colorTexture.GetAddressOf())) ||
            FAILED(a_device->CreateRenderTargetView(colorTexture.Get(), nullptr, renderTargetView.GetAddressOf())))
        {
            return false;
        }

        D3D11_TEXTURE2D_DESC depthDesc = colorDesc;
        depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> depthTexture;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthStencilView;
        if (FAILED(a_device->CreateTexture2D(&depthDesc, nullptr, depthTexture.GetAddressOf())) ||
            FAILED(a_device->CreateDepthStencilView(depthTexture.Get(), nullptr, depthStencilView.GetAddressOf())))
        {
            return false;
        }

        m_colorTexture = std::move(colorTexture);
        m_renderTargetView = std::move(renderTargetView);
        m_depthTexture = std::move(depthTexture);
        m_depthStencilView = std::move(depthStencilView);
        m_targetWidth = a_width;
        m_targetHeight = a_height;
        return true;
    }

    bool NifPreviewRenderer::RenderFrame(std::uint32_t a_width,
                                         std::uint32_t a_height)
    {
        if (m_vertexBuffer == nullptr || m_indexBuffer == nullptr || !m_bounds.valid ||
            m_platformDevice == nullptr || !m_platformDevice->IsValid() || !m_transportReady ||
            !EnsureTargets(m_platformDevice->Device(), a_width, a_height))
        {
            return false;
        }

        Meridian::UI::NifView::CameraState camera{};
        {
            std::lock_guard lock(m_cameraMutex);
            camera = m_camera;
        }

        const Float3 unpannedCenter{
            (m_bounds.minimum.x + m_bounds.maximum.x) * 0.5f,
            (m_bounds.minimum.y + m_bounds.maximum.y) * 0.5f,
            (m_bounds.minimum.z + m_bounds.maximum.z) * 0.5f,
        };
        const Float3 extent{
            (m_bounds.maximum.x - m_bounds.minimum.x) * 0.5f,
            (m_bounds.maximum.y - m_bounds.minimum.y) * 0.5f,
            (m_bounds.maximum.z - m_bounds.minimum.z) * 0.5f,
        };
        const auto radius = std::max(1.0f, std::sqrt(extent.x * extent.x + extent.y * extent.y + extent.z * extent.z));
        constexpr float fov = std::numbers::pi_v<float> / 4.0f;
        const auto aspect = static_cast<float>(a_width) / static_cast<float>(a_height);
        const auto aspectAdjustment = aspect < 1.0f ? 1.0f / aspect : 1.0f;
        const auto distance = radius * 1.25f * aspectAdjustment /
                              std::tan(fov * 0.5f) * camera.distanceScale;
        const auto yaw = camera.yawDegrees * std::numbers::pi_v<float> / 180.0f;
        const auto pitch = camera.pitchDegrees * std::numbers::pi_v<float> / 180.0f;
        const auto basis = BuildCameraBasis(yaw, pitch);
        const auto lighting = BuildLightingRig(
            camera.lightingPreset, camera.exposureStops, basis);
        const auto modelCenter = ApplyNormalizedPan(
            unpannedCenter, radius, basis, camera.panX, camera.panY, camera.panZ);
        const ::DirectX::XMVECTOR target = ::DirectX::XMVectorSet(
            modelCenter.x, modelCenter.y, modelCenter.z, 1.0f);
        const ::DirectX::XMVECTOR eye = ::DirectX::XMVectorSet(
            modelCenter.x + distance * basis.eyeDirection.x,
            modelCenter.y + distance * basis.eyeDirection.y,
            modelCenter.z + distance * basis.eyeDirection.z,
            1.0f);
        const auto view = ::DirectX::XMMatrixLookAtRH(
            eye, target, ::DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f));
        const auto nearPlane = std::max(0.01f, distance - radius * 2.0f);
        const auto farPlane = std::max(nearPlane + 1.0f, distance + radius * 3.0f);
        const auto projection = ::DirectX::XMMatrixPerspectiveFovRH(fov, aspect, nearPlane, farPlane);

        SceneConstants sceneConstants{};
        ::DirectX::XMStoreFloat4x4(
            &sceneConstants.worldViewProjection,
            ::DirectX::XMMatrixMultiply(view, projection));
        sceneConstants.cameraPosition = {
            modelCenter.x + distance * basis.eyeDirection.x,
            modelCenter.y + distance * basis.eyeDirection.y,
            modelCenter.z + distance * basis.eyeDirection.z,
            1.0f,
        };
        sceneConstants.ambientTop = {
            lighting.ambientTop.x, lighting.ambientTop.y, lighting.ambientTop.z, 1.0f};
        sceneConstants.ambientBottom = {
            lighting.ambientBottom.x,
            lighting.ambientBottom.y,
            lighting.ambientBottom.z,
            1.0f,
        };
        for (std::size_t index = 0; index < lighting.lightDirection.size(); ++index)
        {
            const auto& direction = lighting.lightDirection[index];
            const auto& color = lighting.lightColor[index];
            sceneConstants.lightDirection[index] = {
                direction.x, direction.y, direction.z, 0.0f};
            sceneConstants.lightColor[index] = {color.x, color.y, color.z, 1.0f};
        }
        sceneConstants.postProcessParams = {lighting.exposureMultiplier, 0.0f, 0.0f, 0.0f};

        {
            std::lock_guard contextLock(m_platformDevice->ContextMutex());
            auto* context = m_platformDevice->Context();
            const float clearColor[4] = {0.035f, 0.04f, 0.055f, 0.0f};
            context->ClearRenderTargetView(m_renderTargetView.Get(), clearColor);
            context->ClearDepthStencilView(
                m_depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
            ID3D11RenderTargetView* renderTarget = m_renderTargetView.Get();
            context->OMSetRenderTargets(1, &renderTarget, m_depthStencilView.Get());
            D3D11_VIEWPORT viewport{0.0f, 0.0f, static_cast<float>(a_width), static_cast<float>(a_height), 0.0f, 1.0f};
            context->RSSetViewports(1, &viewport);

            context->UpdateSubresource(
                m_sceneConstantBuffer.Get(), 0, nullptr, &sceneConstants, 0, 0);
            ID3D11Buffer* sceneBuffer = m_sceneConstantBuffer.Get();
            ID3D11Buffer* materialBuffer = m_materialConstantBuffer.Get();
            context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
            context->PSSetShader(m_pixelShader.Get(), nullptr, 0);
            context->VSSetConstantBuffers(0, 1, &sceneBuffer);
            context->PSSetConstantBuffers(0, 1, &sceneBuffer);
            context->PSSetConstantBuffers(1, 1, &materialBuffer);
            ID3D11SamplerState* samplers[] = {
                m_states->AnisotropicWrap(),
                m_states->LinearClamp(),
            };
            context->PSSetSamplers(0, static_cast<UINT>(std::size(samplers)), samplers);

            constexpr UINT stride = sizeof(GpuVertex);
            constexpr UINT offset = 0;
            ID3D11Buffer* vertexBuffer = m_vertexBuffer.Get();
            context->IASetInputLayout(m_inputLayout.Get());
            context->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
            context->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
            context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            for (const auto& draw : m_draws)
            {
                const auto visibility = m_sceneVisibility.find(draw.sceneObject);
                if (visibility == m_sceneVisibility.end() || !visibility->second)
                {
                    continue;
                }
                const bool usesAlpha = draw.material.alphaBlend ||
                                       draw.material.opacity < 0.999f;
                context->OMSetBlendState(
                    usesAlpha ? (draw.material.premultipliedAlpha ? m_states->AlphaBlend() : m_states->NonPremultiplied()) : m_states->Opaque(),
                    nullptr,
                    0xFFFFFFFF);
                context->OMSetDepthStencilState(
                    usesAlpha ? m_states->DepthRead() : m_states->DepthDefault(), 0);
                context->RSSetState(
                    draw.material.twoSided ? m_states->CullNone() : m_states->CullCounterClockwise());

                MaterialConstants materialConstants{};
                materialConstants.specularColorPower = {
                    draw.material.specularColor.x,
                    draw.material.specularColor.y,
                    draw.material.specularColor.z,
                    draw.material.specularPower,
                };
                materialConstants.materialParams = {
                    draw.material.opacity,
                    draw.material.specularStrength,
                    draw.material.environmentScale,
                    draw.material.alphaThreshold,
                };
                materialConstants.tintColor = {
                    draw.material.tintColor.x,
                    draw.material.tintColor.y,
                    draw.material.tintColor.z,
                    1.0f,
                };
                if (draw.material.tangentSpaceNormal && draw.normalTexture != nullptr)
                {
                    materialConstants.materialFlags |= MATERIAL_NORMAL_MAP;
                }
                if (draw.material.modelSpaceNormal && draw.normalTexture != nullptr)
                {
                    materialConstants.materialFlags |= MATERIAL_MODEL_SPACE_NORMAL;
                }
                if (draw.material.feature == PreviewMaterialFeature::FaceGen &&
                    draw.faceTintTexture != nullptr && draw.faceDetailTexture != nullptr)
                {
                    materialConstants.materialFlags |= MATERIAL_FACEGEN;
                }
                if (draw.material.feature == PreviewMaterialFeature::FaceGenRgbTint)
                {
                    materialConstants.materialFlags |= MATERIAL_FACEGEN_RGB_TINT;
                }
                if (draw.material.specularEnabled)
                {
                    materialConstants.materialFlags |= MATERIAL_SPECULAR;
                }
                if (draw.specularTexture != nullptr)
                {
                    materialConstants.materialFlags |= MATERIAL_SPECULAR_MAP;
                }
                if (draw.material.environmentEnabled && draw.environmentTexture != nullptr)
                {
                    materialConstants.materialFlags |= MATERIAL_ENVIRONMENT_MAP;
                }
                if (draw.material.alphaTest)
                {
                    materialConstants.materialFlags |= MATERIAL_ALPHA_TEST;
                }
                if (draw.material.premultipliedAlpha)
                {
                    materialConstants.materialFlags |= MATERIAL_PREMULTIPLIED_ALPHA;
                }
                context->UpdateSubresource(
                    m_materialConstantBuffer.Get(), 0, nullptr, &materialConstants, 0, 0);

                ID3D11ShaderResourceView* textures[] = {
                    draw.diffuseTexture != nullptr ? draw.diffuseTexture.Get() : m_fallbackWhiteTexture.Get(),
                    draw.normalTexture != nullptr ? draw.normalTexture.Get() : m_fallbackNormalTexture.Get(),
                    draw.specularTexture != nullptr ? draw.specularTexture.Get() : m_fallbackWhiteTexture.Get(),
                    draw.environmentTexture.Get(),
                    draw.environmentMaskTexture != nullptr ? draw.environmentMaskTexture.Get() : m_fallbackWhiteTexture.Get(),
                    draw.faceTintTexture != nullptr ? draw.faceTintTexture.Get() : m_fallbackWhiteTexture.Get(),
                    draw.faceDetailTexture != nullptr ? draw.faceDetailTexture.Get() : m_fallbackWhiteTexture.Get(),
                };
                context->PSSetShaderResources(
                    0, static_cast<UINT>(std::size(textures)), textures);
                context->DrawIndexed(draw.indexCount, draw.startIndex, 0);
            }

            ID3D11ShaderResourceView* nullTextures[7]{};
            context->PSSetShaderResources(0, static_cast<UINT>(std::size(nullTextures)), nullTextures);

            // The source must no longer be bound for output when the transport
            // copies it into the shared ring.
            context->OMSetRenderTargets(0, nullptr, nullptr);
        }

        return m_transport.ProduceFrame(m_colorTexture.Get());
    }

    void NifPreviewRenderer::ResetModelResources()
    {
        m_vertexBuffer.Reset();
        m_indexBuffer.Reset();
        m_indexCount = 0;
        m_draws.clear();
        m_sceneVisibility.clear();
        m_textureCache.clear();
        m_bounds = {};
        m_cameraDirty.store(false, std::memory_order_release);
    }
}
