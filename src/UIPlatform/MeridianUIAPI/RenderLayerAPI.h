// SPDX-License-Identifier: MIT

#pragma once

#include "Settings.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace Meridian::UI::RenderLayer
{
    inline constexpr char EXTENSION_NAME[] = "Meridian.RenderLayer";
    inline constexpr std::uint32_t INTERFACE_VERSION = 1;

    using SurfaceHandle = std::uint64_t;
    inline constexpr SurfaceHandle INVALID_SURFACE_HANDLE = 0;

    struct SurfaceCreateInfo
    {
        std::uint32_t structSize = sizeof(SurfaceCreateInfo);
        const char* ownerName = nullptr;
        const char* surfaceName = nullptr;
        std::int32_t x = 0;
        std::int32_t y = 0;
        std::int32_t width = 1;
        std::int32_t height = 1;
        std::int32_t zOrder = 0;
        bool initiallyVisible = false;
        std::uint8_t reserved[3] = {};
    };

    inline constexpr std::uint32_t SURFACE_CREATE_INFO_MIN_SIZE_1 =
        static_cast<std::uint32_t>(offsetof(SurfaceCreateInfo, reserved) + sizeof(SurfaceCreateInfo::reserved));
    static_assert(sizeof(SurfaceCreateInfo) == SURFACE_CREATE_INFO_MIN_SIZE_1);

    inline bool IsSupported(const char* a_name, std::uint32_t a_version)
    {
        return a_name != nullptr &&
               std::strcmp(a_name, EXTENSION_NAME) == 0 &&
               a_version == INTERFACE_VERSION;
    }

    class IRenderLayerAPI
    {
    public:
        virtual ~IRenderLayerAPI() = default;

        virtual SurfaceHandle __cdecl CreateSurface(const SurfaceCreateInfo* a_info) = 0;
        virtual void __cdecl DestroySurface(SurfaceHandle a_surface) = 0;
        virtual bool __cdecl IsValid(SurfaceHandle a_surface) const = 0;

        virtual bool __cdecl SetRect(SurfaceHandle a_surface,
                                     std::int32_t a_x,
                                     std::int32_t a_y,
                                     std::int32_t a_width,
                                     std::int32_t a_height) = 0;
        virtual bool __cdecl SetZOrder(SurfaceHandle a_surface, std::int32_t a_zOrder) = 0;
        virtual bool __cdecl SetVisible(SurfaceHandle a_surface, bool a_visible) = 0;
        virtual bool __cdecl IsVisible(SurfaceHandle a_surface) const = 0;
    };

    using QueryMeridianExtensionFn = bool(__cdecl*)(const char* a_name,
                                                    std::uint32_t a_version,
                                                    void** a_outInterface,
                                                    Meridian::UI::Settings* a_settings,
                                                    const char* a_consumerName);
}
