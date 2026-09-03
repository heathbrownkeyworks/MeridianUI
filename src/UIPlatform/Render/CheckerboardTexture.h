#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Meridian::Render
{
    struct RGBA8
    {
        std::uint8_t red = 0;
        std::uint8_t green = 0;
        std::uint8_t blue = 0;
        std::uint8_t alpha = 255;

        constexpr bool operator==(const RGBA8&) const = default;
    };

    inline constexpr std::uint32_t MAX_CHECKERBOARD_DIMENSION = 4096;

    inline std::vector<std::uint8_t> BuildCheckerboardRGBA8(
        std::uint32_t a_width,
        std::uint32_t a_height,
        std::uint32_t a_tileSize,
        RGBA8 a_first,
        RGBA8 a_second)
    {
        if (a_width == 0 || a_height == 0 || a_tileSize == 0 ||
            a_width > MAX_CHECKERBOARD_DIMENSION || a_height > MAX_CHECKERBOARD_DIMENSION)
        {
            return {};
        }

        const auto pixelCount = static_cast<std::size_t>(a_width) * a_height;
        std::vector<std::uint8_t> pixels(pixelCount * 4);

        for (std::uint32_t y = 0; y < a_height; ++y)
        {
            for (std::uint32_t x = 0; x < a_width; ++x)
            {
                const auto color = (((x / a_tileSize) + (y / a_tileSize)) % 2 == 0) ? a_first : a_second;
                const auto offset = (static_cast<std::size_t>(y) * a_width + x) * 4;
                pixels[offset] = color.red;
                pixels[offset + 1] = color.green;
                pixels[offset + 2] = color.blue;
                pixels[offset + 3] = color.alpha;
            }
        }

        return pixels;
    }
}
