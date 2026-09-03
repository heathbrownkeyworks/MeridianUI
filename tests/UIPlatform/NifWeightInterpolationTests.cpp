#include "Render/NifWeightInterpolation.h"

#include <cmath>
#include <iostream>
#include <limits>

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

    bool Almost(float a_left, float a_right)
    {
        return std::abs(a_left - a_right) < 0.0001f;
    }

    Meridian::Render::NifPreview::PreviewMesh MakeMesh(float a_offset)
    {
        using namespace Meridian::Render::NifPreview;
        PreviewMesh mesh{};
        mesh.vertices = {
            {
                .position = {a_offset, 0.0f, 0.0f},
                .normal = {1.0f, 0.0f, 0.0f},
                .tangent = {1.0f, 0.0f, 0.0f, 1.0f},
                .textureCoordinate = {0.1f, 0.2f},
                .boneWeights = {1.0f, 0.0f, 0.0f, 0.0f},
                .boneIndices = {1, 2, 0, 0},
            },
            {
                .position = {1.0f + a_offset, 0.0f, 0.0f},
                .normal = {0.0f, 1.0f, 0.0f},
                .tangent = {0.0f, 1.0f, 0.0f, 1.0f},
                .textureCoordinate = {0.3f, 0.4f},
                .boneWeights = {0.75f, 0.25f, 0.0f, 0.0f},
                .boneIndices = {1, 2, 0, 0},
            },
            {
                .position = {a_offset, 1.0f, 0.0f},
                .normal = {0.0f, 0.0f, 1.0f},
                .tangent = {1.0f, 0.0f, 0.0f, 1.0f},
                .textureCoordinate = {0.5f, 0.6f},
                .boneWeights = {0.5f, 0.5f, 0.0f, 0.0f},
                .boneIndices = {1, 2, 0, 0},
            },
        };
        mesh.indices = {0, 1, 2};
        PreviewMaterial material{};
        material.diffuseTexturePath = "textures\\armor\\hide.dds";
        mesh.draws.push_back({
            .startIndex = 0,
            .indexCount = 3,
            .material = std::move(material),
        });
        for (const auto& vertex : mesh.vertices)
        {
            mesh.bounds.Include(vertex.position);
        }
        return mesh;
    }
}

int main()
{
    using namespace Meridian::Render::NifPreview;

    auto low = MakeMesh(0.0f);
    auto high = MakeMesh(2.0f);
    high.vertices[0].normal = {0.0f, 1.0f, 0.0f};
    high.vertices[0].tangent = {0.0f, 1.0f, 0.0f, 1.0f};
    high.vertices[0].boneWeights = {0.5f, 0.5f, 0.0f, 0.0f};
    high.vertices[0].textureCoordinate = {0.9f, 0.9f};

    const auto zero = InterpolateWeightMeshes(low, high, 0.0f);
    Expect(zero && Almost(zero.mesh.vertices[0].position.x, 0.0f),
           "zero weight uses the low endpoint");

    const auto full = InterpolateWeightMeshes(low, high, 1.0f);
    Expect(full && Almost(full.mesh.vertices[0].position.x, 2.0f),
           "full weight uses the high endpoint");

    const auto half = InterpolateWeightMeshes(low, high, 0.5f);
    Expect(half && Almost(half.mesh.vertices[0].position.x, 1.0f),
           "mid weight interpolates vertex positions");
    if (half)
    {
        const auto& vertex = half.mesh.vertices[0];
        Expect(Almost(vertex.normal.x, 0.7071067f) &&
                   Almost(vertex.normal.y, 0.7071067f),
               "interpolated normals are normalized");
        Expect(Almost(vertex.tangent.x, 0.7071067f) &&
                   Almost(vertex.tangent.y, 0.7071067f) &&
                   vertex.tangent.w == 1.0f,
               "interpolated tangents are normalized with stable handedness");
        Expect(Almost(vertex.boneWeights[0], 1.0f) &&
                   Almost(vertex.boneWeights[1], 0.0f),
               "post-skinning metadata is retained from the low endpoint");
        Expect(Almost(vertex.textureCoordinate.x, 0.1f) &&
                   half.mesh.draws[0].material.diffuseTexturePath ==
                       "textures\\armor\\hide.dds",
               "the low endpoint retains UV and material identity");
        Expect(half.mesh.bounds.valid && Almost(half.mesh.bounds.minimum.x, 1.0f) &&
                   Almost(half.mesh.bounds.maximum.x, 2.0f),
               "bounds are rebuilt from interpolated positions");
    }

    Expect(InterpolateWeightMeshes(low, high, -0.01f).error ==
               WeightInterpolationError::InvalidWeight,
           "negative weights are rejected");
    Expect(InterpolateWeightMeshes(low, high, 1.01f).error ==
               WeightInterpolationError::InvalidWeight,
           "weights above one are rejected");
    Expect(InterpolateWeightMeshes(
               low, high, std::numeric_limits<float>::quiet_NaN()).error ==
               WeightInterpolationError::InvalidWeight,
           "non-finite weights are rejected");

    auto mismatch = high;
    mismatch.indices[2] = 1;
    const auto indexMismatch = InterpolateWeightMeshes(low, mismatch, 0.5f);
    Expect(indexMismatch.error == WeightInterpolationError::TopologyMismatch &&
               indexMismatch.topologyMismatch == WeightTopologyMismatch::Indices,
           "different index topology is rejected");
    mismatch = high;
    mismatch.draws[0].indexCount = 2;
    const auto drawMismatch = InterpolateWeightMeshes(low, mismatch, 0.5f);
    Expect(drawMismatch.error == WeightInterpolationError::TopologyMismatch &&
               drawMismatch.topologyMismatch == WeightTopologyMismatch::DrawRange &&
               drawMismatch.mismatchIndex == 0,
           "different draw topology is rejected");
    mismatch = high;
    mismatch.vertices[0].boneIndices[0] = 9;
    mismatch.vertices[0].boneWeights = {0.0f, 1.0f, 0.0f, 0.0f};
    const auto paletteMismatch = InterpolateWeightMeshes(low, mismatch, 0.5f);
    Expect(paletteMismatch && paletteMismatch.mesh.vertices[0].boneIndices[0] == 1 &&
               Almost(paletteMismatch.mesh.vertices[0].boneWeights[0], 1.0f),
           "post-skinning palette differences do not reject compatible geometry");
    mismatch = high;
    mismatch.vertices[0].tangent.w = -1.0f;
    const auto lowHandedness = InterpolateWeightMeshes(low, mismatch, 0.25f);
    const auto highHandedness = InterpolateWeightMeshes(low, mismatch, 0.75f);
    Expect(lowHandedness && highHandedness &&
               lowHandedness.mesh.vertices[0].tangent.w == 1.0f &&
               highHandedness.mesh.vertices[0].tangent.w == -1.0f,
           "different tangent handedness selects the nearest endpoint");

    if (g_failures != 0)
    {
        std::cerr << g_failures << " NIF weight interpolation test(s) failed\n";
        return 1;
    }
    std::cout << "All NIF weight interpolation tests passed\n";
    return 0;
}
