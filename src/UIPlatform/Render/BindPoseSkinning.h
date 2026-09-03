#pragma once

#include "Render/NifPreviewMesh.h"

#include <algorithm>
#include <cmath>
#include <span>
#include <vector>

namespace Meridian::Render::NifPreview
{
    struct AffineTransform
    {
        float rotation[3][3] = {
            {1.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f},
            {0.0f, 0.0f, 1.0f},
        };
        Float3 translation{};
        float scale = 1.0f;
    };

    struct BindPoseBone
    {
        AffineTransform boneToModel{};
        AffineTransform skinToBone{};
    };

    enum class SkinningError
    {
        None,
        EmptyMesh,
        MissingWeights,
        InvalidBoneIndex,
        InvalidTransform,
        NonFiniteVertex,
    };

    namespace SkinningDetail
    {
        inline bool IsFinite(Float3 a_value)
        {
            return std::isfinite(a_value.x) &&
                   std::isfinite(a_value.y) &&
                   std::isfinite(a_value.z);
        }

        inline bool IsFinite(const AffineTransform& a_transform)
        {
            if (!IsFinite(a_transform.translation) ||
                !std::isfinite(a_transform.scale) ||
                std::abs(a_transform.scale) <= 1.0e-8f)
            {
                return false;
            }
            for (const auto& row : a_transform.rotation)
            {
                for (const auto component : row)
                {
                    if (!std::isfinite(component))
                    {
                        return false;
                    }
                }
            }
            return true;
        }

        inline Float3 Rotate(const AffineTransform& a_transform, Float3 a_value)
        {
            return {
                a_transform.rotation[0][0] * a_value.x +
                    a_transform.rotation[0][1] * a_value.y +
                    a_transform.rotation[0][2] * a_value.z,
                a_transform.rotation[1][0] * a_value.x +
                    a_transform.rotation[1][1] * a_value.y +
                    a_transform.rotation[1][2] * a_value.z,
                a_transform.rotation[2][0] * a_value.x +
                    a_transform.rotation[2][1] * a_value.y +
                    a_transform.rotation[2][2] * a_value.z,
            };
        }

        inline Float3 TransformPoint(const AffineTransform& a_transform, Float3 a_value)
        {
            a_value.x *= a_transform.scale;
            a_value.y *= a_transform.scale;
            a_value.z *= a_transform.scale;
            return Rotate(a_transform, a_value) + a_transform.translation;
        }

        inline Float3 TransformDirection(const AffineTransform& a_transform, Float3 a_value)
        {
            auto rotated = Rotate(a_transform, a_value);
            if (a_transform.scale < 0.0f)
            {
                rotated.x = -rotated.x;
                rotated.y = -rotated.y;
                rotated.z = -rotated.z;
            }
            return rotated;
        }

        inline AffineTransform Compose(const AffineTransform& a_parent,
                                       const AffineTransform& a_local)
        {
            AffineTransform result{};
            for (std::size_t row = 0; row < 3; ++row)
            {
                for (std::size_t column = 0; column < 3; ++column)
                {
                    result.rotation[row][column] = 0.0f;
                    for (std::size_t component = 0; component < 3; ++component)
                    {
                        result.rotation[row][column] +=
                            a_parent.rotation[row][component] *
                            a_local.rotation[component][column];
                    }
                }
            }
            result.scale = a_parent.scale * a_local.scale;
            const Float3 localTranslation{
                a_local.translation.x * a_parent.scale,
                a_local.translation.y * a_parent.scale,
                a_local.translation.z * a_parent.scale,
            };
            result.translation = Rotate(a_parent, localTranslation) + a_parent.translation;
            return result;
        }
    }

    inline bool TryInvertAffine(const AffineTransform& a_transform,
                                 AffineTransform& a_inverse)
    {
        if (!SkinningDetail::IsFinite(a_transform))
        {
            return false;
        }

        const auto& matrix = a_transform.rotation;
        const float determinant =
            matrix[0][0] * (matrix[1][1] * matrix[2][2] -
                            matrix[1][2] * matrix[2][1]) -
            matrix[0][1] * (matrix[1][0] * matrix[2][2] -
                            matrix[1][2] * matrix[2][0]) +
            matrix[0][2] * (matrix[1][0] * matrix[2][1] -
                            matrix[1][1] * matrix[2][0]);
        if (!std::isfinite(determinant) || std::abs(determinant) <= 1.0e-8f)
        {
            return false;
        }

        AffineTransform inverse{};
        const float inverseDeterminant = 1.0f / determinant;
        inverse.rotation[0][0] =
            (matrix[1][1] * matrix[2][2] - matrix[1][2] * matrix[2][1]) *
            inverseDeterminant;
        inverse.rotation[0][1] =
            (matrix[0][2] * matrix[2][1] - matrix[0][1] * matrix[2][2]) *
            inverseDeterminant;
        inverse.rotation[0][2] =
            (matrix[0][1] * matrix[1][2] - matrix[0][2] * matrix[1][1]) *
            inverseDeterminant;
        inverse.rotation[1][0] =
            (matrix[1][2] * matrix[2][0] - matrix[1][0] * matrix[2][2]) *
            inverseDeterminant;
        inverse.rotation[1][1] =
            (matrix[0][0] * matrix[2][2] - matrix[0][2] * matrix[2][0]) *
            inverseDeterminant;
        inverse.rotation[1][2] =
            (matrix[0][2] * matrix[1][0] - matrix[0][0] * matrix[1][2]) *
            inverseDeterminant;
        inverse.rotation[2][0] =
            (matrix[1][0] * matrix[2][1] - matrix[1][1] * matrix[2][0]) *
            inverseDeterminant;
        inverse.rotation[2][1] =
            (matrix[0][1] * matrix[2][0] - matrix[0][0] * matrix[2][1]) *
            inverseDeterminant;
        inverse.rotation[2][2] =
            (matrix[0][0] * matrix[1][1] - matrix[0][1] * matrix[1][0]) *
            inverseDeterminant;
        inverse.scale = 1.0f / a_transform.scale;

        const auto rotatedTranslation =
            SkinningDetail::Rotate(inverse, a_transform.translation);
        inverse.translation = {
            -rotatedTranslation.x * inverse.scale,
            -rotatedTranslation.y * inverse.scale,
            -rotatedTranslation.z * inverse.scale,
        };
        if (!SkinningDetail::IsFinite(inverse))
        {
            return false;
        }

        a_inverse = inverse;
        return true;
    }

    inline bool TryMakeRelativeTransform(const AffineTransform& a_rootWorld,
                                         const AffineTransform& a_objectWorld,
                                         AffineTransform& a_relative)
    {
        AffineTransform worldToRoot{};
        if (!TryInvertAffine(a_rootWorld, worldToRoot) ||
            !SkinningDetail::IsFinite(a_objectWorld))
        {
            return false;
        }

        const auto relative = SkinningDetail::Compose(worldToRoot, a_objectWorld);
        if (!SkinningDetail::IsFinite(relative))
        {
            return false;
        }
        a_relative = relative;
        return true;
    }

    inline SkinningError ApplyBindPoseSkinning(PreviewMesh& a_mesh,
                                               std::span<const BindPoseBone> a_bones)
    {
        if (a_mesh.Empty())
        {
            return SkinningError::EmptyMesh;
        }

        std::vector<AffineTransform> skinToModel;
        skinToModel.reserve(a_bones.size());
        for (const auto& bone : a_bones)
        {
            if (!SkinningDetail::IsFinite(bone.boneToModel) ||
                !SkinningDetail::IsFinite(bone.skinToBone))
            {
                return SkinningError::InvalidTransform;
            }
            auto combined = SkinningDetail::Compose(bone.boneToModel, bone.skinToBone);
            if (!SkinningDetail::IsFinite(combined))
            {
                return SkinningError::InvalidTransform;
            }
            skinToModel.push_back(combined);
        }

        auto output = a_mesh.vertices;
        PreviewBounds bounds{};
        for (std::size_t vertexIndex = 0; vertexIndex < a_mesh.vertices.size(); ++vertexIndex)
        {
            const auto& source = a_mesh.vertices[vertexIndex];
            Float3 position{};
            Float3 normal{};
            Float3 tangent{};
            float weightSum = 0.0f;
            float strongestWeight = -1.0f;
            float tangentSign = source.tangent.w;

            for (std::size_t influence = 0; influence < source.boneWeights.size(); ++influence)
            {
                const auto weight = source.boneWeights[influence];
                if (!std::isfinite(weight) || weight < 0.0f)
                {
                    return SkinningError::NonFiniteVertex;
                }
                if (weight <= 1.0e-8f)
                {
                    continue;
                }
                const auto boneIndex = source.boneIndices[influence];
                if (boneIndex >= skinToModel.size())
                {
                    return SkinningError::InvalidBoneIndex;
                }

                const auto& transform = skinToModel[boneIndex];
                const auto transformedPosition =
                    SkinningDetail::TransformPoint(transform, source.position);
                const auto transformedNormal =
                    SkinningDetail::TransformDirection(transform, source.normal);
                const auto transformedTangent = SkinningDetail::TransformDirection(
                    transform,
                    {source.tangent.x, source.tangent.y, source.tangent.z});
                position += Float3{
                    transformedPosition.x * weight,
                    transformedPosition.y * weight,
                    transformedPosition.z * weight,
                };
                normal += Float3{
                    transformedNormal.x * weight,
                    transformedNormal.y * weight,
                    transformedNormal.z * weight,
                };
                tangent += Float3{
                    transformedTangent.x * weight,
                    transformedTangent.y * weight,
                    transformedTangent.z * weight,
                };
                weightSum += weight;
                if (weight > strongestWeight)
                {
                    strongestWeight = weight;
                    tangentSign = transform.scale < 0.0f ? -source.tangent.w : source.tangent.w;
                }
            }

            if (!std::isfinite(weightSum) || weightSum <= 1.0e-6f)
            {
                return SkinningError::MissingWeights;
            }
            const auto inverseWeight = 1.0f / weightSum;
            position.x *= inverseWeight;
            position.y *= inverseWeight;
            position.z *= inverseWeight;
            if (!SkinningDetail::IsFinite(position) ||
                !SkinningDetail::IsFinite(normal) ||
                !SkinningDetail::IsFinite(tangent))
            {
                return SkinningError::NonFiniteVertex;
            }

            auto& destination = output[vertexIndex];
            destination.position = position;
            destination.normal = Normalize(normal);
            const auto normalizedTangent = Normalize(tangent);
            destination.tangent = {
                normalizedTangent.x,
                normalizedTangent.y,
                normalizedTangent.z,
                tangentSign,
            };
            bounds.Include(position);
        }

        a_mesh.vertices = std::move(output);
        a_mesh.bounds = bounds;
        return SkinningError::None;
    }
}
