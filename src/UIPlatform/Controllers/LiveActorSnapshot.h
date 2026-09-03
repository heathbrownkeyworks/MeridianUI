#pragma once

#include "MeridianUIAPI/NifSceneAPI.h"

#include <cstdint>

namespace Meridian::Controllers::LiveActorSnapshot
{
    enum class SourceKind : std::uint8_t
    {
        Equipment,
        Skin,
        Head,
    };

    [[nodiscard]] constexpr bool IsActorAppearanceBipedSlot(std::uint32_t a_slot)
    {
        return a_slot < 32;
    }

    [[nodiscard]] constexpr SourceKind ClassifyBipedSource(bool a_isSkin)
    {
        return a_isSkin ? SourceKind::Skin : SourceKind::Equipment;
    }

    [[nodiscard]] constexpr bool ShouldCaptureSource(SourceKind a_source,
                                                     std::uint32_t a_parts)
    {
        using namespace Meridian::UI::NifScene;
        switch (a_source)
        {
        case SourceKind::Equipment:
            return (a_parts & ACTOR_APPEARANCE_PART_EQUIPMENT) != 0;
        case SourceKind::Skin:
            return (a_parts & ACTOR_APPEARANCE_PART_SKIN) != 0;
        case SourceKind::Head:
            return (a_parts & ACTOR_APPEARANCE_PART_HEAD) != 0;
        }
        return false;
    }
}
