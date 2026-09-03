#pragma once

#include <cstdint>

namespace Meridian::Controllers::ActorAppearance
{
    inline constexpr std::uint32_t BIPED_HEAD = 1u << 0;
    inline constexpr std::uint32_t BIPED_HAIR = 1u << 1;
    inline constexpr std::uint32_t BIPED_BODY = 1u << 2;
    inline constexpr std::uint32_t BIPED_HANDS = 1u << 3;
    inline constexpr std::uint32_t BIPED_LONG_HAIR = 1u << 11;
    inline constexpr std::uint32_t BIPED_CIRCLET = 1u << 12;
    inline constexpr std::uint32_t BIPED_DECAPITATE_HEAD = 1u << 20;

    enum class HeadPartKind : std::uint32_t
    {
        Misc = 0,
        Face = 1,
        Eyes = 2,
        Hair = 3,
        FacialHair = 4,
        Scar = 5,
        Eyebrows = 6,
    };

    [[nodiscard]] constexpr bool ShouldIncludeSkinAddon(
        std::uint32_t a_addonSlots,
        std::uint32_t a_occupiedSlots)
    {
        return a_addonSlots != 0 && (a_addonSlots & a_occupiedSlots) == 0;
    }

    [[nodiscard]] constexpr bool ShouldIncludeHeadPart(
        HeadPartKind a_kind,
        std::uint32_t a_occupiedSlots)
    {
        const auto blocked = a_kind == HeadPartKind::Hair ?
            BIPED_HEAD | BIPED_HAIR | BIPED_LONG_HAIR :
            BIPED_HEAD | BIPED_DECAPITATE_HEAD;
        return (a_occupiedSlots & blocked) == 0;
    }
}
