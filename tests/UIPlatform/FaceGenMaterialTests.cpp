#include "Render/FaceGenMaterial.h"

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
        return std::abs(a_left - a_right) <= 1.0e-5f;
    }
}

int main()
{
    using namespace Meridian::Render::NifPreview;

    const auto detail = FaceGenRgbDetailColor();
    Expect(Near(detail.x, 1.01172f) &&
               Near(detail.y, 0.996094f) &&
               Near(detail.z, 1.01172f),
           "FaceGen RGB tint uses Skyrim's fixed detail-color constants");

    const Float3 diffuse{0.5f, 0.25f, 0.75f};
    const Float3 tint{0.25f, 0.75f, 0.5f};
    const Float3 unityDetail{1.0f, 1.0f, 1.0f};
    const auto overlaid = ApplyFaceGenOverlay(diffuse, tint, unityDetail);
    Expect(Near(overlaid.x, 0.375f) &&
               Near(overlaid.y, 0.34375f) &&
               Near(overlaid.z, 0.75f),
           "FaceGen overlay follows Skyrim's diffuse/tint blend");

    PreviewMaterial generic{};
    Expect(generic.feature == PreviewMaterialFeature::Generic &&
               !generic.modelSpaceNormal && !generic.tangentSpaceNormal,
           "ordinary materials default to the generic no-normal-map path");

    PreviewMaterial faceGen{};
    faceGen.feature = PreviewMaterialFeature::FaceGen;
    faceGen.faceTintTexturePath = "textures\\actors\\character\\facetint.dds";
    faceGen.faceDetailTexturePath = "textures\\actors\\character\\facedetail.dds";
    faceGen.modelSpaceNormal = true;
    Expect(faceGen.feature == PreviewMaterialFeature::FaceGen &&
               faceGen.modelSpaceNormal && !faceGen.tangentSpaceNormal &&
               !faceGen.faceTintTexturePath.empty() &&
               !faceGen.faceDetailTexturePath.empty(),
           "texture-driven FaceGen materials retain tint, detail, and model-space normal state");

    PreviewMaterial rgbTint{};
    rgbTint.feature = PreviewMaterialFeature::FaceGenRgbTint;
    Expect(rgbTint.feature == PreviewMaterialFeature::FaceGenRgbTint,
           "FaceGen RGB tint remains distinct from texture-driven FaceGen");

    if (g_failures != 0)
    {
        std::cerr << g_failures << " FaceGen material test(s) failed\n";
        return 1;
    }
    std::cout << "All FaceGen material tests passed\n";
    return 0;
}
