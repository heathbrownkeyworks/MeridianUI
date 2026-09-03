#pragma once

#include "PCH.h"
#include "Render/NifPreviewMesh.h"
#include "Render/NifMaterialTint.h"
#include "Render/NifTextureOverride.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace Meridian::Render::NifPreview
{
    enum class ExtractionStatus
    {
        Success,
        NoSupportedGeometry,
        MalformedGeometry,
        GpuReadbackFailed,
    };

    struct ExtractionResult
    {
        ExtractionStatus status = ExtractionStatus::NoSupportedGeometry;
        PreviewMesh mesh{};
        std::size_t acceptedShapes = 0;
        std::size_t skippedShapes = 0;
        std::size_t matchedTextureOverrides = 0;
        std::size_t matchedMaterialTintOverrides = 0;
        std::size_t reconstructedExternalBones = 0;
        std::size_t decodedDynamicShapes = 0;
        std::string firstFailedShapeName;
    };

    ExtractionResult ExtractModelMesh(RE::NiNode* a_root,
                                      ID3D11Device* a_device,
                                      ID3D11DeviceContext* a_context,
                                      std::span<const ModelTextureOverride> a_textureOverrides = {},
                                      const ActorMaterialTintOverride* a_materialTint = nullptr);

    ExtractionResult ExtractLiveActorMesh(
        RE::NiNode* a_actorRoot,
        std::span<RE::NiAVObject* const> a_selectedRoots,
        ID3D11Device* a_device,
        ID3D11DeviceContext* a_context);
}
