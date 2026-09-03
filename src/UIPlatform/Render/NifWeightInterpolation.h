#pragma once

#include "Render/NifPreviewMesh.h"

#include <cmath>
#include <cstddef>

namespace Meridian::Render::NifPreview
{
    enum class WeightInterpolationError
    {
        None,
        InvalidWeight,
        EmptyMesh,
        TopologyMismatch,
        NonFiniteVertex,
    };

    enum class WeightTopologyMismatch
    {
        None,
        VertexCount,
        Indices,
        DrawCount,
        DrawRange,
    };

    inline const char* WeightTopologyMismatchName(WeightTopologyMismatch a_mismatch)
    {
        switch (a_mismatch)
        {
        case WeightTopologyMismatch::None:
            return "none";
        case WeightTopologyMismatch::VertexCount:
            return "vertex-count";
        case WeightTopologyMismatch::Indices:
            return "indices";
        case WeightTopologyMismatch::DrawCount:
            return "draw-count";
        case WeightTopologyMismatch::DrawRange:
            return "draw-range";
        }
        return "unknown";
    }

    struct WeightInterpolationResult
    {
        WeightInterpolationError error = WeightInterpolationError::EmptyMesh;
        WeightTopologyMismatch topologyMismatch = WeightTopologyMismatch::None;
        std::size_t mismatchIndex = 0;
        PreviewMesh mesh;

        explicit operator bool() const
        {
            return error == WeightInterpolationError::None;
        }
    };

    inline bool IsFinite(Float2 a_value)
    {
        return std::isfinite(a_value.x) && std::isfinite(a_value.y);
    }

    inline bool IsFinite(Float3 a_value)
    {
        return std::isfinite(a_value.x) && std::isfinite(a_value.y) &&
               std::isfinite(a_value.z);
    }

    inline bool IsFinite(Float4 a_value)
    {
        return std::isfinite(a_value.x) && std::isfinite(a_value.y) &&
               std::isfinite(a_value.z) && std::isfinite(a_value.w);
    }

    inline float Lerp(float a_low, float a_high, float a_weight)
    {
        return a_low + (a_high - a_low) * a_weight;
    }

    inline Float3 Lerp(Float3 a_low, Float3 a_high, float a_weight)
    {
        return {
            Lerp(a_low.x, a_high.x, a_weight),
            Lerp(a_low.y, a_high.y, a_weight),
            Lerp(a_low.z, a_high.z, a_weight),
        };
    }

    inline float SelectTangentHandedness(float a_low, float a_high, float a_weight)
    {
        const auto preferred = a_weight < 0.5f ? a_low : a_high;
        const auto alternate = a_weight < 0.5f ? a_high : a_low;
        if (std::isfinite(preferred) && std::abs(preferred) > 1.0e-6f)
        {
            return std::signbit(preferred) ? -1.0f : 1.0f;
        }
        if (std::isfinite(alternate) && std::abs(alternate) > 1.0e-6f)
        {
            return std::signbit(alternate) ? -1.0f : 1.0f;
        }
        return 1.0f;
    }

    inline WeightInterpolationResult InterpolateWeightMeshes(
        const PreviewMesh& a_low,
        const PreviewMesh& a_high,
        float a_weight)
    {
        if (!std::isfinite(a_weight) || a_weight < 0.0f || a_weight > 1.0f)
        {
            return {.error = WeightInterpolationError::InvalidWeight};
        }
        if (a_low.Empty() || a_high.Empty() || a_low.draws.empty() ||
            a_high.draws.empty())
        {
            return {.error = WeightInterpolationError::EmptyMesh};
        }
        if (a_low.vertices.size() != a_high.vertices.size())
        {
            return {
                .error = WeightInterpolationError::TopologyMismatch,
                .topologyMismatch = WeightTopologyMismatch::VertexCount,
            };
        }
        if (a_low.indices != a_high.indices)
        {
            return {
                .error = WeightInterpolationError::TopologyMismatch,
                .topologyMismatch = WeightTopologyMismatch::Indices,
            };
        }
        if (a_low.draws.size() != a_high.draws.size())
        {
            return {
                .error = WeightInterpolationError::TopologyMismatch,
                .topologyMismatch = WeightTopologyMismatch::DrawCount,
            };
        }
        for (std::size_t index = 0; index < a_low.draws.size(); ++index)
        {
            const auto& lowDraw = a_low.draws[index];
            const auto& highDraw = a_high.draws[index];
            if (lowDraw.startIndex != highDraw.startIndex ||
                lowDraw.indexCount != highDraw.indexCount)
            {
                return {
                    .error = WeightInterpolationError::TopologyMismatch,
                    .topologyMismatch = WeightTopologyMismatch::DrawRange,
                    .mismatchIndex = index,
                };
            }
        }

        WeightInterpolationResult result{
            .error = WeightInterpolationError::None,
            .mesh = a_low,
        };
        result.mesh.bounds = {};
        for (std::size_t index = 0; index < result.mesh.vertices.size(); ++index)
        {
            const auto& low = a_low.vertices[index];
            const auto& high = a_high.vertices[index];
            if (!IsFinite(low.position) || !IsFinite(high.position) ||
                !IsFinite(low.normal) || !IsFinite(high.normal) ||
                !IsFinite(low.tangent) || !IsFinite(high.tangent) ||
                !IsFinite(low.textureCoordinate) || !IsFinite(high.textureCoordinate))
            {
                return {.error = WeightInterpolationError::NonFiniteVertex};
            }
            auto& output = result.mesh.vertices[index];
            output.position = Lerp(low.position, high.position, a_weight);
            output.normal = Normalize(Lerp(low.normal, high.normal, a_weight));
            const auto tangent = Normalize(
                Lerp({low.tangent.x, low.tangent.y, low.tangent.z},
                     {high.tangent.x, high.tangent.y, high.tangent.z},
                     a_weight));
            output.tangent = {
                tangent.x,
                tangent.y,
                tangent.z,
                SelectTangentHandedness(low.tangent.w, high.tangent.w, a_weight),
            };
            result.mesh.bounds.Include(output.position);
        }
        return result;
    }
}
