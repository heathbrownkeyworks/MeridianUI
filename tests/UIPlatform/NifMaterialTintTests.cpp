#include "Render/NifMaterialTint.h"

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

    bool Near(float a_left, float a_right)
    {
        return std::abs(a_left - a_right) <= 1.0e-6f;
    }
}

int main()
{
    using namespace Meridian::Render::NifPreview;

    const auto actorTint = MakeActorMaterialTintOverride(
        64, 128, 255,
        32, 64, 128,
        true);
    Expect(actorTint.hasBodyTint && actorTint.hasHairTint,
           "actor tint conversion retains both available channels");
    Expect(Near(actorTint.bodyTint.x, 64.0f / 255.0f) &&
               Near(actorTint.bodyTint.y, 128.0f / 255.0f) &&
               Near(actorTint.bodyTint.z, 1.0f),
           "body tint follows Skyrim's 255-based actor conversion");
    Expect(Near(actorTint.hairTint.x, 32.0f / 128.0f) &&
               Near(actorTint.hairTint.y, 64.0f / 128.0f) &&
               Near(actorTint.hairTint.z, 1.0f),
           "hair tint follows Skyrim's 128-based actor conversion");

    PreviewMaterial body{};
    body.tintKind = MaterialTintKind::Body;
    Expect(ApplyActorMaterialTint(body, actorTint) &&
               Near(body.tintColor.x, actorTint.bodyTint.x) &&
               Near(body.tintColor.y, actorTint.bodyTint.y) &&
               Near(body.tintColor.z, actorTint.bodyTint.z),
           "FaceGen RGB-tint materials receive the actor body tint");

    PreviewMaterial hair{};
    hair.tintKind = MaterialTintKind::Hair;
    Expect(ApplyActorMaterialTint(hair, actorTint) &&
               Near(hair.tintColor.x, actorTint.hairTint.x) &&
               Near(hair.tintColor.y, actorTint.hairTint.y) &&
               Near(hair.tintColor.z, actorTint.hairTint.z),
           "HairTint materials receive the actor hair color");

    PreviewMaterial ordinary{};
    const auto ordinaryColor = ordinary.tintColor;
    Expect(!ApplyActorMaterialTint(ordinary, actorTint) &&
               Near(ordinary.tintColor.x, ordinaryColor.x) &&
               Near(ordinary.tintColor.y, ordinaryColor.y) &&
               Near(ordinary.tintColor.z, ordinaryColor.z),
           "ordinary lighting materials are not actor-tinted");

    auto missingHair = actorTint;
    missingHair.hasHairTint = false;
    PreviewMaterial missingHairMaterial{};
    missingHairMaterial.tintKind = MaterialTintKind::Hair;
    Expect(!ApplyActorMaterialTint(missingHairMaterial, missingHair),
           "missing actor hair color preserves the NIF material tint");

    auto invalid = actorTint;
    invalid.bodyTint.x = std::numeric_limits<float>::quiet_NaN();
    PreviewMaterial invalidBody{};
    invalidBody.tintKind = MaterialTintKind::Body;
    Expect(!ApplyActorMaterialTint(invalidBody, invalid),
           "non-finite actor colors are rejected");

    if (g_failures != 0)
    {
        std::cerr << g_failures << " NIF material tint test(s) failed\n";
        return 1;
    }
    std::cout << "All NIF material tint tests passed\n";
    return 0;
}
