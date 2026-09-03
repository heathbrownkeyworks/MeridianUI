#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace Meridian::Render::NifPreview
{
    enum class MaterialTintKind : std::uint8_t
    {
        None,
        Body,
        Hair,
    };

    enum class PreviewMaterialFeature : std::uint8_t
    {
        Generic,
        FaceGen,
        FaceGenRgbTint,
        HairTint,
    };

    struct Float2
    {
        float x = 0.0f;
        float y = 0.0f;
    };

    struct Float3
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    struct Float4
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float w = 1.0f;
    };

    inline Float3 operator+(Float3 a_left, Float3 a_right)
    {
        return {a_left.x + a_right.x, a_left.y + a_right.y, a_left.z + a_right.z};
    }

    inline Float3 operator-(Float3 a_left, Float3 a_right)
    {
        return {a_left.x - a_right.x, a_left.y - a_right.y, a_left.z - a_right.z};
    }

    inline Float3& operator+=(Float3& a_left, Float3 a_right)
    {
        a_left = a_left + a_right;
        return a_left;
    }

    inline Float3 Cross(Float3 a_left, Float3 a_right)
    {
        return {
            a_left.y * a_right.z - a_left.z * a_right.y,
            a_left.z * a_right.x - a_left.x * a_right.z,
            a_left.x * a_right.y - a_left.y * a_right.x,
        };
    }

    inline Float3 Normalize(Float3 a_value)
    {
        const auto lengthSquared =
            a_value.x * a_value.x + a_value.y * a_value.y + a_value.z * a_value.z;
        if (!std::isfinite(lengthSquared) || lengthSquared <= 1.0e-12f)
        {
            return {0.0f, 0.0f, 1.0f};
        }
        const auto inverseLength = 1.0f / std::sqrt(lengthSquared);
        return {a_value.x * inverseLength, a_value.y * inverseLength, a_value.z * inverseLength};
    }

    struct PreviewVertex
    {
        Float3 position{};
        Float3 normal{0.0f, 0.0f, 1.0f};
        Float4 tangent{1.0f, 0.0f, 0.0f, 1.0f};
        Float2 textureCoordinate{};
        std::array<float, 4> boneWeights{};
        std::array<std::uint16_t, 4> boneIndices{};
    };

    struct PreviewMaterial
    {
        std::string diffuseTexturePath;
        std::string normalTexturePath;
        std::string specularTexturePath;
        std::string environmentTexturePath;
        std::string environmentMaskTexturePath;
        std::string faceTintTexturePath;
        std::string faceDetailTexturePath;
        std::string faceSubsurfaceTexturePath;
        Float2 textureOffset{};
        Float2 textureScale{1.0f, 1.0f};
        Float3 specularColor{1.0f, 1.0f, 1.0f};
        Float3 tintColor{1.0f, 1.0f, 1.0f};
        float opacity = 1.0f;
        float alphaThreshold = 0.0f;
        float specularPower = 1.0f;
        float specularStrength = 1.0f;
        float environmentScale = 0.0f;
        bool alphaBlend = false;
        bool alphaTest = false;
        bool premultipliedAlpha = false;
        bool specularEnabled = false;
        bool tangentSpaceNormal = false;
        bool modelSpaceNormal = false;
        bool environmentEnabled = false;
        PreviewMaterialFeature feature = PreviewMaterialFeature::Generic;
        MaterialTintKind tintKind = MaterialTintKind::None;
        // Unsupported shaders keep the historical no-cull behavior so a
        // missing material never makes otherwise valid geometry disappear.
        bool twoSided = true;
    };

    struct PreviewDrawRange
    {
        std::uint64_t sceneObject = 0;
        std::uint32_t startIndex = 0;
        std::uint32_t indexCount = 0;
        PreviewMaterial material{};
    };

    struct PreviewBounds
    {
        Float3 minimum{
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
        };
        Float3 maximum{
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest(),
        };
        bool valid = false;

        void Include(Float3 a_point)
        {
            minimum.x = std::min(minimum.x, a_point.x);
            minimum.y = std::min(minimum.y, a_point.y);
            minimum.z = std::min(minimum.z, a_point.z);
            maximum.x = std::max(maximum.x, a_point.x);
            maximum.y = std::max(maximum.y, a_point.y);
            maximum.z = std::max(maximum.z, a_point.z);
            valid = true;
        }

        void Include(const PreviewBounds& a_other)
        {
            if (!a_other.valid)
            {
                return;
            }
            Include(a_other.minimum);
            Include(a_other.maximum);
        }
    };

    struct PreviewMesh
    {
        std::vector<PreviewVertex> vertices;
        std::vector<std::uint32_t> indices;
        std::vector<PreviewDrawRange> draws;
        PreviewBounds bounds{};

        bool Empty() const
        {
            return vertices.empty() || indices.empty() || !bounds.valid;
        }
    };
}
