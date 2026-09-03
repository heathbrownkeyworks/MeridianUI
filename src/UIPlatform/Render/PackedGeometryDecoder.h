#pragma once

#include "Render/NifPreviewMesh.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>

namespace Meridian::Render::NifPreview
{
    inline constexpr std::size_t MAX_PREVIEW_VERTICES = 4'000'000;
    inline constexpr std::size_t MAX_PREVIEW_INDICES = 12'000'000;

    struct PackedGeometryLayout
    {
        std::size_t stride = 0;
        std::size_t positionOffset = 0;
        std::size_t normalOffset = 0;
        std::size_t tangentOffset = 0;
        std::size_t textureCoordinateOffset = 0;
        std::size_t skinningOffset = 0;
        bool hasPackedNormal = false;
        bool hasPackedTangent = false;
        bool hasTextureCoordinate = false;
        bool hasSkinning = false;
    };

    struct PackedPositionStream
    {
        std::span<const std::uint8_t> bytes{};
        std::size_t stride = 0;
        std::size_t offset = 0;
    };

    enum class DecodeError
    {
        None,
        EmptyGeometry,
        InvalidLayout,
        TooLarge,
        TruncatedVertexBuffer,
        InvalidIndexCount,
        IndexOutOfRange,
        NonFiniteVertex,
        InvalidSkinWeights,
    };

    struct DecodeResult
    {
        DecodeError error = DecodeError::None;
        PreviewMesh mesh{};
    };

    namespace Detail
    {
        inline float ReadFloat(const std::uint8_t* a_source)
        {
            float value = 0.0f;
            std::memcpy(&value, a_source, sizeof(value));
            return value;
        }

        inline float DecodeHalf(const std::uint8_t* a_source)
        {
            std::uint16_t packed = 0;
            std::memcpy(&packed, a_source, sizeof(packed));
            const auto sign = (packed & 0x8000u) != 0 ? -1.0f : 1.0f;
            const auto exponent = static_cast<unsigned>((packed >> 10) & 0x1Fu);
            const auto mantissa = static_cast<unsigned>(packed & 0x03FFu);
            if (exponent == 0)
            {
                return sign * std::ldexp(static_cast<float>(mantissa), -24);
            }
            if (exponent == 0x1Fu)
            {
                return mantissa == 0 ? sign * std::numeric_limits<float>::infinity() : std::numeric_limits<float>::quiet_NaN();
            }
            return sign * std::ldexp(1.0f + static_cast<float>(mantissa) / 1024.0f,
                                     static_cast<int>(exponent) - 15);
        }

        inline Float3 DecodePackedNormal(const std::uint8_t* a_source)
        {
            constexpr float scale = 2.0f / 255.0f;
            return Normalize({
                static_cast<float>(a_source[0]) * scale - 1.0f,
                static_cast<float>(a_source[1]) * scale - 1.0f,
                static_cast<float>(a_source[2]) * scale - 1.0f,
            });
        }

        inline float DecodeSignedByte(std::uint8_t a_source)
        {
            return static_cast<float>(a_source) * (2.0f / 255.0f) - 1.0f;
        }

        inline float Dot(Float3 a_left, Float3 a_right)
        {
            return a_left.x * a_right.x + a_left.y * a_right.y + a_left.z * a_right.z;
        }

        inline Float4 DecodePackedTangent(const std::uint8_t* a_source,
                                          const std::uint8_t* a_positionSource,
                                          const PackedGeometryLayout& a_layout,
                                          Float3 a_normal)
        {
            // Skyrim SE packs the texture U axis across position.w, normal.w, and
            // VA_BINORMAL.w. VA_BINORMAL.xyz contains the texture V axis. Retain
            // the U axis and reduce V to the handedness expected by the shader.
            const auto textureU = Normalize({
                ReadFloat(a_positionSource + 12),
                DecodeSignedByte(a_source[a_layout.normalOffset + 3]),
                DecodeSignedByte(a_source[a_layout.tangentOffset + 3]),
            });
            const auto textureV = DecodePackedNormal(a_source + a_layout.tangentOffset);
            return {
                textureU.x,
                textureU.y,
                textureU.z,
                Dot(Cross(a_normal, textureU), textureV) < 0.0f ? -1.0f : 1.0f,
            };
        }

        inline Float3 OrthogonalTangent(Float3 a_normal)
        {
            const auto axis = std::abs(a_normal.z) < 0.999f ? Float3{0.0f, 0.0f, 1.0f} : Float3{0.0f, 1.0f, 0.0f};
            return Normalize(Cross(axis, a_normal));
        }

        inline void GenerateTangents(PreviewMesh& a_mesh)
        {
            std::vector<Float3> tangentSums(a_mesh.vertices.size());
            std::vector<float> handednessSums(a_mesh.vertices.size());
            for (std::size_t index = 0; index < a_mesh.indices.size(); index += 3)
            {
                const auto i0 = a_mesh.indices[index];
                const auto i1 = a_mesh.indices[index + 1];
                const auto i2 = a_mesh.indices[index + 2];
                const auto& v0 = a_mesh.vertices[i0];
                const auto& v1 = a_mesh.vertices[i1];
                const auto& v2 = a_mesh.vertices[i2];
                const auto edge1 = v1.position - v0.position;
                const auto edge2 = v2.position - v0.position;
                const Float2 uv1{
                    v1.textureCoordinate.x - v0.textureCoordinate.x,
                    v1.textureCoordinate.y - v0.textureCoordinate.y,
                };
                const Float2 uv2{
                    v2.textureCoordinate.x - v0.textureCoordinate.x,
                    v2.textureCoordinate.y - v0.textureCoordinate.y,
                };
                const auto determinant = uv1.x * uv2.y - uv1.y * uv2.x;
                if (!std::isfinite(determinant) || std::abs(determinant) <= 1.0e-12f)
                {
                    continue;
                }
                const auto inverse = 1.0f / determinant;
                const Float3 tangent{
                    (edge1.x * uv2.y - edge2.x * uv1.y) * inverse,
                    (edge1.y * uv2.y - edge2.y * uv1.y) * inverse,
                    (edge1.z * uv2.y - edge2.z * uv1.y) * inverse,
                };
                const Float3 bitangent{
                    (edge2.x * uv1.x - edge1.x * uv2.x) * inverse,
                    (edge2.y * uv1.x - edge1.y * uv2.x) * inverse,
                    (edge2.z * uv1.x - edge1.z * uv2.x) * inverse,
                };
                for (const auto vertexIndex : {i0, i1, i2})
                {
                    tangentSums[vertexIndex] += tangent;
                    const auto sign = Dot(Cross(a_mesh.vertices[vertexIndex].normal, tangent),
                                          bitangent) < 0.0f
                                          ? -1.0f
                                          : 1.0f;
                    handednessSums[vertexIndex] += sign;
                }
            }

            for (std::size_t index = 0; index < a_mesh.vertices.size(); ++index)
            {
                const auto normal = a_mesh.vertices[index].normal;
                auto tangent = tangentSums[index];
                const auto projection = Dot(normal, tangent);
                tangent = {
                    tangent.x - normal.x * projection,
                    tangent.y - normal.y * projection,
                    tangent.z - normal.z * projection,
                };
                const auto lengthSquared = Dot(tangent, tangent);
                if (!std::isfinite(lengthSquared) || lengthSquared <= 1.0e-12f)
                {
                    tangent = OrthogonalTangent(normal);
                }
                else
                {
                    tangent = Normalize(tangent);
                }
                a_mesh.vertices[index].tangent = {
                    tangent.x,
                    tangent.y,
                    tangent.z,
                    handednessSums[index] < 0.0f ? -1.0f : 1.0f,
                };
            }
        }

        inline void GenerateNormals(PreviewMesh& a_mesh)
        {
            for (auto& vertex : a_mesh.vertices)
            {
                vertex.normal = {};
            }
            for (std::size_t index = 0; index < a_mesh.indices.size(); index += 3)
            {
                const auto i0 = a_mesh.indices[index];
                const auto i1 = a_mesh.indices[index + 1];
                const auto i2 = a_mesh.indices[index + 2];
                const auto edge1 = a_mesh.vertices[i1].position - a_mesh.vertices[i0].position;
                const auto edge2 = a_mesh.vertices[i2].position - a_mesh.vertices[i0].position;
                const auto normal = Cross(edge1, edge2);
                a_mesh.vertices[i0].normal += normal;
                a_mesh.vertices[i1].normal += normal;
                a_mesh.vertices[i2].normal += normal;
            }
            for (auto& vertex : a_mesh.vertices)
            {
                vertex.normal = Normalize(vertex.normal);
            }
        }
    }

    inline DecodeResult DecodePackedGeometry(
        std::span<const std::uint8_t> a_vertexBytes,
        std::size_t a_vertexCount,
        std::span<const std::uint16_t> a_indices,
        const PackedGeometryLayout& a_layout,
        PackedPositionStream a_positionStream = {})
    {
        DecodeResult result{};
        if (a_vertexCount == 0 || a_indices.empty())
        {
            result.error = DecodeError::EmptyGeometry;
            return result;
        }
        if (a_vertexCount > MAX_PREVIEW_VERTICES || a_indices.size() > MAX_PREVIEW_INDICES)
        {
            result.error = DecodeError::TooLarge;
            return result;
        }
        const bool hasExternalPositions = !a_positionStream.bytes.empty();
        const auto positionWidth =
            a_layout.hasPackedTangent ? sizeof(float) * 4 : sizeof(float) * 3;
        if (a_layout.stride == 0 ||
            (!hasExternalPositions &&
             (a_layout.positionOffset > a_layout.stride ||
              positionWidth > a_layout.stride - a_layout.positionOffset)) ||
            (hasExternalPositions &&
             (a_positionStream.stride == 0 ||
              a_positionStream.offset > a_positionStream.stride ||
              positionWidth > a_positionStream.stride - a_positionStream.offset)) ||
            (a_layout.hasPackedNormal &&
             (a_layout.normalOffset > a_layout.stride || 4 > a_layout.stride - a_layout.normalOffset)) ||
            (a_layout.hasPackedTangent &&
             (a_layout.tangentOffset > a_layout.stride || 4 > a_layout.stride - a_layout.tangentOffset)) ||
            (a_layout.hasTextureCoordinate &&
             (a_layout.textureCoordinateOffset > a_layout.stride ||
              4 > a_layout.stride - a_layout.textureCoordinateOffset)) ||
            (a_layout.hasSkinning &&
             (a_layout.skinningOffset > a_layout.stride ||
              12 > a_layout.stride - a_layout.skinningOffset)))
        {
            result.error = DecodeError::InvalidLayout;
            return result;
        }
        if (hasExternalPositions)
        {
            if (a_positionStream.offset > a_positionStream.bytes.size() ||
                positionWidth > a_positionStream.bytes.size() - a_positionStream.offset ||
                a_vertexCount - 1 >
                    (a_positionStream.bytes.size() - a_positionStream.offset - positionWidth) /
                        a_positionStream.stride)
            {
                result.error = DecodeError::TruncatedVertexBuffer;
                return result;
            }
        }
        if (a_vertexCount > a_vertexBytes.size() / a_layout.stride)
        {
            result.error = DecodeError::TruncatedVertexBuffer;
            return result;
        }
        if (a_indices.size() % 3 != 0)
        {
            result.error = DecodeError::InvalidIndexCount;
            return result;
        }

        result.mesh.vertices.resize(a_vertexCount);
        for (std::size_t index = 0; index < a_vertexCount; ++index)
        {
            const auto* source = a_vertexBytes.data() + index * a_layout.stride;
            const auto* positionSource = hasExternalPositions ?
                a_positionStream.bytes.data() +
                    index * a_positionStream.stride + a_positionStream.offset :
                source + a_layout.positionOffset;
            auto& vertex = result.mesh.vertices[index];
            vertex.position = {
                Detail::ReadFloat(positionSource),
                Detail::ReadFloat(positionSource + 4),
                Detail::ReadFloat(positionSource + 8),
            };
            if (!std::isfinite(vertex.position.x) ||
                !std::isfinite(vertex.position.y) ||
                !std::isfinite(vertex.position.z))
            {
                result.mesh = {};
                result.error = DecodeError::NonFiniteVertex;
                return result;
            }
            if (a_layout.hasPackedNormal)
            {
                vertex.normal = Detail::DecodePackedNormal(source + a_layout.normalOffset);
            }
            if (a_layout.hasPackedTangent)
            {
                vertex.tangent = Detail::DecodePackedTangent(
                    source,
                    positionSource,
                    a_layout,
                    vertex.normal);
            }
            if (a_layout.hasTextureCoordinate)
            {
                vertex.textureCoordinate = {
                    Detail::DecodeHalf(source + a_layout.textureCoordinateOffset),
                    Detail::DecodeHalf(source + a_layout.textureCoordinateOffset + 2),
                };
                if (!std::isfinite(vertex.textureCoordinate.x) ||
                    !std::isfinite(vertex.textureCoordinate.y))
                {
                    result.mesh = {};
                    result.error = DecodeError::NonFiniteVertex;
                    return result;
                }
            }
            if (a_layout.hasSkinning)
            {
                for (std::size_t influence = 0; influence < vertex.boneWeights.size(); ++influence)
                {
                    const auto weight = Detail::DecodeHalf(
                        source + a_layout.skinningOffset + influence * sizeof(std::uint16_t));
                    if (!std::isfinite(weight) || weight < 0.0f)
                    {
                        result.mesh = {};
                        result.error = DecodeError::InvalidSkinWeights;
                        return result;
                    }
                    vertex.boneWeights[influence] = weight;
                    vertex.boneIndices[influence] =
                        source[a_layout.skinningOffset + 8 + influence];
                }
            }
            result.mesh.bounds.Include(vertex.position);
        }

        result.mesh.indices.reserve(a_indices.size());
        for (const auto index : a_indices)
        {
            if (index >= a_vertexCount)
            {
                result.mesh = {};
                result.error = DecodeError::IndexOutOfRange;
                return result;
            }
            result.mesh.indices.push_back(index);
        }

        if (!a_layout.hasPackedNormal)
        {
            Detail::GenerateNormals(result.mesh);
        }
        if (!a_layout.hasPackedTangent)
        {
            Detail::GenerateTangents(result.mesh);
        }
        result.mesh.draws.push_back({
            .sceneObject = 0,
            .startIndex = 0,
            .indexCount = static_cast<std::uint32_t>(result.mesh.indices.size()),
            .material = {},
        });
        return result;
    }

    inline bool AppendMesh(PreviewMesh& a_destination, const PreviewMesh& a_source)
    {
        if (a_source.Empty() ||
            a_destination.vertices.size() > MAX_PREVIEW_VERTICES - a_source.vertices.size() ||
            a_destination.indices.size() > MAX_PREVIEW_INDICES - a_source.indices.size() ||
            a_destination.vertices.size() > std::numeric_limits<std::uint32_t>::max())
        {
            return false;
        }

        const auto baseVertex = static_cast<std::uint32_t>(a_destination.vertices.size());
        const auto baseIndex = static_cast<std::uint32_t>(a_destination.indices.size());
        a_destination.vertices.insert(
            a_destination.vertices.end(), a_source.vertices.begin(), a_source.vertices.end());
        a_destination.indices.reserve(a_destination.indices.size() + a_source.indices.size());
        for (const auto index : a_source.indices)
        {
            if (index > std::numeric_limits<std::uint32_t>::max() - baseVertex)
            {
                return false;
            }
            a_destination.indices.push_back(index + baseVertex);
        }
        a_destination.draws.reserve(a_destination.draws.size() + a_source.draws.size());
        for (const auto& draw : a_source.draws)
        {
            if (draw.indexCount == 0 ||
                draw.startIndex > a_source.indices.size() ||
                draw.indexCount > a_source.indices.size() - draw.startIndex ||
                draw.startIndex > std::numeric_limits<std::uint32_t>::max() - baseIndex)
            {
                return false;
            }
            auto rebased = draw;
            rebased.startIndex += baseIndex;
            a_destination.draws.push_back(std::move(rebased));
        }
        a_destination.bounds.Include(a_source.bounds);
        return true;
    }
}
