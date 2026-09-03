#pragma once

#include "Render/NifPreviewMesh.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace Meridian::Render::NifPreview
{
    inline constexpr std::size_t MAX_MODEL_TEXTURE_OVERRIDES = 128;

    struct TextureSetOverride
    {
        std::string diffuse;
        std::string normal;
        std::string environmentMask;
        std::string environment;
        std::string specular;
        bool modelSpaceNormals = false;
        bool noSpecularMap = false;
    };

    struct ModelTextureOverride
    {
        std::string name3D;
        std::uint32_t index3D = 0;
        TextureSetOverride textureSet;
    };

    inline bool EqualAsciiInsensitive(std::string_view a_left, std::string_view a_right)
    {
        return a_left.size() == a_right.size() &&
               std::equal(
                   a_left.begin(),
                   a_left.end(),
                   a_right.begin(),
                   [](char a_first, char a_second) {
                       return std::tolower(static_cast<unsigned char>(a_first)) ==
                              std::tolower(static_cast<unsigned char>(a_second));
                   });
    }

    inline const ModelTextureOverride* FindTextureOverride(
        std::span<const ModelTextureOverride> a_overrides,
        std::string_view a_shapeName,
        std::uint32_t a_shapeIndex)
    {
        if (!a_shapeName.empty())
        {
            for (const auto& override : a_overrides)
            {
                if (!override.name3D.empty() &&
                    EqualAsciiInsensitive(override.name3D, a_shapeName))
                {
                    return std::addressof(override);
                }
            }
        }
        for (const auto& override : a_overrides)
        {
            if (override.index3D == a_shapeIndex)
            {
                return std::addressof(override);
            }
        }
        return nullptr;
    }

    inline void ApplyTextureOverride(PreviewMaterial& a_material,
                                     const ModelTextureOverride& a_override)
    {
        a_material.diffuseTexturePath = a_override.textureSet.diffuse;
        a_material.normalTexturePath = a_override.textureSet.normal;
        a_material.environmentMaskTexturePath = a_override.textureSet.environmentMask;
        a_material.environmentTexturePath = a_override.textureSet.environment;
        a_material.specularTexturePath = a_override.textureSet.noSpecularMap ?
            std::string{} : a_override.textureSet.specular;
        a_material.tangentSpaceNormal =
            !a_override.textureSet.modelSpaceNormals &&
            !a_material.normalTexturePath.empty();
        a_material.environmentEnabled =
            a_material.environmentEnabled &&
            !a_material.environmentTexturePath.empty();
    }
}
