#include "Render/BindPoseSkinning.h"

#include <cmath>
#include <iostream>

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

    bool Near(float a_left, float a_right)
    {
        return std::abs(a_left - a_right) < 0.001f;
    }

    bool NearIdentity(const Meridian::Render::NifPreview::AffineTransform& a_transform)
    {
        using namespace Meridian::Render::NifPreview;
        const AffineTransform identity{};
        for (std::size_t row = 0; row < 3; ++row)
        {
            for (std::size_t column = 0; column < 3; ++column)
            {
                if (!Near(a_transform.rotation[row][column],
                          identity.rotation[row][column]))
                {
                    return false;
                }
            }
        }
        return Near(a_transform.translation.x, 0.0f) &&
               Near(a_transform.translation.y, 0.0f) &&
               Near(a_transform.translation.z, 0.0f) &&
               Near(a_transform.scale, 1.0f);
    }

    Meridian::Render::NifPreview::PreviewMesh MakeTriangle()
    {
        using namespace Meridian::Render::NifPreview;
        PreviewMesh mesh{};
        mesh.vertices.resize(3);
        mesh.vertices[0].position = {1.0f, 0.0f, 0.0f};
        mesh.vertices[1].position = {0.0f, 1.0f, 0.0f};
        mesh.vertices[2].position = {0.0f, 0.0f, 1.0f};
        for (auto& vertex : mesh.vertices)
        {
            vertex.boneWeights[0] = 1.0f;
        }
        mesh.indices = {0, 1, 2};
        mesh.draws.push_back({0, 3, {}});
        mesh.bounds.Include(mesh.vertices[0].position);
        mesh.bounds.Include(mesh.vertices[1].position);
        mesh.bounds.Include(mesh.vertices[2].position);
        return mesh;
    }
}

int main()
{
    using namespace Meridian::Render::NifPreview;

    const BindPoseBone identity{};
    auto identityMesh = MakeTriangle();
    Expect(ApplyBindPoseSkinning(identityMesh, std::span(&identity, 1)) == SkinningError::None,
           "one-bone identity skinning succeeds");
    Expect(Near(identityMesh.vertices[0].position.x, 1.0f),
           "identity skinning preserves positions");

    BindPoseBone translated{};
    translated.boneToModel.translation = {10.0f, -2.0f, 3.0f};
    auto translatedMesh = MakeTriangle();
    Expect(ApplyBindPoseSkinning(translatedMesh, std::span(&translated, 1)) == SkinningError::None,
           "translated bone skinning succeeds");
    Expect(Near(translatedMesh.vertices[0].position.x, 11.0f) &&
               Near(translatedMesh.vertices[0].position.y, -2.0f) &&
               Near(translatedMesh.vertices[0].position.z, 3.0f),
           "bone-to-model translation deforms vertices");

    BindPoseBone composed{};
    composed.boneToModel.translation = {10.0f, 0.0f, 0.0f};
    composed.skinToBone.translation = {-10.0f, 0.0f, 0.0f};
    auto composedMesh = MakeTriangle();
    Expect(ApplyBindPoseSkinning(composedMesh, std::span(&composed, 1)) == SkinningError::None,
           "skin-to-bone composition succeeds");
    Expect(Near(composedMesh.vertices[0].position.x, 1.0f),
           "inverse bind translation cancels bind bone translation");

    AffineTransform sourceTransform{};
    sourceTransform.rotation[0][0] = 0.0f;
    sourceTransform.rotation[0][1] = -1.0f;
    sourceTransform.rotation[1][0] = 1.0f;
    sourceTransform.rotation[1][1] = 0.0f;
    sourceTransform.translation = {4.0f, -6.0f, 2.0f};
    sourceTransform.scale = 2.0f;
    AffineTransform inverseTransform{};
    Expect(TryInvertAffine(sourceTransform, inverseTransform),
           "finite rotation, translation, and uniform scale can be inverted");
    Expect(NearIdentity(SkinningDetail::Compose(sourceTransform, inverseTransform)) &&
               NearIdentity(SkinningDetail::Compose(inverseTransform, sourceTransform)),
           "affine inverse composes to identity in both orders");

    AffineTransform relativeRoot{};
    Expect(TryMakeRelativeTransform(sourceTransform, sourceTransform, relativeRoot) &&
               NearIdentity(relativeRoot),
           "a live actor root is identity in its own preview space");

    AffineTransform childLocal{};
    childLocal.translation = {3.0f, -4.0f, 5.0f};
    childLocal.scale = 0.5f;
    childLocal.rotation[0][0] = 0.0f;
    childLocal.rotation[0][1] = 1.0f;
    childLocal.rotation[1][0] = -1.0f;
    childLocal.rotation[1][1] = 0.0f;
    const auto childWorld = SkinningDetail::Compose(sourceTransform, childLocal);
    AffineTransform relativeChild{};
    Expect(TryMakeRelativeTransform(sourceTransform, childWorld, relativeChild),
           "a finite live child transform can be normalized to actor space");
    Expect(Near(relativeChild.translation.x, childLocal.translation.x) &&
               Near(relativeChild.translation.y, childLocal.translation.y) &&
               Near(relativeChild.translation.z, childLocal.translation.z) &&
               Near(relativeChild.scale, childLocal.scale),
           "actor-relative normalization preserves the child local transform");

    auto singularTransform = sourceTransform;
    singularTransform.scale = 0.0f;
    Expect(!TryInvertAffine(singularTransform, inverseTransform),
           "singular affine transforms are rejected");

    auto singularRotation = sourceTransform;
    singularRotation.rotation[2][2] = 0.0f;
    Expect(!TryInvertAffine(singularRotation, inverseTransform),
           "singular rotation matrices are rejected");
    Expect(!TryMakeRelativeTransform(singularTransform, childWorld, relativeChild),
           "live actor roots with singular transforms are rejected");

    BindPoseBone blendBones[2]{};
    blendBones[0].boneToModel.translation = {2.0f, 0.0f, 0.0f};
    blendBones[1].boneToModel.translation = {6.0f, 0.0f, 0.0f};
    auto blendMesh = MakeTriangle();
    for (auto& vertex : blendMesh.vertices)
    {
        vertex.boneWeights = {0.25f, 0.75f, 0.0f, 0.0f};
        vertex.boneIndices = {0, 1, 0, 0};
    }
    Expect(ApplyBindPoseSkinning(blendMesh, blendBones) == SkinningError::None,
           "two-bone blending succeeds");
    Expect(Near(blendMesh.vertices[0].position.x, 6.0f),
           "two-bone positions are weight blended");
    Expect(Near(blendMesh.bounds.minimum.x, 5.0f) && Near(blendMesh.bounds.maximum.x, 6.0f),
           "deformed bounds are regenerated");

    auto invalidIndex = MakeTriangle();
    invalidIndex.vertices[0].boneIndices[0] = 1;
    Expect(ApplyBindPoseSkinning(invalidIndex, std::span(&identity, 1)) ==
               SkinningError::InvalidBoneIndex,
           "out-of-range bone indices are rejected");

    auto missingWeights = MakeTriangle();
    missingWeights.vertices[1].boneWeights = {};
    Expect(ApplyBindPoseSkinning(missingWeights, std::span(&identity, 1)) ==
               SkinningError::MissingWeights,
           "vertices without influences are rejected");

    auto invalidBone = identity;
    invalidBone.boneToModel.scale = 0.0f;
    auto invalidTransform = MakeTriangle();
    Expect(ApplyBindPoseSkinning(invalidTransform, std::span(&invalidBone, 1)) ==
               SkinningError::InvalidTransform,
           "singular bone transforms are rejected");

    return g_failures == 0 ? 0 : 1;
}
