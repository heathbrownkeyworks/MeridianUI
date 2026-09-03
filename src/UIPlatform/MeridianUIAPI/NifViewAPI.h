#pragma once

#include "RenderLayerAPI.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace Meridian::UI::NifView
{
    inline constexpr char EXTENSION_NAME[] = "Meridian.NifView";
    inline constexpr std::uint32_t INTERFACE_VERSION = 1;

    enum class Status : std::uint32_t
    {
        Empty = 0,
        Loading = 1,
        Ready = 2,
        Failed = 3,
        Unsupported = 4,
        InvalidSurface = 5,
        ShuttingDown = 6,
    };

    enum class LightingPreset : std::uint32_t
    {
        Neutral = 0,
        Bright = 1,
        Dramatic = 2,
    };

    inline bool IsValidLightingPreset(LightingPreset a_preset)
    {
        return a_preset == LightingPreset::Neutral ||
               a_preset == LightingPreset::Bright ||
               a_preset == LightingPreset::Dramatic;
    }

    struct NifLoadInfo
    {
        std::uint32_t structSize = sizeof(NifLoadInfo);
        RenderLayer::SurfaceHandle surface = RenderLayer::INVALID_SURFACE_HANDLE;
        const char* modelPath = nullptr;
        bool frameOnLoad = true;
        std::uint8_t reserved[7] = {};
    };

    inline constexpr std::uint32_t NIF_LOAD_INFO_MIN_SIZE_1 =
        static_cast<std::uint32_t>(offsetof(NifLoadInfo, reserved) + sizeof(NifLoadInfo::reserved));
    static_assert(sizeof(NifLoadInfo) == NIF_LOAD_INFO_MIN_SIZE_1);

    struct CameraState
    {
        std::uint32_t structSize = sizeof(CameraState);
        float yawDegrees = 35.0f;
        float pitchDegrees = 15.0f;
        float distanceScale = 1.0f;
        float panX = 0.0f;
        float panY = 0.0f;
        float panZ = 0.0f;
        std::uint32_t reserved = 0;
        LightingPreset lightingPreset = LightingPreset::Neutral;
        float exposureStops = 0.0f;
        std::uint32_t lightingReserved[2] = {};
    };

    inline constexpr std::uint32_t CAMERA_STATE_MIN_SIZE_1 =
        static_cast<std::uint32_t>(offsetof(CameraState, reserved) + sizeof(CameraState::reserved));
    inline constexpr std::uint32_t CAMERA_STATE_LIGHTING_SIZE_1 =
        static_cast<std::uint32_t>(offsetof(CameraState, lightingReserved) +
                                   sizeof(CameraState::lightingReserved));
    static_assert(CAMERA_STATE_MIN_SIZE_1 == 32);
    static_assert(sizeof(CameraState) == CAMERA_STATE_LIGHTING_SIZE_1);

    inline bool IsSupported(const char* a_name, std::uint32_t a_version)
    {
        return a_name != nullptr &&
               std::strcmp(a_name, EXTENSION_NAME) == 0 &&
               a_version == INTERFACE_VERSION;
    }

    class INifViewAPI
    {
    public:
        virtual ~INifViewAPI() = default;

        virtual bool __cdecl LoadModel(const NifLoadInfo* a_info) = 0;
        virtual void __cdecl ClearModel(RenderLayer::SurfaceHandle a_surface) = 0;
        virtual Status __cdecl GetStatus(RenderLayer::SurfaceHandle a_surface) const = 0;
        virtual bool __cdecl SetCamera(RenderLayer::SurfaceHandle a_surface,
                                       const CameraState* a_camera) = 0;
        virtual bool __cdecl FrameModel(RenderLayer::SurfaceHandle a_surface) = 0;
    };

    using QueryMeridianExtensionFn = bool(__cdecl*)(const char* a_name,
                                                    std::uint32_t a_version,
                                                    void** a_outInterface,
                                                    Meridian::UI::Settings* a_settings,
                                                    const char* a_consumerName);
}
