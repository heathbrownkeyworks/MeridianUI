#include "ControlPayload.h"

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
}

int main()
{
    using Meridian::NifTest::ParseCameraPayload;
    using Meridian::NifTest::ParseLayoutPayload;
    using Meridian::NifTest::ParseLightingPayload;
    using Meridian::NifTest::ParseModelPathPayload;
    using Meridian::NifTest::ParseObjectVisibilityPayload;
    using Meridian::NifTest::ParseWeightPayload;

    const auto camera = ParseCameraPayload("45.5,-12.25,1.5,0.1,-0.2,0.3");
    Expect(camera.has_value(), "valid camera payload parses");
    if (camera)
    {
        Expect(std::abs(camera->yawDegrees - 45.5f) < 0.001f, "yaw is preserved");
        Expect(std::abs(camera->pitchDegrees + 12.25f) < 0.001f, "pitch is preserved");
        Expect(std::abs(camera->distanceScale - 1.5f) < 0.001f, "zoom is preserved");
        Expect(std::abs(camera->panX - 0.1f) < 0.001f, "pan X is preserved");
        Expect(std::abs(camera->panY + 0.2f) < 0.001f, "pan Y is preserved");
        Expect(std::abs(camera->panZ - 0.3f) < 0.001f, "pan Z is preserved");
    }

    Expect(!ParseCameraPayload(nullptr), "null camera payload is rejected");
    Expect(!ParseCameraPayload(""), "empty camera payload is rejected");
    Expect(!ParseCameraPayload("1,2,3,4,5"), "missing camera field is rejected");
    Expect(!ParseCameraPayload("1,2,3,4,5,6,7"), "extra camera field is rejected");
    Expect(!ParseCameraPayload("1,2,nan,4,5,6"), "non-finite camera value is rejected");
    Expect(!ParseCameraPayload("1,90,1,0,0,0"), "out-of-range pitch is rejected");
    Expect(!ParseCameraPayload("1,0,0.01,0,0,0"), "out-of-range zoom is rejected");
    Expect(!ParseCameraPayload("1,0,1,11,0,0"), "out-of-range normalized pan is rejected");
    Expect(!ParseCameraPayload("1,0,1,0,0,0 trailing"), "trailing camera data is rejected");

    const auto layout = ParseLayoutPayload("220,140,800,600");
    Expect(layout.has_value(), "valid layout payload parses");
    if (layout)
    {
        Expect(layout->x == 220 && layout->y == 140, "layout origin is preserved");
        Expect(layout->width == 800 && layout->height == 600, "layout size is preserved");
    }

    Expect(!ParseLayoutPayload(nullptr), "null layout payload is rejected");
    Expect(!ParseLayoutPayload("0,0,0,480"), "zero layout width is rejected");
    Expect(!ParseLayoutPayload("0,0,640,-1"), "negative layout height is rejected");
    Expect(!ParseLayoutPayload("0,0,9000,480"), "oversized layout is rejected");
    Expect(!ParseLayoutPayload("0,0,640,480,1"), "extra layout field is rejected");

    const auto lighting = ParseLightingPayload("1,0.7");
    Expect(lighting.has_value(), "valid lighting payload parses");
    if (lighting)
    {
        Expect(lighting->preset == Meridian::UI::NifView::LightingPreset::Bright,
               "lighting preset is preserved");
        Expect(std::abs(lighting->exposureStops - 0.7f) < 0.001f,
               "lighting exposure is preserved");
    }
    Expect(!ParseLightingPayload(nullptr), "null lighting payload is rejected");
    Expect(!ParseLightingPayload(""), "empty lighting payload is rejected");
    Expect(!ParseLightingPayload("3,0"), "unknown lighting preset is rejected");
    Expect(!ParseLightingPayload("-1,0"), "negative lighting preset is rejected");
    Expect(!ParseLightingPayload("1.5,0"), "fractional lighting preset is rejected");
    Expect(!ParseLightingPayload("1,nan"), "non-finite lighting exposure is rejected");
    Expect(!ParseLightingPayload("1,2.1"), "out-of-range lighting exposure is rejected");
    Expect(!ParseLightingPayload("1,0,0"), "extra lighting field is rejected");

    const auto visibility = ParseObjectVisibilityPayload("103,0");
    Expect(visibility && visibility->object == 103 && !visibility->visible,
           "valid object visibility payload parses");
    Expect(!ParseObjectVisibilityPayload(nullptr), "null object visibility is rejected");
    Expect(!ParseObjectVisibilityPayload("0,1"), "zero object handle is rejected");
    Expect(!ParseObjectVisibilityPayload("101,2"), "non-boolean visibility is rejected");
    Expect(!ParseObjectVisibilityPayload("101,-1"), "negative visibility is rejected");
    Expect(!ParseObjectVisibilityPayload("101,1,0"), "extra visibility field is rejected");

    const auto weight = ParseWeightPayload("50");
    Expect(weight && std::abs(*weight - 50.0f) < 0.001f,
           "valid Skyrim weight parses");
    Expect(ParseWeightPayload("0").has_value(), "zero Skyrim weight is accepted");
    Expect(ParseWeightPayload("100").has_value(), "full Skyrim weight is accepted");
    Expect(ParseWeightPayload("37.5").has_value(), "fractional Skyrim weight is accepted");
    Expect(!ParseWeightPayload(nullptr), "null Skyrim weight is rejected");
    Expect(!ParseWeightPayload(""), "empty Skyrim weight is rejected");
    Expect(!ParseWeightPayload("nan"), "non-finite Skyrim weight is rejected");
    Expect(!ParseWeightPayload("-1"), "negative Skyrim weight is rejected");
    Expect(!ParseWeightPayload("101"), "Skyrim weight above 100 is rejected");
    Expect(!ParseWeightPayload("50,100"), "extra Skyrim weight data is rejected");

    const auto modelPath = ParseModelPathPayload("meshes/armor/hide/f/cuirasslight_1.NIF");
    Expect(modelPath && *modelPath == "armor\\hide\\f\\cuirasslight_1.NIF",
           "model paths normalize slash direction and an optional meshes prefix");
    Expect(ParseModelPathPayload("clutter\\coin01.nif").has_value(),
           "static preset paths remain accepted");
    Expect(!ParseModelPathPayload(nullptr), "null model paths are rejected");
    Expect(!ParseModelPathPayload(""), "empty model paths are rejected");
    Expect(!ParseModelPathPayload("C:\\meshes\\armor.nif"),
           "absolute model paths are rejected");
    Expect(!ParseModelPathPayload("armor\\..\\secret.nif"),
           "model path traversal is rejected");
    Expect(!ParseModelPathPayload("armor\\hide\\cuirass.dds"),
           "non-NIF model extensions are rejected");

    if (g_failures != 0)
    {
        std::cerr << g_failures << " NIF test control payload test(s) failed\n";
        return 1;
    }

    std::cout << "All NIF test control payload tests passed\n";
    return 0;
}
