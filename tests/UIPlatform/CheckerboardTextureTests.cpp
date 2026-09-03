#include "Render/CheckerboardTexture.h"

#include <cstddef>
#include <cstdint>
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

    Meridian::Render::RGBA8 PixelAt(const std::vector<std::uint8_t>& a_pixels,
                                    std::size_t a_width,
                                    std::size_t a_x,
                                    std::size_t a_y)
    {
        const auto offset = (a_y * a_width + a_x) * 4;
        return {
            a_pixels[offset],
            a_pixels[offset + 1],
            a_pixels[offset + 2],
            a_pixels[offset + 3],
        };
    }
}

int main()
{
    using Meridian::Render::BuildCheckerboardRGBA8;
    using Meridian::Render::RGBA8;

    constexpr RGBA8 first{ 1, 2, 3, 4 };
    constexpr RGBA8 second{ 10, 20, 30, 40 };
    const auto pixels = BuildCheckerboardRGBA8(4, 4, 2, first, second);

    Expect(pixels.size() == 4u * 4u * 4u, "checkerboard has exactly four RGBA bytes per pixel");
    Expect(PixelAt(pixels, 4, 0, 0) == first, "top-left tile uses the first color");
    Expect(PixelAt(pixels, 4, 1, 1) == first, "pixels remain in the same tile before the boundary");
    Expect(PixelAt(pixels, 4, 2, 0) == second, "horizontal tile boundary alternates colors");
    Expect(PixelAt(pixels, 4, 0, 2) == second, "vertical tile boundary alternates colors");
    Expect(PixelAt(pixels, 4, 2, 2) == first, "diagonal tiles return to the first color");

    Expect(pixels[0] == 1 && pixels[1] == 2 && pixels[2] == 3 && pixels[3] == 4,
           "pixel storage order is RGBA");
    Expect(BuildCheckerboardRGBA8(0, 4, 2, first, second).empty(), "zero width is rejected");
    Expect(BuildCheckerboardRGBA8(4, 0, 2, first, second).empty(), "zero height is rejected");
    Expect(BuildCheckerboardRGBA8(4, 4, 0, first, second).empty(), "zero tile size is rejected");
    Expect(BuildCheckerboardRGBA8(4097, 1, 1, first, second).empty(), "oversized dimensions are rejected");

    return g_failures == 0 ? 0 : 1;
}
