#include "Render/NifCameraMath.h"

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

    bool Near(float a_left, float a_right)
    {
        return std::abs(a_left - a_right) < 0.0001f;
    }

    float Dot(const Meridian::Render::NifPreview::Float3& a_left,
              const Meridian::Render::NifPreview::Float3& a_right)
    {
        return a_left.x * a_right.x + a_left.y * a_right.y + a_left.z * a_right.z;
    }
}

int main()
{
    using namespace Meridian::Render::NifPreview;

    const auto front = BuildCameraBasis(0.0f, 0.0f);
    Expect(Near(front.eyeDirection.x, 1.0f) && Near(front.eyeDirection.y, 0.0f),
           "zero yaw places the eye on positive X");
    Expect(Near(front.right.x, 0.0f) && Near(front.right.y, 1.0f),
           "screen right is positive Y at zero yaw");
    Expect(Near(front.up.z, 1.0f), "screen up is positive Z at zero pitch");

    const auto quarterTurn = BuildCameraBasis(std::numbers::pi_v<float> * 0.5f, 0.0f);
    Expect(Near(quarterTurn.eyeDirection.x, 0.0f) && Near(quarterTurn.eyeDirection.y, 1.0f),
           "quarter-turn yaw places the eye on positive Y");
    Expect(Near(quarterTurn.right.x, -1.0f) && Near(quarterTurn.right.y, 0.0f),
           "screen right rotates with the camera");

    const auto pitched = BuildCameraBasis(0.4f, 0.3f);
    Expect(Near(Dot(pitched.eyeDirection, pitched.right), 0.0f),
           "eye and right vectors remain orthogonal");
    Expect(Near(Dot(pitched.eyeDirection, pitched.up), 0.0f),
           "eye and up vectors remain orthogonal");
    Expect(Near(Dot(pitched.right, pitched.up), 0.0f),
           "right and up vectors remain orthogonal");

    const Float3 center{10.0f, 20.0f, 30.0f};
    const auto panned = ApplyNormalizedPan(center, 8.0f, front, 0.5f, -0.25f, 0.125f);
    Expect(Near(panned.x, 11.0f), "depth pan scales by model radius");
    Expect(Near(panned.y, 24.0f), "horizontal pan scales by model radius");
    Expect(Near(panned.z, 28.0f), "vertical pan scales by model radius");

    if (g_failures != 0)
    {
        std::cerr << g_failures << " NIF camera math test(s) failed\n";
        return 1;
    }

    std::cout << "All NIF camera math tests passed\n";
    return 0;
}
