#include "MeridianUIAPI/NifViewAPI.h"

#include <iostream>
#include <type_traits>

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
    using namespace Meridian::UI::NifView;

    static_assert(std::is_standard_layout_v<NifLoadInfo>);
    static_assert(std::is_standard_layout_v<CameraState>);
    static_assert(std::is_same_v<std::underlying_type_t<Status>, std::uint32_t>);
    static_assert(std::is_same_v<std::underlying_type_t<LightingPreset>, std::uint32_t>);

    NifLoadInfo load{};
    Expect(load.structSize == sizeof(NifLoadInfo), "load info advertises its compiled size");
    Expect(load.surface == Meridian::UI::RenderLayer::INVALID_SURFACE_HANDLE,
           "load info defaults to an invalid render surface");
    Expect(load.frameOnLoad, "new models frame automatically by default");
    Expect(NIF_LOAD_INFO_MIN_SIZE_1 == sizeof(NifLoadInfo), "version 1 load info size is frozen");

    CameraState camera{};
    Expect(camera.structSize == sizeof(CameraState), "camera state advertises its compiled size");
    Expect(camera.yawDegrees == 35.0f, "camera has a useful default yaw");
    Expect(camera.pitchDegrees == 15.0f, "camera has a useful default pitch");
    Expect(camera.distanceScale == 1.0f, "camera defaults to fitted distance");
    Expect(camera.panX == 0.0f && camera.panY == 0.0f && camera.panZ == 0.0f,
           "camera pan defaults to the framed model center");
    Expect(CAMERA_STATE_MIN_SIZE_1 == 32, "original version 1 camera boundary is frozen");
    Expect(CAMERA_STATE_LIGHTING_SIZE_1 == sizeof(CameraState),
           "lighting-aware camera state advertises its complete size");
    Expect(camera.lightingPreset == LightingPreset::Neutral,
           "camera defaults to neutral studio lighting");
    Expect(camera.exposureStops == 0.0f, "camera defaults to neutral exposure adjustment");
    Expect(static_cast<std::uint32_t>(LightingPreset::Neutral) == 0,
           "neutral lighting preset ABI value is stable");
    Expect(static_cast<std::uint32_t>(LightingPreset::Bright) == 1,
           "bright lighting preset ABI value is stable");
    Expect(static_cast<std::uint32_t>(LightingPreset::Dramatic) == 2,
           "dramatic lighting preset ABI value is stable");
    Expect(IsValidLightingPreset(LightingPreset::Neutral) &&
               IsValidLightingPreset(LightingPreset::Bright) &&
               IsValidLightingPreset(LightingPreset::Dramatic),
           "all published lighting presets are accepted");
    Expect(!IsValidLightingPreset(static_cast<LightingPreset>(3)),
           "unknown lighting presets are rejected");

    Expect(static_cast<std::uint32_t>(Status::Empty) == 0, "Empty status ABI value is stable");
    Expect(static_cast<std::uint32_t>(Status::Ready) == 2, "Ready status ABI value is stable");
    Expect(static_cast<std::uint32_t>(Status::Unsupported) == 4,
           "Unsupported status ABI value is stable");
    Expect(static_cast<std::uint32_t>(Status::ShuttingDown) == 6,
           "ShuttingDown status ABI value is stable");

    Expect(IsSupported("Meridian.NifView", 1), "exact Meridian.NifView/1 query is supported");
    Expect(!IsSupported("Meridian.NifView", 2), "future interface versions are rejected");
    Expect(!IsSupported("meridian.nifview", 1), "extension name matching is exact");
    Expect(!IsSupported(nullptr, 1), "null extension names are rejected");

    return g_failures == 0 ? 0 : 1;
}
