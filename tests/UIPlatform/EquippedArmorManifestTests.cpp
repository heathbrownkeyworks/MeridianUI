#include "EquippedArmorManifest.h"

#include <cstdint>
#include <iostream>
#include <unordered_map>

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
    using namespace Meridian::NifTest;

    const std::unordered_map<std::uint32_t, std::uint32_t> worn{
        {1u << 0, 0x00013913},
        {1u << 2, 0x00013911},
        {1u << 3, 0x00013911},
        {1u << 31, 0xFE001234},
    };
    std::uint32_t calls = 0;
    const auto result = BuildEquippedArmorManifest(
        [&](std::uint32_t a_slotMask) {
            ++calls;
            const auto found = worn.find(a_slotMask);
            return found != worn.end() ? found->second : 0u;
        });

    Expect(calls == 32, "every Skyrim biped slot is inspected exactly once");
    Expect(result.error == EquippedArmorManifestError::None,
           "a bounded equipped outfit succeeds");
    Expect(result.items.size() == 3,
           "empty slots are ignored and multi-slot armor is deduplicated");
    Expect(result.items[0].armorFormID == 0x00013913 &&
               result.items[1].armorFormID == 0x00013911 &&
               result.items[2].armorFormID == 0xFE001234,
           "unique armor records retain first-seen biped-slot order");
    Expect(result.items[0].object == result.items[0].armorFormID &&
               result.items[1].object == result.items[1].armorFormID &&
               result.items[2].object == result.items[2].armorFormID,
           "runtime ARMO FormIDs become stable nonzero public handles");

    const auto capped = BuildEquippedArmorManifest(
        [](std::uint32_t a_slotMask) { return a_slotMask; }, 2);
    Expect(capped.error == EquippedArmorManifestError::TooManyUniqueArmors &&
               capped.items.empty(),
           "an excessive manifest is rejected without exposing a partial outfit");

    if (g_failures != 0)
    {
        std::cerr << g_failures << " equipped armor manifest test(s) failed\n";
        return 1;
    }
    std::cout << "All equipped armor manifest tests passed\n";
    return 0;
}
