#include "Render/NifTextureOverride.h"

#include <iostream>
#include <span>
#include <string>
#include <vector>

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
}

int main()
{
    using namespace Meridian::Render::NifPreview;

    ModelTextureOverride fallback{
        .name3D = "OtherShape",
        .index3D = 0,
        .textureSet = {.diffuse = "textures\\fallback.dds"},
    };
    ModelTextureOverride named{
        .name3D = "_CuirassLight_1",
        .index3D = 7,
        .textureSet = {.diffuse = "textures\\named.dds"},
    };
    const std::vector overrides{fallback, named};

    const auto* byName = FindTextureOverride(
        std::span<const ModelTextureOverride>(overrides), "_cuirasslight_1", 0);
    Expect(byName == std::addressof(overrides[1]),
           "case-insensitive 3D name takes precedence over index fallback");

    const auto* byIndex = FindTextureOverride(
        std::span<const ModelTextureOverride>(overrides), "_CuirassLight_0", 0);
    Expect(byIndex == std::addressof(overrides[0]),
           "shape index supports weighted endpoint names that differ");

    const auto* missing = FindTextureOverride(
        std::span<const ModelTextureOverride>(overrides), "Unmatched", 3);
    Expect(missing == nullptr, "unmatched shapes keep their NIF material");

    PreviewMaterial material{};
    material.diffuseTexturePath = "old-diffuse.dds";
    material.normalTexturePath = "old-normal.dds";
    material.environmentMaskTexturePath = "old-mask.dds";
    material.environmentTexturePath = "old-environment.dds";
    material.specularTexturePath = "old-specular.dds";
    material.tangentSpaceNormal = false;
    const ModelTextureOverride replacement{
        .textureSet = {
            .diffuse = "textures\\new-diffuse.dds",
            .normal = "textures\\new-normal.dds",
            .environmentMask = "textures\\new-mask.dds",
            .environment = "textures\\new-environment.dds",
            .specular = "textures\\new-specular.dds",
        },
    };
    ApplyTextureOverride(material, replacement);
    Expect(material.diffuseTexturePath == replacement.textureSet.diffuse &&
               material.normalTexturePath == replacement.textureSet.normal &&
               material.environmentMaskTexturePath == replacement.textureSet.environmentMask &&
               material.environmentTexturePath == replacement.textureSet.environment &&
               material.specularTexturePath == replacement.textureSet.specular,
           "alternate texture set replaces every supported material path");
    Expect(material.tangentSpaceNormal,
           "a normal map remains tangent-space unless the TXST marks model-space normals");

    PreviewMaterial flagged{};
    flagged.specularTexturePath = "old-specular.dds";
    const ModelTextureOverride flags{
        .textureSet = {
            .normal = "textures\\model-space.dds",
            .specular = "textures\\ignored-specular.dds",
            .modelSpaceNormals = true,
            .noSpecularMap = true,
        },
    };
    ApplyTextureOverride(flagged, flags);
    Expect(!flagged.tangentSpaceNormal,
           "model-space TXST normals do not enable tangent-space sampling");
    Expect(flagged.specularTexturePath.empty(),
           "NoSpecularMap clears the alternate specular/backlight path");

    if (g_failures != 0)
    {
        std::cerr << g_failures << " NIF texture override test(s) failed\n";
        return 1;
    }
    std::cout << "All NIF texture override tests passed\n";
    return 0;
}
