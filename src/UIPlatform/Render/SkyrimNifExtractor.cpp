#include "Render/SkyrimNifExtractor.h"

#include "Render/BindPoseSkinning.h"
#include "Render/NifTextureOverride.h"
#include "Render/PackedGeometryDecoder.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <wrl/client.h>

namespace Meridian::Render::NifPreview
{
    namespace
    {
        inline constexpr std::size_t MAX_MODEL_NODES = 100'000;
        inline constexpr std::size_t MAX_MODEL_DEPTH = 1'024;
        inline constexpr std::size_t MAX_SKIN_BONES = 512;
        inline constexpr std::size_t MAX_SKIN_PARTITIONS = 256;
        inline constexpr std::size_t MAX_PARTITION_BONES = 256;

        using ObjectTransformMap = std::unordered_map<const RE::NiAVObject*, AffineTransform>;

        AffineTransform ToAffineTransform(const RE::NiTransform& a_transform)
        {
            AffineTransform result{};
            for (std::size_t row = 0; row < 3; ++row)
            {
                for (std::size_t column = 0; column < 3; ++column)
                {
                    result.rotation[row][column] = a_transform.rotate.entry[row][column];
                }
            }
            result.translation = {
                a_transform.translate.x,
                a_transform.translate.y,
                a_transform.translate.z,
            };
            result.scale = a_transform.scale;
            return result;
        }

        bool CollectObjectTransforms(RE::NiAVObject* a_object,
                                     const AffineTransform& a_parent,
                                     ObjectTransformMap& a_transforms,
                                     std::size_t a_depth)
        {
            if (a_object == nullptr)
            {
                return true;
            }
            if (a_depth > MAX_MODEL_DEPTH || a_transforms.size() >= MAX_MODEL_NODES)
            {
                return false;
            }
            if (a_transforms.contains(a_object))
            {
                return true;
            }

            const auto modelTransform = SkinningDetail::Compose(
                a_parent, ToAffineTransform(a_object->local));
            if (!SkinningDetail::IsFinite(modelTransform))
            {
                return false;
            }
            a_transforms.emplace(a_object, modelTransform);
            if (auto* node = a_object->AsNode(); node != nullptr)
            {
                for (const auto& child : node->GetChildren())
                {
                    if (child != nullptr &&
                        !CollectObjectTransforms(
                            child.get(), modelTransform, a_transforms, a_depth + 1))
                    {
                        return false;
                    }
                }
            }
            return true;
        }

        bool CollectLiveObjectTransforms(RE::NiAVObject* a_object,
                                         const AffineTransform& a_actorWorld,
                                         ObjectTransformMap& a_transforms,
                                         std::size_t a_depth)
        {
            if (a_object == nullptr)
            {
                return true;
            }
            if (a_depth > MAX_MODEL_DEPTH || a_transforms.size() >= MAX_MODEL_NODES)
            {
                return false;
            }
            if (a_transforms.contains(a_object))
            {
                return true;
            }

            AffineTransform actorRelative{};
            if (!TryMakeRelativeTransform(
                    a_actorWorld, ToAffineTransform(a_object->world), actorRelative))
            {
                return false;
            }
            a_transforms.emplace(a_object, actorRelative);
            if (auto* node = a_object->AsNode(); node != nullptr)
            {
                for (const auto& child : node->GetChildren())
                {
                    if (child != nullptr &&
                        !CollectLiveObjectTransforms(
                            child.get(), a_actorWorld, a_transforms, a_depth + 1))
                    {
                        return false;
                    }
                }
            }
            return true;
        }

        bool CopyGpuBuffer(ID3D11Device* a_device,
                           ID3D11DeviceContext* a_context,
                           ID3D11Buffer* a_source,
                           std::size_t a_requiredBytes,
                           std::vector<std::uint8_t>& a_output)
        {
            if (a_device == nullptr || a_context == nullptr || a_source == nullptr || a_requiredBytes == 0)
            {
                return false;
            }

            D3D11_BUFFER_DESC sourceDesc{};
            a_source->GetDesc(&sourceDesc);
            if (a_requiredBytes > sourceDesc.ByteWidth)
            {
                return false;
            }

            D3D11_BUFFER_DESC stagingDesc = sourceDesc;
            stagingDesc.Usage = D3D11_USAGE_STAGING;
            stagingDesc.BindFlags = 0;
            stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            stagingDesc.MiscFlags = 0;
            stagingDesc.StructureByteStride = 0;

            Microsoft::WRL::ComPtr<ID3D11Buffer> staging;
            if (FAILED(a_device->CreateBuffer(&stagingDesc, nullptr, staging.GetAddressOf())))
            {
                return false;
            }

            a_context->CopyResource(staging.Get(), a_source);
            D3D11_MAPPED_SUBRESOURCE mapped{};
            if (FAILED(a_context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
            {
                return false;
            }

            a_output.resize(a_requiredBytes);
            std::memcpy(a_output.data(), mapped.pData, a_requiredBytes);
            a_context->Unmap(staging.Get(), 0);
            return true;
        }

        enum class ShapeResult
        {
            Accepted,
            Unsupported,
            Malformed,
            ReadbackFailed,
        };

        PreviewMaterial ExtractMaterial(RE::BSTriShape* a_shape,
                                        bool a_preferRuntimeTextures)
        {
            PreviewMaterial material{};
            if (a_shape == nullptr)
            {
                return material;
            }

            const auto& geometryData = a_shape->GetGeometryRuntimeData();
            auto* shader = a_shape->lightingShaderProp_cast();
            if (shader == nullptr)
            {
                return material;
            }

            material.twoSided = shader->flags.all(
                RE::BSShaderProperty::EShaderPropertyFlag::kTwoSided);
            material.premultipliedAlpha = shader->flags.all(
                RE::BSShaderProperty::EShaderPropertyFlag::kPremultAlpha);
            material.specularEnabled = shader->flags.all(
                RE::BSShaderProperty::EShaderPropertyFlag::kSpecular);
            const auto usesModelSpaceNormals = shader->flags.all(
                RE::BSShaderProperty::EShaderPropertyFlag::kModelSpaceNormals);
            material.opacity = std::clamp(shader->QMaterialAlpha(), 0.0f, 1.0f);

            if (auto* alpha = geometryData.alphaProperty.get(); alpha != nullptr)
            {
                material.alphaBlend = alpha->GetAlphaBlending();
                material.alphaTest = alpha->GetAlphaTesting();
                material.alphaThreshold = static_cast<float>(alpha->alphaThreshold) / 255.0f;
            }

            auto* baseMaterial = shader->GetBaseMaterial();
            if (baseMaterial == nullptr ||
                baseMaterial->GetType() != RE::BSShaderMaterial::Type::kLighting)
            {
                return material;
            }
            auto* lightingMaterial = static_cast<RE::BSLightingShaderMaterialBase*>(baseMaterial);
            material.specularColor = {
                lightingMaterial->specularColor.red,
                lightingMaterial->specularColor.green,
                lightingMaterial->specularColor.blue,
            };
            material.specularPower = std::clamp(lightingMaterial->specularPower, 1.0f, 256.0f);
            material.specularStrength = std::clamp(
                lightingMaterial->specularColorScale, 0.0f, 16.0f);
            material.textureOffset = {
                lightingMaterial->texCoordOffset[0].x,
                lightingMaterial->texCoordOffset[0].y,
            };
            material.textureScale = {
                lightingMaterial->texCoordScale[0].x,
                lightingMaterial->texCoordScale[0].y,
            };
            const auto copyTint = [&](const RE::NiColor& a_color,
                                      MaterialTintKind a_kind) {
                const Float3 tint{a_color.red, a_color.green, a_color.blue};
                if (IsValidMaterialTint(tint))
                {
                    material.tintColor = tint;
                    material.tintKind = a_kind;
                }
            };
            switch (baseMaterial->GetFeature())
            {
            case RE::BSShaderMaterial::Feature::kFaceGen:
            {
                material.feature = PreviewMaterialFeature::FaceGen;
                auto* faceGen = static_cast<RE::BSLightingShaderMaterialFacegen*>(
                    lightingMaterial);
                const auto copySourceTexturePath = [](RE::NiSourceTexture* a_texture,
                                                      std::string& a_destination) {
                    if (a_texture == nullptr)
                    {
                        return;
                    }
                    const auto* name = a_texture->name.c_str();
                    if (name != nullptr && name[0] != '\0')
                    {
                        a_destination = name;
                    }
                };
                copySourceTexturePath(faceGen->tintTexture.get(),
                                      material.faceTintTexturePath);
                copySourceTexturePath(faceGen->detailTexture.get(),
                                      material.faceDetailTexturePath);
                copySourceTexturePath(faceGen->subsurfaceTexture.get(),
                                      material.faceSubsurfaceTexturePath);
                break;
            }
            case RE::BSShaderMaterial::Feature::kFaceGenRGBTint:
                material.feature = PreviewMaterialFeature::FaceGenRgbTint;
                copyTint(
                    static_cast<RE::BSLightingShaderMaterialFacegenTint*>(
                        lightingMaterial)->tintColor,
                    MaterialTintKind::Body);
                break;
            case RE::BSShaderMaterial::Feature::kHairTint:
                material.feature = PreviewMaterialFeature::HairTint;
                copyTint(
                    static_cast<RE::BSLightingShaderMaterialHairTint*>(
                        lightingMaterial)->tintColor,
                    MaterialTintKind::Hair);
                break;
            default:
                break;
            }
            if (const auto textureSet = lightingMaterial->GetTextureSet(); textureSet != nullptr)
            {
                const auto readTexturePath = [&](std::uint32_t a_slot, std::string& a_destination) {
                    if (const auto* path = textureSet->GetTexturePath(
                            static_cast<RE::BSTextureSet::Texture>(a_slot));
                        path != nullptr)
                    {
                        a_destination = path;
                    }
                };
                readTexturePath(RE::BSTextureSet::Textures::kDiffuse,
                                material.diffuseTexturePath);
                readTexturePath(RE::BSTextureSet::Textures::kNormal,
                                material.normalTexturePath);
                readTexturePath(RE::BSTextureSet::Textures::kEnvironment,
                                material.environmentTexturePath);
                readTexturePath(RE::BSTextureSet::Textures::kEnvironmentMask,
                                material.environmentMaskTexturePath);
                readTexturePath(RE::BSTextureSet::Textures::kSpecular,
                                material.specularTexturePath);
                if (material.feature == PreviewMaterialFeature::FaceGen)
                {
                    readTexturePath(RE::BSTextureSet::Textures::kMultilayer,
                                    material.faceTintTexturePath);
                    readTexturePath(RE::BSTextureSet::Textures::kDetailMap,
                                    material.faceDetailTexturePath);
                    readTexturePath(RE::BSTextureSet::Textures::kSubsurfaceTint,
                                    material.faceSubsurfaceTexturePath);
                }
                material.tangentSpaceNormal =
                    !usesModelSpaceNormals && !material.normalTexturePath.empty();
                material.modelSpaceNormal =
                    usesModelSpaceNormals && !material.normalTexturePath.empty();

                if (baseMaterial->GetFeature() == RE::BSShaderMaterial::Feature::kEnvironmentMap)
                {
                    auto* envMaterial = static_cast<RE::BSLightingShaderMaterialEnvmap*>(
                        lightingMaterial);
                    material.environmentScale = std::clamp(envMaterial->envMapScale, 0.0f, 16.0f);
                    material.environmentEnabled =
                        !material.environmentTexturePath.empty() &&
                        shader->flags.all(RE::BSShaderProperty::EShaderPropertyFlag::kEnvMap);
                }
            }
            if (a_preferRuntimeTextures)
            {
                const auto copySourceTexturePath = [](RE::NiSourceTexture* a_texture,
                                                      std::string& a_destination) {
                    if (a_texture == nullptr)
                    {
                        return;
                    }
                    const auto* name = a_texture->name.c_str();
                    if (name != nullptr && name[0] != '\0')
                    {
                        a_destination = name;
                    }
                };
                copySourceTexturePath(lightingMaterial->diffuseTexture.get(),
                                      material.diffuseTexturePath);
                copySourceTexturePath(lightingMaterial->normalTexture.get(),
                                      material.normalTexturePath);
                copySourceTexturePath(lightingMaterial->specularBackLightingTexture.get(),
                                      material.specularTexturePath);
                material.tangentSpaceNormal =
                    !usesModelSpaceNormals && !material.normalTexturePath.empty();
                material.modelSpaceNormal =
                    usesModelSpaceNormals && !material.normalTexturePath.empty();
            }
            return material;
        }

        ShapeResult DecodeRendererGeometry(RE::BSGraphics::TriShape* a_rendererData,
                                           std::size_t a_vertexCount,
                                           std::size_t a_indexCount,
                                           bool a_requireSkinning,
                                           ID3D11Device* a_device,
                                           ID3D11DeviceContext* a_context,
                                           PreviewMesh& a_output,
                                           PackedPositionStream a_positionStream = {})
        {
            if (a_rendererData == nullptr || a_vertexCount == 0 || a_indexCount == 0)
            {
                return ShapeResult::Unsupported;
            }
            auto vertexDesc = a_rendererData->vertexDesc;
            if ((!vertexDesc.HasFlag(RE::BSGraphics::Vertex::VF_VERTEX) &&
                 a_positionStream.bytes.empty()) ||
                (a_requireSkinning &&
                 !vertexDesc.HasFlag(RE::BSGraphics::Vertex::VF_SKINNED)))
            {
                return ShapeResult::Unsupported;
            }
            const auto stride = static_cast<std::size_t>(vertexDesc.GetSize());
            if (stride == 0 || a_vertexCount > MAX_PREVIEW_VERTICES ||
                a_indexCount > MAX_PREVIEW_INDICES)
            {
                return ShapeResult::Malformed;
            }

            const auto vertexBytesRequired = a_vertexCount * stride;
            const auto indexBytesRequired = a_indexCount * sizeof(std::uint16_t);
            std::vector<std::uint8_t> ownedVertexBytes;
            std::vector<std::uint8_t> ownedIndexBytes;
            std::span<const std::uint8_t> vertexBytes;
            std::span<const std::uint16_t> indices;

            if (a_rendererData->rawVertexData != nullptr)
            {
                vertexBytes = {a_rendererData->rawVertexData, vertexBytesRequired};
            }
            else
            {
                auto* vertexBuffer =
                    reinterpret_cast<ID3D11Buffer*>(a_rendererData->vertexBuffer);
                if (!CopyGpuBuffer(a_device, a_context, vertexBuffer, vertexBytesRequired, ownedVertexBytes))
                {
                    return ShapeResult::ReadbackFailed;
                }
                vertexBytes = ownedVertexBytes;
            }

            if (a_rendererData->rawIndexData != nullptr)
            {
                indices = {a_rendererData->rawIndexData, a_indexCount};
            }
            else
            {
                auto* indexBuffer =
                    reinterpret_cast<ID3D11Buffer*>(a_rendererData->indexBuffer);
                if (!CopyGpuBuffer(a_device, a_context, indexBuffer, indexBytesRequired, ownedIndexBytes))
                {
                    return ShapeResult::ReadbackFailed;
                }
                indices = {
                    reinterpret_cast<const std::uint16_t*>(ownedIndexBytes.data()),
                    a_indexCount,
                };
            }

            PackedGeometryLayout layout{};
            layout.stride = stride;
            if (vertexDesc.HasFlag(RE::BSGraphics::Vertex::VF_VERTEX))
            {
                layout.positionOffset =
                    vertexDesc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_POSITION);
            }
            layout.hasPackedNormal = vertexDesc.HasFlag(RE::BSGraphics::Vertex::VF_NORMAL);
            if (layout.hasPackedNormal)
            {
                layout.normalOffset = vertexDesc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_NORMAL);
            }
            layout.hasPackedTangent = vertexDesc.HasFlag(RE::BSGraphics::Vertex::VF_TANGENT);
            if (layout.hasPackedTangent)
            {
                layout.tangentOffset =
                    vertexDesc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_BINORMAL);
            }
            layout.hasTextureCoordinate = vertexDesc.HasFlag(RE::BSGraphics::Vertex::VF_UV);
            if (layout.hasTextureCoordinate)
            {
                layout.textureCoordinateOffset =
                    vertexDesc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_TEXCOORD0);
            }
            layout.hasSkinning = vertexDesc.HasFlag(RE::BSGraphics::Vertex::VF_SKINNED);
            if (layout.hasSkinning)
            {
                layout.skinningOffset =
                    vertexDesc.GetAttributeOffset(RE::BSGraphics::Vertex::VA_SKINNING);
            }

            auto decoded = DecodePackedGeometry(
                vertexBytes,
                a_vertexCount,
                indices,
                layout,
                a_positionStream);
            if (decoded.error != DecodeError::None)
            {
                return ShapeResult::Malformed;
            }
            a_output = std::move(decoded.mesh);
            return ShapeResult::Accepted;
        }

        void ApplyMaterial(PreviewMesh& a_mesh, const PreviewMaterial& a_material)
        {
            for (auto& vertex : a_mesh.vertices)
            {
                vertex.textureCoordinate.x =
                    vertex.textureCoordinate.x * a_material.textureScale.x +
                    a_material.textureOffset.x;
                vertex.textureCoordinate.y =
                    vertex.textureCoordinate.y * a_material.textureScale.y +
                    a_material.textureOffset.y;
            }
            if (!a_mesh.draws.empty())
            {
                a_mesh.draws.front().material = a_material;
            }
        }

        ShapeResult ExtractRigidShape(RE::BSTriShape* a_shape,
                                      const AffineTransform& a_transform,
                                      ID3D11Device* a_device,
                                      ID3D11DeviceContext* a_context,
                                      const ModelTextureOverride* a_textureOverride,
                                      const ActorMaterialTintOverride* a_materialTint,
                                      bool a_preferRuntimeTextures,
                                      std::size_t& a_matchedMaterialTintOverrides,
                                      PreviewMesh& a_combined)
        {
            const auto& geometryData = a_shape->GetGeometryRuntimeData();
            const auto& shapeData = a_shape->GetTrishapeRuntimeData();
            PreviewMesh decoded{};
            const auto decodeResult = DecodeRendererGeometry(
                geometryData.rendererData,
                static_cast<std::size_t>(shapeData.vertexCount),
                static_cast<std::size_t>(shapeData.triangleCount) * 3,
                false,
                a_device,
                a_context,
                decoded);
            if (decodeResult != ShapeResult::Accepted)
            {
                return decodeResult;
            }

            decoded.bounds = {};
            auto material = ExtractMaterial(a_shape, a_preferRuntimeTextures);
            if (a_textureOverride != nullptr)
            {
                ApplyTextureOverride(material, *a_textureOverride);
            }
            if (a_materialTint != nullptr &&
                ApplyActorMaterialTint(material, *a_materialTint))
            {
                ++a_matchedMaterialTintOverrides;
            }
            for (auto& vertex : decoded.vertices)
            {
                vertex.position = SkinningDetail::TransformPoint(a_transform, vertex.position);
                vertex.normal = Normalize(
                    SkinningDetail::TransformDirection(a_transform, vertex.normal));
                const auto rotatedTangent = SkinningDetail::TransformDirection(
                    a_transform,
                    {vertex.tangent.x, vertex.tangent.y, vertex.tangent.z});
                const auto normalizedTangent = Normalize(rotatedTangent);
                vertex.tangent.x = normalizedTangent.x;
                vertex.tangent.y = normalizedTangent.y;
                vertex.tangent.z = normalizedTangent.z;
                if (a_transform.scale < 0.0f)
                {
                    vertex.tangent.w = -vertex.tangent.w;
                }
                decoded.bounds.Include(vertex.position);
            }
            ApplyMaterial(decoded, material);
            return AppendMesh(a_combined, decoded) ? ShapeResult::Accepted : ShapeResult::Malformed;
        }

        ShapeResult BuildSkinBones(RE::NiSkinInstance* a_skinInstance,
                                   const ObjectTransformMap& a_transforms,
                                   std::vector<BindPoseBone>& a_bones,
                                   std::size_t& a_reconstructedExternalBones)
        {
            if (a_skinInstance == nullptr || a_skinInstance->skinData == nullptr)
            {
                return ShapeResult::Unsupported;
            }
            const auto* skinData = a_skinInstance->skinData.get();
            const auto boneCount = static_cast<std::size_t>(skinData->bones);
            if (boneCount == 0)
            {
                return ShapeResult::Unsupported;
            }
            if (boneCount > MAX_SKIN_BONES || skinData->boneData == nullptr ||
                a_skinInstance->bones == nullptr)
            {
                return ShapeResult::Malformed;
            }

            a_bones.resize(boneCount);
            for (std::size_t boneIndex = 0; boneIndex < boneCount; ++boneIndex)
            {
                const auto skinToBone =
                    ToAffineTransform(skinData->boneData[boneIndex].skinToBone);
                a_bones[boneIndex].skinToBone = skinToBone;
                auto* boneObject = a_skinInstance->bones[boneIndex];
                const auto transform = a_transforms.find(boneObject);
                if (boneObject == nullptr || transform == a_transforms.end())
                {
                    if (!TryInvertAffine(skinToBone, a_bones[boneIndex].boneToModel))
                    {
                        return ShapeResult::Malformed;
                    }
                    ++a_reconstructedExternalBones;
                    continue;
                }
                a_bones[boneIndex].boneToModel = transform->second;
            }
            return ShapeResult::Accepted;
        }

        ShapeResult ExtractSkinnedShape(RE::BSTriShape* a_shape,
                                        const ObjectTransformMap& a_transforms,
                                        ID3D11Device* a_device,
                                        ID3D11DeviceContext* a_context,
                                        const ModelTextureOverride* a_textureOverride,
                                        const ActorMaterialTintOverride* a_materialTint,
                                        bool a_preferRuntimeTextures,
                                        std::size_t& a_matchedMaterialTintOverrides,
                                        std::size_t& a_reconstructedExternalBones,
                                        std::size_t& a_decodedDynamicShapes,
                                        PreviewMesh& a_combined)
        {
            const auto& geometryData = a_shape->GetGeometryRuntimeData();
            auto* skinInstance = geometryData.skinInstance.get();
            std::vector<BindPoseBone> skinBones;
            const auto boneResult = BuildSkinBones(
                skinInstance,
                a_transforms,
                skinBones,
                a_reconstructedExternalBones);
            if (boneResult != ShapeResult::Accepted)
            {
                return boneResult;
            }

            auto* skinPartition = skinInstance->skinPartition.get();
            if (skinPartition == nullptr && skinInstance->skinData != nullptr)
            {
                skinPartition = skinInstance->skinData->skinPartition.get();
            }
            if (skinPartition == nullptr || skinPartition->numPartitions == 0)
            {
                return ShapeResult::Unsupported;
            }
            const auto partitionCount = std::min<std::size_t>(
                skinPartition->numPartitions, skinPartition->partitions.size());
            if (partitionCount == 0 || partitionCount > MAX_SKIN_PARTITIONS ||
                partitionCount != skinPartition->numPartitions)
            {
                return ShapeResult::Malformed;
            }
            const auto sharedVertexCount = static_cast<std::size_t>(
                skinPartition->vertexCount);
            if (sharedVertexCount == 0 || sharedVertexCount > MAX_PREVIEW_VERTICES)
            {
                return ShapeResult::Malformed;
            }

            std::vector<std::uint8_t> dynamicPositionBytes;
            PackedPositionStream positionStream{};
            const bool isDynamic = a_shape->AsDynamicTriShape() != nullptr;
            if (isDynamic)
            {
                constexpr std::size_t DYNAMIC_POSITION_STRIDE = sizeof(float) * 4;
                if (sharedVertexCount >
                    std::numeric_limits<std::size_t>::max() / DYNAMIC_POSITION_STRIDE)
                {
                    return ShapeResult::Malformed;
                }
                const auto bytesRequired = sharedVertexCount * DYNAMIC_POSITION_STRIDE;
                auto& dynamicData =
                    a_shape->AsDynamicTriShape()->GetDynamicTrishapeRuntimeData();
                {
                    RE::BSSpinLockGuard locker(dynamicData.lock);
                    if (dynamicData.dynamicData == nullptr ||
                        dynamicData.dataSize < bytesRequired)
                    {
                        return ShapeResult::Malformed;
                    }
                    dynamicPositionBytes.resize(bytesRequired);
                    std::memcpy(
                        dynamicPositionBytes.data(),
                        dynamicData.dynamicData,
                        bytesRequired);
                }
                positionStream = {
                    .bytes = dynamicPositionBytes,
                    .stride = DYNAMIC_POSITION_STRIDE,
                };
            }

            PreviewMesh shapeMesh{};
            auto material = ExtractMaterial(a_shape, a_preferRuntimeTextures);
            if (a_textureOverride != nullptr)
            {
                ApplyTextureOverride(material, *a_textureOverride);
            }
            if (a_materialTint != nullptr &&
                ApplyActorMaterialTint(material, *a_materialTint))
            {
                ++a_matchedMaterialTintOverrides;
            }
            for (std::size_t partitionIndex = 0; partitionIndex < partitionCount; ++partitionIndex)
            {
                const auto& partition = skinPartition->partitions[partitionIndex];
                const auto paletteCount = static_cast<std::size_t>(partition.numBones);
                if (paletteCount == 0 || paletteCount > MAX_PARTITION_BONES ||
                    partition.bones == nullptr)
                {
                    return ShapeResult::Malformed;
                }

                for (std::size_t paletteIndex = 0; paletteIndex < paletteCount; ++paletteIndex)
                {
                    const auto skinBoneIndex = static_cast<std::size_t>(
                        partition.bones[paletteIndex]);
                    if (skinBoneIndex >= skinBones.size())
                    {
                        return ShapeResult::Malformed;
                    }
                }
                if (partition.vertices == 0 ||
                    static_cast<std::size_t>(partition.vertices) > sharedVertexCount)
                {
                    return ShapeResult::Malformed;
                }

                PreviewMesh decoded{};
                const auto decodeResult = DecodeRendererGeometry(
                    partition.buffData,
                    sharedVertexCount,
                    static_cast<std::size_t>(partition.triangles) * 3,
                    true,
                    a_device,
                    a_context,
                    decoded,
                    positionStream);
                if (decodeResult != ShapeResult::Accepted)
                {
                    return decodeResult;
                }
                if (ApplyBindPoseSkinning(decoded, skinBones) != SkinningError::None)
                {
                    return ShapeResult::Malformed;
                }
                ApplyMaterial(decoded, material);
                if (!AppendMesh(shapeMesh, decoded))
                {
                    return ShapeResult::Malformed;
                }
            }

            if (!AppendMesh(a_combined, shapeMesh))
            {
                return ShapeResult::Malformed;
            }
            if (isDynamic)
            {
                ++a_decodedDynamicShapes;
            }
            return ShapeResult::Accepted;
        }

        ShapeResult ExtractShape(RE::BSTriShape* a_shape,
                                 const AffineTransform& a_transform,
                                 const ObjectTransformMap& a_transforms,
                                 ID3D11Device* a_device,
                                 ID3D11DeviceContext* a_context,
                                 const ModelTextureOverride* a_textureOverride,
                                 const ActorMaterialTintOverride* a_materialTint,
                                 bool a_preferRuntimeTextures,
                                 std::size_t& a_matchedMaterialTintOverrides,
                                 std::size_t& a_reconstructedExternalBones,
                                 std::size_t& a_decodedDynamicShapes,
                                 PreviewMesh& a_combined)
        {
            if (a_shape == nullptr)
            {
                return ShapeResult::Unsupported;
            }
            return a_shape->GetGeometryRuntimeData().skinInstance != nullptr
                       ? ExtractSkinnedShape(
                             a_shape,
                             a_transforms,
                             a_device,
                             a_context,
                             a_textureOverride,
                             a_materialTint,
                             a_preferRuntimeTextures,
                             a_matchedMaterialTintOverrides,
                             a_reconstructedExternalBones,
                             a_decodedDynamicShapes,
                             a_combined)
                       : ExtractRigidShape(
                             a_shape,
                             a_transform,
                             a_device,
                             a_context,
                             a_textureOverride,
                             a_materialTint,
                             a_preferRuntimeTextures,
                             a_matchedMaterialTintOverrides,
                             a_combined);
        }

        void Traverse(RE::NiAVObject* a_object,
                      const ObjectTransformMap& a_transforms,
                      ID3D11Device* a_device,
                       ID3D11DeviceContext* a_context,
                       std::span<const ModelTextureOverride> a_textureOverrides,
                       const ActorMaterialTintOverride* a_materialTint,
                       bool a_skipCulled,
                       bool a_preferRuntimeTextures,
                       ExtractionResult& a_result,
                      bool& a_sawMalformed,
                      bool& a_sawReadbackFailure,
                      std::uint32_t& a_shapeIndex,
                      std::unordered_set<const RE::NiAVObject*>& a_visited)
        {
            if (a_object == nullptr || !a_visited.insert(a_object).second)
            {
                return;
            }
            if (a_skipCulled && a_object->GetAppCulled())
            {
                if (a_object->AsTriShape() != nullptr)
                {
                    ++a_result.skippedShapes;
                }
                return;
            }

            const auto transform = a_transforms.find(a_object);
            if (transform == a_transforms.end())
            {
                a_sawMalformed = true;
                return;
            }
            if (auto* shape = a_object->AsTriShape(); shape != nullptr)
            {
                const auto currentShapeIndex = a_shapeIndex++;
                const auto* rawShapeName = shape->name.c_str();
                const std::string_view shapeName = rawShapeName != nullptr ?
                    std::string_view(rawShapeName) : std::string_view{};
                const auto* textureOverride = FindTextureOverride(
                    a_textureOverrides, shapeName, currentShapeIndex);
                if (textureOverride != nullptr)
                {
                    ++a_result.matchedTextureOverrides;
                }
                switch (ExtractShape(
                    shape,
                    transform->second,
                    a_transforms,
                    a_device,
                    a_context,
                    textureOverride,
                    a_materialTint,
                    a_preferRuntimeTextures,
                    a_result.matchedMaterialTintOverrides,
                    a_result.reconstructedExternalBones,
                    a_result.decodedDynamicShapes,
                    a_result.mesh))
                {
                case ShapeResult::Accepted:
                    ++a_result.acceptedShapes;
                    break;
                case ShapeResult::Unsupported:
                    ++a_result.skippedShapes;
                    break;
                case ShapeResult::Malformed:
                    ++a_result.skippedShapes;
                    a_sawMalformed = true;
                    if (a_result.firstFailedShapeName.empty())
                    {
                        const auto* name = shape->name.c_str();
                        a_result.firstFailedShapeName =
                            name != nullptr && name[0] != '\0' ? name : "<unnamed>";
                    }
                    break;
                case ShapeResult::ReadbackFailed:
                    ++a_result.skippedShapes;
                    a_sawReadbackFailure = true;
                    if (a_result.firstFailedShapeName.empty())
                    {
                        const auto* name = shape->name.c_str();
                        a_result.firstFailedShapeName =
                            name != nullptr && name[0] != '\0' ? name : "<unnamed>";
                    }
                    break;
                }
            }

            if (auto* node = a_object->AsNode(); node != nullptr)
            {
                for (const auto& child : node->GetChildren())
                {
                    if (child != nullptr)
                    {
                        Traverse(
                            child.get(),
                            a_transforms,
                            a_device,
                            a_context,
                            a_textureOverrides,
                            a_materialTint,
                            a_skipCulled,
                            a_preferRuntimeTextures,
                            a_result,
                            a_sawMalformed,
                            a_sawReadbackFailure,
                            a_shapeIndex,
                            a_visited);
                    }
                }
            }
        }
    }

    ExtractionResult ExtractModelMesh(RE::NiNode* a_root,
                                      ID3D11Device* a_device,
                                      ID3D11DeviceContext* a_context,
                                      std::span<const ModelTextureOverride> a_textureOverrides,
                                      const ActorMaterialTintOverride* a_materialTint)
    {
        ExtractionResult result{};
        if (a_root == nullptr || a_device == nullptr || a_context == nullptr)
        {
            result.status = ExtractionStatus::MalformedGeometry;
            return result;
        }

        ObjectTransformMap transforms;
        transforms.reserve(1'024);
        if (!CollectObjectTransforms(a_root, AffineTransform{}, transforms, 0))
        {
            result.status = ExtractionStatus::MalformedGeometry;
            return result;
        }

        bool sawMalformed = false;
        bool sawReadbackFailure = false;
        std::uint32_t shapeIndex = 0;
        std::unordered_set<const RE::NiAVObject*> visited;
        visited.reserve(transforms.size());
        Traverse(
            a_root,
            transforms,
            a_device,
            a_context,
            a_textureOverrides,
            a_materialTint,
            false,
            false,
            result,
            sawMalformed,
            sawReadbackFailure,
            shapeIndex,
            visited);
        if (!result.mesh.Empty())
        {
            result.status = ExtractionStatus::Success;
        }
        else if (sawReadbackFailure)
        {
            result.status = ExtractionStatus::GpuReadbackFailed;
        }
        else if (sawMalformed)
        {
            result.status = ExtractionStatus::MalformedGeometry;
        }
        else
        {
            result.status = ExtractionStatus::NoSupportedGeometry;
        }
        return result;
    }

    ExtractionResult ExtractLiveActorMesh(
        RE::NiNode* a_actorRoot,
        std::span<RE::NiAVObject* const> a_selectedRoots,
        ID3D11Device* a_device,
        ID3D11DeviceContext* a_context)
    {
        ExtractionResult result{};
        if (a_actorRoot == nullptr || a_selectedRoots.empty() ||
            a_device == nullptr || a_context == nullptr)
        {
            result.status = ExtractionStatus::MalformedGeometry;
            return result;
        }

        ObjectTransformMap transforms;
        transforms.reserve(4'096);
        const auto actorWorld = ToAffineTransform(a_actorRoot->world);
        if (!CollectLiveObjectTransforms(a_actorRoot, actorWorld, transforms, 0))
        {
            result.status = ExtractionStatus::MalformedGeometry;
            return result;
        }

        bool sawMalformed = false;
        bool sawReadbackFailure = false;
        std::uint32_t shapeIndex = 0;
        std::unordered_set<const RE::NiAVObject*> visited;
        visited.reserve(transforms.size());
        for (auto* root : a_selectedRoots)
        {
            if (root == nullptr || !transforms.contains(root))
            {
                sawMalformed = true;
                continue;
            }
            Traverse(
                root,
                transforms,
                a_device,
                a_context,
                {},
                nullptr,
                true,
                true,
                result,
                sawMalformed,
                sawReadbackFailure,
                shapeIndex,
                visited);
        }

        if (!result.mesh.Empty())
        {
            result.status = ExtractionStatus::Success;
        }
        else if (sawReadbackFailure)
        {
            result.status = ExtractionStatus::GpuReadbackFailed;
        }
        else if (sawMalformed)
        {
            result.status = ExtractionStatus::MalformedGeometry;
        }
        else
        {
            result.status = ExtractionStatus::NoSupportedGeometry;
        }
        return result;
    }
}
