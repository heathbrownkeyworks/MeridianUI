#pragma once

#include <cstddef>
#include <cstdint>

namespace Meridian::NifTest
{
    // DirectInput keyboard scan codes used by Skyrim button events.
    inline constexpr std::uint32_t KEY_N = 0x31;
    inline constexpr std::uint32_t KEY_LEFT_ALT = 0x38;
    inline constexpr std::uint32_t KEY_RIGHT_ALT = 0xB8;

    inline bool IsToggleHotkey(std::uint32_t a_keyCode,
                               bool a_isDown,
                               const std::uint8_t* a_keyState,
                               std::size_t a_stateSize)
    {
        if (!a_isDown || a_keyCode != KEY_N || a_keyState == nullptr)
        {
            return false;
        }
        const auto held = [&](std::uint32_t a_code) {
            return a_code < a_stateSize && (a_keyState[a_code] & 0x80) != 0;
        };
        return held(KEY_LEFT_ALT) || held(KEY_RIGHT_ALT);
    }
}
