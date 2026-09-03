#pragma once

#include "Controllers/NifModelPath.h"
#include "MeridianUIAPI/NifSceneAPI.h"
#include "MeridianUIAPI/NifViewAPI.h"

#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

namespace Meridian::NifTest
{
    struct CameraPayload
    {
        float yawDegrees = 0.0f;
        float pitchDegrees = 0.0f;
        float distanceScale = 1.0f;
        float panX = 0.0f;
        float panY = 0.0f;
        float panZ = 0.0f;
    };

    struct LayoutPayload
    {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
    };

    struct LightingPayload
    {
        Meridian::UI::NifView::LightingPreset preset =
            Meridian::UI::NifView::LightingPreset::Neutral;
        float exposureStops = 0.0f;
    };

    struct ObjectVisibilityPayload
    {
        Meridian::UI::NifScene::ObjectHandle object =
            Meridian::UI::NifScene::INVALID_OBJECT_HANDLE;
        bool visible = true;
    };

    namespace Detail
    {
        template<class T, std::size_t N>
        std::optional<std::array<T, N>> ParseCsv(const char* a_payload)
        {
            if (a_payload == nullptr)
            {
                return std::nullopt;
            }

            const std::string_view text(a_payload);
            if (text.empty())
            {
                return std::nullopt;
            }

            std::array<T, N> values{};
            std::size_t offset = 0;
            for (std::size_t index = 0; index < N; ++index)
            {
                const auto separator = text.find(',', offset);
                const auto endOffset = separator == std::string_view::npos ? text.size() : separator;
                if (endOffset == offset)
                {
                    return std::nullopt;
                }

                const char* begin = text.data() + offset;
                const char* end = text.data() + endOffset;
                const auto [parsedEnd, error] = std::from_chars(begin, end, values[index]);
                if (error != std::errc{} || parsedEnd != end)
                {
                    return std::nullopt;
                }

                if (index + 1 < N)
                {
                    if (separator == std::string_view::npos)
                    {
                        return std::nullopt;
                    }
                    offset = separator + 1;
                }
                else if (separator != std::string_view::npos)
                {
                    return std::nullopt;
                }
            }
            return values;
        }
    }

    inline std::optional<CameraPayload> ParseCameraPayload(const char* a_payload)
    {
        const auto values = Detail::ParseCsv<float, 6>(a_payload);
        if (!values)
        {
            return std::nullopt;
        }
        for (const auto value : *values)
        {
            if (!std::isfinite(value))
            {
                return std::nullopt;
            }
        }

        CameraPayload result{
            .yawDegrees = (*values)[0],
            .pitchDegrees = (*values)[1],
            .distanceScale = (*values)[2],
            .panX = (*values)[3],
            .panY = (*values)[4],
            .panZ = (*values)[5],
        };
        if (result.pitchDegrees < -89.0f || result.pitchDegrees > 89.0f ||
            result.distanceScale < 0.05f || result.distanceScale > 20.0f ||
            std::abs(result.panX) > 10.0f ||
            std::abs(result.panY) > 10.0f ||
            std::abs(result.panZ) > 10.0f)
        {
            return std::nullopt;
        }
        return result;
    }

    inline std::optional<LayoutPayload> ParseLayoutPayload(const char* a_payload)
    {
        const auto values = Detail::ParseCsv<int, 4>(a_payload);
        if (!values)
        {
            return std::nullopt;
        }

        LayoutPayload result{
            .x = (*values)[0],
            .y = (*values)[1],
            .width = (*values)[2],
            .height = (*values)[3],
        };
        constexpr int MAX_COORDINATE = 8192;
        constexpr int MAX_SIZE = 8192;
        if (result.x < -MAX_COORDINATE || result.x > MAX_COORDINATE ||
            result.y < -MAX_COORDINATE || result.y > MAX_COORDINATE ||
            result.width <= 0 || result.width > MAX_SIZE ||
            result.height <= 0 || result.height > MAX_SIZE)
        {
            return std::nullopt;
        }
        return result;
    }

    inline std::optional<LightingPayload> ParseLightingPayload(const char* a_payload)
    {
        const auto values = Detail::ParseCsv<float, 2>(a_payload);
        if (!values || !std::isfinite((*values)[0]) || !std::isfinite((*values)[1]) ||
            std::trunc((*values)[0]) != (*values)[0] ||
            (*values)[0] < 0.0f || (*values)[0] > 2.0f)
        {
            return std::nullopt;
        }

        const auto preset = static_cast<Meridian::UI::NifView::LightingPreset>(
            static_cast<std::uint32_t>((*values)[0]));
        if (!Meridian::UI::NifView::IsValidLightingPreset(preset) ||
            (*values)[1] < -2.0f || (*values)[1] > 2.0f)
        {
            return std::nullopt;
        }
        return LightingPayload{.preset = preset, .exposureStops = (*values)[1]};
    }

    inline std::optional<ObjectVisibilityPayload> ParseObjectVisibilityPayload(
        const char* a_payload)
    {
        const auto values = Detail::ParseCsv<std::uint64_t, 2>(a_payload);
        if (!values || (*values)[0] == Meridian::UI::NifScene::INVALID_OBJECT_HANDLE ||
            (*values)[1] > 1)
        {
            return std::nullopt;
        }
        return ObjectVisibilityPayload{
            .object = (*values)[0],
            .visible = (*values)[1] == 1,
        };
    }

    inline std::optional<float> ParseWeightPayload(const char* a_payload)
    {
        const auto values = Detail::ParseCsv<float, 1>(a_payload);
        if (!values || !std::isfinite((*values)[0]) ||
            (*values)[0] < 0.0f || (*values)[0] > 100.0f)
        {
            return std::nullopt;
        }
        return (*values)[0];
    }

    inline std::optional<std::string> ParseModelPathPayload(const char* a_payload)
    {
        if (a_payload == nullptr)
        {
            return std::nullopt;
        }
        std::string normalized;
        if (!Meridian::Controllers::NormalizeNifModelPath(a_payload, normalized))
        {
            return std::nullopt;
        }
        return normalized;
    }
}
