#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Meridian::NifTest
{
    inline constexpr std::size_t BIPED_SLOT_COUNT = 32;
    inline constexpr std::size_t MAX_EQUIPPED_ARMORS = 32;

    enum class EquippedArmorManifestError
    {
        None,
        TooManyUniqueArmors,
    };

    struct EquippedArmorItem
    {
        std::uint32_t object = 0;
        std::uint32_t armorFormID = 0;
    };

    struct EquippedArmorManifestResult
    {
        EquippedArmorManifestError error = EquippedArmorManifestError::None;
        std::vector<EquippedArmorItem> items;
    };

    template <class SlotLookup>
    EquippedArmorManifestResult BuildEquippedArmorManifest(
        SlotLookup&& a_lookup,
        std::size_t a_maxItems = MAX_EQUIPPED_ARMORS)
    {
        EquippedArmorManifestResult result{};
        result.items.reserve(
            a_maxItems < BIPED_SLOT_COUNT ? a_maxItems : BIPED_SLOT_COUNT);
        for (std::size_t slotIndex = 0; slotIndex < BIPED_SLOT_COUNT; ++slotIndex)
        {
            const auto slotMask = std::uint32_t{1} << slotIndex;
            const auto armorFormID = static_cast<std::uint32_t>(
                a_lookup(slotMask));
            if (armorFormID == 0 ||
                std::any_of(
                    result.items.begin(),
                    result.items.end(),
                    [armorFormID](const EquippedArmorItem& a_item) {
                        return a_item.armorFormID == armorFormID;
                    }))
            {
                continue;
            }
            if (result.items.size() >= a_maxItems)
            {
                result.error = EquippedArmorManifestError::TooManyUniqueArmors;
                result.items.clear();
                return result;
            }
            result.items.push_back({
                .object = armorFormID,
                .armorFormID = armorFormID,
            });
        }
        return result;
    }
}
