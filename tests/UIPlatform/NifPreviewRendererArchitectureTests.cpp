#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>

namespace
{
    int g_failures = 0;

    void Expect(bool a_condition, const char* a_message)
    {
        if (!a_condition)
        {
            ++g_failures;
            std::cerr << "FAILED: " << a_message << '\n';
        }
    }

    std::string ReadSource(const std::filesystem::path& a_relativePath)
    {
        const auto path = std::filesystem::path(MERIDIAN_SOURCE_DIR) / a_relativePath;
        std::ifstream stream(path, std::ios::binary);
        std::ostringstream contents;
        contents << stream.rdbuf();
        Expect(stream.good() || stream.eof(), "architecture source file is readable");
        return contents.str();
    }

    bool Contains(std::string_view a_text, std::string_view a_needle)
    {
        return a_text.find(a_needle) != std::string_view::npos;
    }
}

int main()
{
    const auto rendererHeader = ReadSource("src/UIPlatform/Render/NifPreviewRenderer.h");
    const auto rendererSource = ReadSource("src/UIPlatform/Render/NifPreviewRenderer.cpp");
    const auto extractorSource = ReadSource("src/UIPlatform/Render/SkyrimNifExtractor.cpp");
    const auto extractorHeader = ReadSource("src/UIPlatform/Render/SkyrimNifExtractor.h");
    const auto bindPoseHeader = ReadSource("src/UIPlatform/Render/BindPoseSkinning.h");
    const auto materialTintHeader = ReadSource("src/UIPlatform/Render/NifMaterialTint.h");
    const auto sceneController = ReadSource("src/UIPlatform/Controllers/NifSceneAPIController.cpp");
    const auto armorPath = ReadSource("src/UIPlatform/Controllers/NifArmorModelPath.h");
    const auto actorPolicy = ReadSource("src/UIPlatform/Controllers/ActorAppearancePolicy.h");
    const auto viewController = ReadSource("src/UIPlatform/Controllers/NifViewAPIController.cpp");
    const auto loadGeneration = ReadSource("src/UIPlatform/Controllers/NifLoadGeneration.h");
    const auto extensionSource = ReadSource("src/UIPlatform/main.cpp");
    const auto renderDeviceHeader = ReadSource("src/UIPlatform/Render/RenderDevice.h");
    const auto transportSource = ReadSource("src/UIPlatform/Render/FrameTransport.cpp");
    const auto materialShader = ReadSource("src/UIPlatform/Render/Shaders/NifMaterial.hlsl");

    Expect(Contains(rendererHeader, "FrameTransport m_transport"),
           "NIF renderer owns a cross-device frame transport");
    Expect(Contains(rendererSource, "a_renderData.platformDevice"),
           "NIF renderer selects Meridian's private platform device");
    Expect(Contains(rendererSource, "m_transport.ProduceFrame"),
           "NIF renderer publishes its private-device target");
    Expect(Contains(rendererSource, "DXGI_FORMAT_B8G8R8A8_UNORM"),
           "NIF target format matches the transport ring");
    Expect(Contains(renderDeviceHeader, "ContextMutex"),
           "private immediate-context access has a coarse serialization lock");
    Expect(Contains(transportSource, "ContextMutex"),
           "frame copies participate in private-context serialization");
    Expect(Contains(rendererSource, "ReadSkyrimTexture"),
           "diffuse bytes are resolved through Skyrim's virtual resource system");
    Expect(Contains(rendererSource, "CreateDDSTextureFromMemoryEx"),
           "DDS textures are created explicitly on the renderer's private device");
    Expect(Contains(rendererSource, "struct GpuVertex") &&
               Contains(rendererSource, "XMFLOAT4 tangent"),
           "NIF vertices retain tangent space for normal mapping");
    Expect(Contains(rendererSource, "for (const auto& draw : m_draws)"),
           "per-shape material draw ranges are rendered independently");
    Expect(Contains(rendererSource, "DDS_LOADER_FORCE_SRGB") &&
               Contains(rendererSource, "DDS_LOADER_IGNORE_SRGB"),
           "color and data textures use distinct DDS color-space semantics");
    Expect(Contains(rendererSource, "D3D11_SRV_DIMENSION_TEXTURECUBE"),
           "environment resources are validated as cubemaps");
    Expect(Contains(materialShader, "gNormalTexture") &&
               Contains(materialShader, "gSpecularTexture") &&
               Contains(materialShader, "gEnvironmentTexture") &&
               Contains(materialShader, "gEnvironmentMaskTexture"),
           "the Meridian shader combines normal, specular, cubemap, and mask resources");
    Expect(Contains(materialShader, "discard"),
           "alpha-tested materials enforce their NIF threshold in the pixel shader");
    Expect(Contains(rendererSource, "BuildLightingRig") &&
               Contains(materialShader, "gAmbientTop") &&
               Contains(materialShader, "gAmbientBottom"),
           "camera-relative studio lights include hemispheric ambient fill");
    Expect(Contains(materialShader, "ToneMapACES") &&
               Contains(materialShader, "LinearToSrgb") &&
               Contains(materialShader, "gPostProcessParams.x"),
           "linear material lighting is exposed, tone mapped, and display encoded");
    Expect(Contains(extractorSource, "skinInstance->skinPartition") &&
               Contains(extractorSource, "partition.bones") &&
               Contains(extractorSource, "skinPartition->vertexCount") &&
               Contains(extractorSource, "ApplyBindPoseSkinning(decoded, skinBones)") &&
               !Contains(extractorSource, "palette.push_back"),
           "optimized skinned partitions decode the shared vertex buffer with global skin bones");
    Expect(Contains(extractorSource, "MAX_SKIN_BONES") &&
                Contains(extractorSource, "MAX_SKIN_PARTITIONS") &&
                Contains(extractorSource, "MAX_PARTITION_BONES"),
           "skin traversal has explicit bone and partition limits");
    Expect(Contains(bindPoseHeader, "TryInvertAffine") &&
               Contains(extractorSource, "TryInvertAffine(skinToBone") &&
               Contains(extractorSource, "reconstructedExternalBones"),
           "standalone head parts reconstruct externally owned bind bones safely");
    Expect(Contains(extractorHeader, "reconstructedExternalBones") &&
               Contains(rendererSource, "reconstructed {} external bones"),
           "head-part bind reconstruction is exposed in renderer diagnostics");
    Expect(Contains(extractorSource, "AsDynamicTriShape") &&
               Contains(extractorSource, "GetDynamicTrishapeRuntimeData") &&
               Contains(extractorSource, "BSSpinLockGuard") &&
               Contains(extractorSource, "PackedPositionStream") &&
               Contains(extractorSource, "decodedDynamicShapes"),
           "BSDynamicTriShape head parts copy their locked Vector4 position stream");
    Expect(Contains(extractorHeader, "decodedDynamicShapes") &&
               Contains(rendererSource, "decoded {} dynamic shapes"),
           "dynamic head-part decoding is exposed in renderer diagnostics");
    Expect(Contains(materialTintHeader, "ActorMaterialTintOverride") &&
               Contains(rendererHeader, "ActorMaterialTintOverride materialTint") &&
               Contains(sceneController, ".materialTint = request.materialTint"),
           "actor tint state crosses only the private scene-submission boundary");
    Expect(Contains(sceneController, "partClone") &&
               Contains(sceneController, "GetFaceNodeSkinned") &&
               Contains(extractorSource, "a_preferRuntimeTextures"),
           "loaded actor appearance retains Skyrim's evaluated geometry and runtime materials");
    Expect(Contains(extractorSource, "Feature::kFaceGenRGBTint") &&
               Contains(extractorSource, "Feature::kHairTint") &&
               Contains(extractorSource, "ApplyActorMaterialTint"),
           "the extractor classifies tint shader materials and applies actor overrides to copies");
    Expect(Contains(extractorHeader, "matchedMaterialTintOverrides") &&
               Contains(rendererSource, "matched {} actor tint overrides"),
           "actor tint application is exposed in renderer diagnostics");
    Expect(Contains(materialShader, "gTintColor") &&
               Contains(materialShader, "ApplyFaceGenOverlay") &&
               Contains(materialShader, "MATERIAL_FACEGEN_RGB_TINT"),
           "the material shader applies Skyrim's FaceGen RGB overlay instead of direct multiplication");
    Expect(Contains(sceneController, "requested.size()") &&
                Contains(sceneController, "BSModelDB::Demand") &&
                Contains(sceneController, "SubmitNifSceneBatch"),
            "scene replacement copies, loads, and submits one bounded atomic batch");
    Expect(Contains(sceneController, "ReplaceWeightedScene") &&
                Contains(sceneController, "highModelPath") &&
                Contains(sceneController, "WEIGHTED_INTERFACE_VERSION"),
            "weighted scene replacement validates paired endpoint descriptors");
    Expect(Contains(sceneController, "ReplaceArmorScene") &&
               Contains(sceneController, "LookupByID<RE::TESObjectARMO>") &&
               Contains(sceneController, "armor->armorAddons") &&
               Contains(sceneController, "addon->IsValidRace(a_race)") &&
               !Contains(sceneController, "GetArmorAddon(race)") &&
               Contains(sceneController, "modelSwap = addon->bipedModels[sexIndex]") &&
               Contains(sceneController, "modelSwap.GetModel()"),
           "armor scenes resolve every race-valid ARMA and sex model on the Skyrim task");
    Expect(Contains(armorPath, "ARMA_WEIGHT_SLIDER_ENABLED = 2") &&
               Contains(armorPath, "ResolveArmorModelPaths") &&
               Contains(sceneController, "LoadAndSubmitRequestedScene"),
           "armor records derive weighted endpoints and reuse atomic scene loading");
    Expect(Contains(armorPath, "IsIntentionalBlankArmorModelPath") &&
               Contains(sceneController, "contains only intentional blank proxy models") &&
               Contains(sceneController, "requested.empty()"),
           "intentional equipped body proxies are omitted without weakening real armor failures");
    Expect(Contains(sceneController, "ReplaceActorAppearanceScene") &&
               Contains(sceneController, "LookupByID<RE::Actor>") &&
               Contains(sceneController, "Get3D(false)") &&
               Contains(sceneController, "GetBiped(false)") &&
               Contains(sceneController, "GetFaceNodeSkinned") &&
               Contains(sceneController, "liveActorRoots"),
           "actor appearance scenes acquire loaded player or NPC render roots on Skyrim's task thread");
    Expect(Contains(rendererHeader, "liveActorRoots") &&
               Contains(rendererSource, "ExtractLiveActorMesh") &&
               Contains(extractorHeader, "ExtractLiveActorMesh") &&
               Contains(extractorSource, "TryMakeRelativeTransform") &&
               Contains(extractorSource, "a_object->world"),
           "live actor snapshots normalize evaluated world transforms into one actor-local assembly");
    Expect(Contains(extractorSource, "GetAppCulled") &&
               Contains(rendererSource, "live actor") &&
               Contains(rendererSource, "accepted {} live shapes"),
           "live extraction skips culled geometry and reports dedicated snapshot diagnostics");
    Expect(Contains(extractorSource, "BSLightingShaderMaterialFacegen") &&
               Contains(extractorSource, "tintTexture") &&
               Contains(extractorSource, "detailTexture") &&
               Contains(extractorSource, "modelSpaceNormal"),
           "live materials retain FaceGen textures and model-space normal semantics");
    Expect(Contains(sceneController, "alternateTextures") &&
               Contains(sceneController, "numAlternateTextures") &&
               Contains(sceneController, "BGSTextureSet::Flag::kHasModelSpaceNormalMap") &&
               Contains(rendererHeader, "textureOverrides"),
           "ARMA texture swaps are copied into private renderer-owned descriptors");
    Expect(Contains(extractorSource, "FindTextureOverride") &&
               Contains(extractorSource, "ApplyTextureOverride") &&
               Contains(extractorSource, "shapeIndex"),
           "alternate textures match by shape name with traversal-index fallback");
    Expect(Contains(rendererSource, "ComposeScene") &&
                Contains(rendererSource, "MAX_RESOLVED_SCENE_PARTS") &&
                Contains(rendererSource, "m_sceneVisibility.find") &&
                Contains(rendererSource, "draw.sceneObject") &&
                Contains(rendererSource, "m_pendingVisibility.try_emplace(object, visible)"),
            "composed scene draws retain object ownership for visibility filtering");
    Expect(Contains(rendererHeader, "weightScene") &&
                Contains(rendererSource, "InterpolateWeightMeshes") &&
                Contains(rendererSource, "WeightInterpolationError::TopologyMismatch"),
            "paired endpoints are interpolated only after topology validation");
    Expect(Contains(rendererHeader, "lowModelPath") &&
               Contains(rendererHeader, "highModelPath") &&
               Contains(sceneController, ".lowModelPath = request.lowModelPath") &&
               Contains(sceneController, ".highModelPath = request.highModelPath"),
           "private scene submissions retain resolved endpoint paths for diagnostics");
    Expect(Contains(rendererSource, "scene object {} endpoint {} path '{}'") &&
               Contains(rendererSource, "extraction status {}") &&
               Contains(rendererSource, "first failed shape '{}'"),
           "extraction failures identify the object, endpoint, path, status, and first failed shape");
    Expect(Contains(extractorSource, "firstFailedShapeName") &&
               Contains(extractorSource, "shape->name.c_str()"),
           "the extractor retains the first malformed or unreadable shape name");
    Expect(Contains(loadGeneration, "NextNifLoadGeneration") &&
                Contains(viewController, "NextNifLoadGeneration()") &&
                Contains(sceneController, "NextNifLoadGeneration()"),
            "single-model and multi-object loads share one stale-work generation source");
    Expect(Contains(extensionSource, "Meridian.NifScene/1 requested") &&
                Contains(extensionSource, "Meridian.NifScene/2 requested") &&
                Contains(extensionSource, "Meridian.NifScene/3 requested") &&
                Contains(extensionSource, "Meridian.NifScene/4 requested") &&
                Contains(extensionSource, "NifSceneAPIController"),
            "scene API versions are queried separately from the frozen NifView vtable");

    Expect(!Contains(rendererSource, "CreateDeferredContext"),
           "ENB-incompatible game-device deferred contexts are forbidden");
    Expect(!Contains(rendererSource, "FinishCommandList"),
           "NIF renderer no longer records a game-device command list");
    Expect(!Contains(rendererSource, "ExecuteCommandList"),
           "NIF renderer no longer executes through Skyrim's immediate context");
    Expect(!Contains(rendererSource, "D3DCompile"),
           "material shaders are build-compiled rather than compiled in Skyrim");

    if (g_failures != 0)
    {
        std::cerr << g_failures << " NIF renderer architecture test(s) failed\n";
        return 1;
    }

    std::cout << "All NIF renderer architecture tests passed\n";
    return 0;
}
