#include "MeridianUIAPI/RenderLayerAPI.h"

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
    using namespace Meridian::UI::RenderLayer;

    static_assert(std::is_standard_layout_v<SurfaceCreateInfo>);
    static_assert(std::is_same_v<SurfaceHandle, std::uint64_t>);
    static_assert(INVALID_SURFACE_HANDLE == 0);

    SurfaceCreateInfo info{};
    Expect(info.structSize == sizeof(SurfaceCreateInfo), "surface create info advertises its compiled size");
    Expect(info.x == 0 && info.y == 0, "surface position defaults to the origin");
    Expect(info.width == 1 && info.height == 1, "surface dimensions default to a valid pixel");
    Expect(info.zOrder == 0, "surface z-order defaults to zero");
    Expect(!info.initiallyVisible, "surfaces default hidden");
    Expect(SURFACE_CREATE_INFO_MIN_SIZE_1 == sizeof(SurfaceCreateInfo), "version 1 minimum size is frozen");

    Expect(IsSupported("Meridian.RenderLayer", 1), "exact Meridian.RenderLayer/1 query is supported");
    Expect(!IsSupported("Meridian.RenderLayer", 2), "future interface versions are rejected");
    Expect(!IsSupported("meridian.renderlayer", 1), "extension name matching is exact");
    Expect(!IsSupported(nullptr, 1), "null extension names are rejected");

    return g_failures == 0 ? 0 : 1;
}
