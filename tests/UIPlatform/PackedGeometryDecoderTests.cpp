#include "Render/PackedGeometryDecoder.h"

#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
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

    void WriteFloat(std::vector<std::uint8_t>& a_bytes, std::size_t a_offset, float a_value)
    {
        std::memcpy(a_bytes.data() + a_offset, &a_value, sizeof(a_value));
    }

    void WriteHalf(std::vector<std::uint8_t>& a_bytes, std::size_t a_offset, std::uint16_t a_value)
    {
        std::memcpy(a_bytes.data() + a_offset, &a_value, sizeof(a_value));
    }
}

int main()
{
    using namespace Meridian::Render::NifPreview;

    constexpr std::size_t stride = 40;
    std::vector<std::uint8_t> vertices(stride * 3, 0);
    const float positions[3][3] = {
        {-1.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f},
        {0.0f, 2.0f, 0.0f},
    };
    for (std::size_t vertex = 0; vertex < 3; ++vertex)
    {
        const auto base = vertex * stride;
        WriteFloat(vertices, base, positions[vertex][0]);
        WriteFloat(vertices, base + 4, positions[vertex][1]);
        WriteFloat(vertices, base + 8, positions[vertex][2]);
        WriteFloat(vertices, base + 12, 1.0f);
        vertices[base + 16] = 128;
        vertices[base + 17] = 128;
        vertices[base + 18] = 255;
        vertices[base + 19] = 128;
        const std::uint16_t uvs[3][2] = {
            {0x0000, 0x0000},
            {0x3C00, 0x3800},
            {0xBC00, 0x4000},
        };
        WriteHalf(vertices, base + 20, uvs[vertex][0]);
        WriteHalf(vertices, base + 22, uvs[vertex][1]);
        vertices[base + 24] = 128;
        vertices[base + 25] = 255;
        vertices[base + 26] = 128;
        vertices[base + 27] = 128;
        WriteHalf(vertices, base + 28, 0x3A00);
        WriteHalf(vertices, base + 30, 0x3400);
        WriteHalf(vertices, base + 32, 0x0000);
        WriteHalf(vertices, base + 34, 0x0000);
        vertices[base + 36] = 7;
        vertices[base + 37] = 6;
        vertices[base + 38] = 0;
        vertices[base + 39] = 0;
    }
    const std::uint16_t indices[] = {0, 1, 2};

    PackedGeometryLayout layout{};
    layout.stride = stride;
    layout.positionOffset = 0;
    layout.normalOffset = 16;
    layout.hasPackedNormal = true;
    layout.textureCoordinateOffset = 20;
    layout.hasTextureCoordinate = true;
    layout.tangentOffset = 24;
    layout.hasPackedTangent = true;
    layout.skinningOffset = 28;
    layout.hasSkinning = true;

    const auto decoded = DecodePackedGeometry(vertices, 3, indices, layout);
    Expect(decoded.error == DecodeError::None, "valid packed geometry decodes");
    Expect(decoded.mesh.vertices.size() == 3, "all vertices are copied");
    Expect(decoded.mesh.indices.size() == 3, "all indices are copied");
    Expect(decoded.mesh.vertices[0].position.x == -1.0f, "position floats retain their value");
    Expect(decoded.mesh.vertices[2].position.y == 2.0f, "position components use XYZ order");
    Expect(decoded.mesh.vertices[0].normal.z > 0.99f, "packed normals decode and normalize");
    Expect(decoded.mesh.vertices[1].textureCoordinate.x == 1.0f &&
               decoded.mesh.vertices[1].textureCoordinate.y == 0.5f,
           "half-precision UV values decode exactly");
    Expect(decoded.mesh.vertices[2].textureCoordinate.x == -1.0f &&
               decoded.mesh.vertices[2].textureCoordinate.y == 2.0f,
           "signed and greater-than-one UV values are preserved");
    Expect(decoded.mesh.vertices[0].tangent.x > 0.99f &&
               decoded.mesh.vertices[0].tangent.w == 1.0f,
           "Skyrim's split packed tangent basis decodes with handedness");
    Expect(std::abs(decoded.mesh.vertices[0].boneWeights[0] - 0.75f) < 0.001f &&
               std::abs(decoded.mesh.vertices[0].boneWeights[1] - 0.25f) < 0.001f,
           "four half-precision skin weights decode");
    Expect(decoded.mesh.vertices[0].boneIndices[0] == 7 &&
               decoded.mesh.vertices[0].boneIndices[1] == 6,
           "four optimized global skin-bone indices decode");
    Expect(decoded.mesh.draws.size() == 1 && decoded.mesh.draws[0].indexCount == 3,
           "decoded geometry starts with one default draw range");
    Expect(decoded.mesh.bounds.minimum.x == -1.0f && decoded.mesh.bounds.maximum.x == 1.0f,
           "X bounds cover every vertex");
    Expect(decoded.mesh.bounds.minimum.y == 0.0f && decoded.mesh.bounds.maximum.y == 2.0f,
           "Y bounds cover every vertex");

    constexpr std::size_t dynamicStride = 24;
    constexpr std::size_t positionStride = 16;
    std::vector<std::uint8_t> dynamicVertices(dynamicStride * 3, 0);
    std::vector<std::uint8_t> dynamicPositions(positionStride * 3, 0);
    for (std::size_t vertex = 0; vertex < 3; ++vertex)
    {
        const auto source = vertex * stride;
        const auto dynamicBase = vertex * dynamicStride;
        const auto positionBase = vertex * positionStride;
        std::memcpy(dynamicPositions.data() + positionBase,
                    vertices.data() + source,
                    positionStride);
        std::memcpy(dynamicVertices.data() + dynamicBase,
                    vertices.data() + source + 16,
                    4);
        std::memcpy(dynamicVertices.data() + dynamicBase + 4,
                    vertices.data() + source + 20,
                    4);
        std::memcpy(dynamicVertices.data() + dynamicBase + 8,
                    vertices.data() + source + 24,
                    4);
        std::memcpy(dynamicVertices.data() + dynamicBase + 12,
                    vertices.data() + source + 28,
                    12);
    }
    PackedGeometryLayout dynamicLayout{};
    dynamicLayout.stride = dynamicStride;
    dynamicLayout.normalOffset = 0;
    dynamicLayout.hasPackedNormal = true;
    dynamicLayout.textureCoordinateOffset = 4;
    dynamicLayout.hasTextureCoordinate = true;
    dynamicLayout.tangentOffset = 8;
    dynamicLayout.hasPackedTangent = true;
    dynamicLayout.skinningOffset = 12;
    dynamicLayout.hasSkinning = true;
    const PackedPositionStream externalPositions{
        .bytes = dynamicPositions,
        .stride = positionStride,
    };
    const auto dynamicDecoded = DecodePackedGeometry(
        dynamicVertices,
        3,
        indices,
        dynamicLayout,
        externalPositions);
    Expect(dynamicDecoded.error == DecodeError::None,
           "dynamic geometry decodes attributes with an external position stream");
    Expect(dynamicDecoded.mesh.vertices[0].position.x == -1.0f &&
               dynamicDecoded.mesh.vertices[2].position.y == 2.0f,
           "dynamic geometry receives XYZ from its Vector4 position stream");
    Expect(dynamicDecoded.mesh.vertices[0].tangent.x > 0.99f,
           "dynamic position W participates in the split packed tangent basis");

    auto truncatedPositions = externalPositions;
    truncatedPositions.bytes = std::span<const std::uint8_t>(
        dynamicPositions.data(), dynamicPositions.size() - 1);
    Expect(DecodePackedGeometry(dynamicVertices,
                                3,
                                indices,
                                dynamicLayout,
                                truncatedPositions)
               .error == DecodeError::TruncatedVertexBuffer,
           "truncated dynamic position streams are rejected");

    PackedGeometryLayout noNormals = layout;
    noNormals.hasPackedNormal = false;
    const auto generated = DecodePackedGeometry(vertices, 3, indices, noNormals);
    Expect(generated.error == DecodeError::None, "geometry without normals is accepted");
    Expect(generated.mesh.vertices[0].normal.z > 0.99f, "missing normals are generated from triangles");

    PackedGeometryLayout noTangents = layout;
    noTangents.hasPackedTangent = false;
    const auto generatedTangents = DecodePackedGeometry(vertices, 3, indices, noTangents);
    Expect(generatedTangents.error == DecodeError::None,
           "geometry without packed tangents is accepted");
    Expect(std::isfinite(generatedTangents.mesh.vertices[0].tangent.x) &&
               std::abs(generatedTangents.mesh.vertices[0].tangent.w) == 1.0f,
           "missing tangents are generated with finite handedness");

    const std::uint16_t badIndices[] = {0, 1, 3};
    Expect(DecodePackedGeometry(vertices, 3, badIndices, layout).error == DecodeError::IndexOutOfRange,
           "out-of-range indices are rejected");
    Expect(DecodePackedGeometry(vertices, 2, indices, layout).error == DecodeError::IndexOutOfRange,
           "a shared-buffer partition must decode against the shape-wide vertex count");
    Expect(DecodePackedGeometry(std::span<const std::uint8_t>(vertices.data(), vertices.size() - 1),
                                3,
                                indices,
                                layout)
                   .error == DecodeError::TruncatedVertexBuffer,
           "truncated vertex buffers are rejected");

    PackedGeometryLayout badLayout = layout;
    badLayout.positionOffset = 28;
    Expect(DecodePackedGeometry(vertices, 3, indices, badLayout).error == DecodeError::InvalidLayout,
           "position reads outside the stride are rejected");

    badLayout = layout;
    badLayout.textureCoordinateOffset = 38;
    Expect(DecodePackedGeometry(vertices, 3, indices, badLayout).error == DecodeError::InvalidLayout,
           "UV reads outside the stride are rejected");

    badLayout = layout;
    badLayout.tangentOffset = 38;
    Expect(DecodePackedGeometry(vertices, 3, indices, badLayout).error == DecodeError::InvalidLayout,
           "tangent reads outside the stride are rejected");

    badLayout = layout;
    badLayout.skinningOffset = 30;
    Expect(DecodePackedGeometry(vertices, 3, indices, badLayout).error == DecodeError::InvalidLayout,
           "skinning reads outside the stride are rejected");

    auto negativeWeight = vertices;
    WriteHalf(negativeWeight, 28, 0xB800);
    Expect(DecodePackedGeometry(negativeWeight, 3, indices, layout).error == DecodeError::InvalidSkinWeights,
           "negative skin weights are rejected");

    auto nonFinite = vertices;
    WriteFloat(nonFinite, 0, std::numeric_limits<float>::infinity());
    Expect(DecodePackedGeometry(nonFinite, 3, indices, layout).error == DecodeError::NonFiniteVertex,
           "non-finite vertex positions are rejected");

    PreviewMesh combined{};
    Expect(AppendMesh(combined, decoded.mesh), "a first shape appends successfully");
    Expect(AppendMesh(combined, decoded.mesh), "a second shape appends successfully");
    Expect(combined.vertices.size() == 6 && combined.indices.size() == 6,
           "append retains both shapes");
    Expect(combined.indices[3] == 3 && combined.indices[5] == 5,
           "appended indices are rebased to the combined vertex array");
    Expect(combined.draws.size() == 2 && combined.draws[1].startIndex == 3,
           "appended draw ranges are rebased to the combined index array");

    return g_failures == 0 ? 0 : 1;
}
