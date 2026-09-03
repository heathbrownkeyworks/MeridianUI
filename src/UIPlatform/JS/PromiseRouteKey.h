#pragma once

#include <cstdint>

namespace Meridian::JS
{
    /// <summary>Composite route key for PromiseRouter: callIds are only
    /// unique within a single renderer process (see PromiseRouter's class
    /// comment), so the high 32 bits disambiguate by browser id and the
    /// low 32 bits carry the callId as-is. MakeRouteKey and
    /// RouteKeyBrowserIdBits are the ONLY two places that know this
    /// layout — keep it that way.</summary>
    constexpr std::uint64_t MakeRouteKey(int a_browserId, std::int32_t a_callId) noexcept
    {
        return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(a_browserId)) << 32) |
               static_cast<std::uint32_t>(a_callId);
    }

    /// <summary>Inverse of MakeRouteKey's browser-id half: the high 32 bits
    /// of a route key, as the same uint32 MakeRouteKey packed.</summary>
    constexpr std::uint32_t RouteKeyBrowserIdBits(std::uint64_t a_routeKey) noexcept
    {
        return static_cast<std::uint32_t>(a_routeKey >> 32);
    }

    // Layout round-trip pinned at compile time (negative ids exercise the
    // uint32-cast-before-widening rule).
    static_assert(MakeRouteKey(1, 1) == 0x0000'0001'0000'0001ull);
    static_assert(MakeRouteKey(-1, 7) == 0xFFFF'FFFF'0000'0007ull);
    static_assert(MakeRouteKey(3, -2) == 0x0000'0003'FFFF'FFFEull);
    static_assert(RouteKeyBrowserIdBits(MakeRouteKey(-1, 7)) == 0xFFFF'FFFFu);
    static_assert(RouteKeyBrowserIdBits(MakeRouteKey(3, -2)) == 3u);
}
