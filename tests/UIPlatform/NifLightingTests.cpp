#include "Render/NifLighting.h"

#include <cmath>
#include <iostream>
#include <numbers>

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

    bool Near(float a_left, float a_right, float a_tolerance = 0.0001f)
    {
        return std::abs(a_left - a_right) <= a_tolerance;
    }

    float Length(const Meridian::Render::NifPreview::Float3& a_value)
    {
        return std::sqrt(a_value.x * a_value.x + a_value.y * a_value.y +
                         a_value.z * a_value.z);
    }
}

int main()
{
    using namespace Meridian::Render::NifPreview;
    using Meridian::UI::NifView::LightingPreset;

    const auto front = BuildCameraBasis(0.0f, 0.0f);
    const auto neutral = BuildLightingRig(LightingPreset::Neutral, 0.0f, front);
    const auto bright = BuildLightingRig(LightingPreset::Bright, 0.0f, front);
    const auto dramatic = BuildLightingRig(LightingPreset::Dramatic, 0.0f, front);

    Expect(neutral.ambientTop.x > neutral.ambientBottom.x,
           "neutral rig has directional hemispheric ambient light");
    Expect(neutral.lightColor[0].x > neutral.lightColor[1].x &&
               neutral.lightColor[1].x > neutral.lightColor[2].x,
           "neutral rig orders key, fill, and rim energy deliberately");
    Expect(bright.ambientBottom.x > neutral.ambientBottom.x &&
               bright.exposureMultiplier > neutral.exposureMultiplier,
           "bright rig reveals more shadow detail than neutral");
    Expect(dramatic.ambientBottom.x < neutral.ambientBottom.x &&
               dramatic.lightColor[2].z > neutral.lightColor[2].z,
           "dramatic rig deepens shadows and strengthens separation");

    for (const auto& direction : neutral.lightDirection)
    {
        Expect(Near(Length(direction), 1.0f), "studio light directions are normalized");
    }

    const auto quarterTurn = BuildCameraBasis(std::numbers::pi_v<float> * 0.5f, 0.0f);
    const auto rotated = BuildLightingRig(LightingPreset::Neutral, 0.0f, quarterTurn);
    Expect(!Near(neutral.lightDirection[0].x, rotated.lightDirection[0].x) &&
               !Near(neutral.lightDirection[0].y, rotated.lightDirection[0].y),
           "studio rig rotates with the inspection camera");

    const auto maximum = BuildLightingRig(LightingPreset::Neutral, 99.0f, front);
    const auto clampedMaximum = BuildLightingRig(LightingPreset::Neutral, 2.0f, front);
    const auto minimum = BuildLightingRig(LightingPreset::Neutral, -99.0f, front);
    const auto clampedMinimum = BuildLightingRig(LightingPreset::Neutral, -2.0f, front);
    Expect(Near(maximum.exposureMultiplier, clampedMaximum.exposureMultiplier),
           "positive exposure is bounded");
    Expect(Near(minimum.exposureMultiplier, clampedMinimum.exposureMultiplier),
           "negative exposure is bounded");

    const auto fallback = BuildLightingRig(static_cast<LightingPreset>(99), 0.0f, front);
    Expect(Near(fallback.ambientTop.x, neutral.ambientTop.x),
           "unknown presets fall back to neutral");

    if (g_failures != 0)
    {
        std::cerr << g_failures << " NIF lighting test(s) failed\n";
        return 1;
    }
    std::cout << "All NIF lighting tests passed\n";
    return 0;
}
