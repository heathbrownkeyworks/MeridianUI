#pragma once

#include <cstddef>
#include <cstdint>

namespace Meridian::Common
{
    /// <summary>True when the registered chord is held. A code of 0 means
    /// "unset half" — a chord with both halves 0 never fires (unarmed).
    /// a_keyState is the 256-byte DirectInput-style state array where bit
    /// 0x80 = held; a_stateSize guards the lookup.</summary>
    inline bool ChordSatisfied(std::uint32_t a_code1, std::uint32_t a_code2,
                               const std::uint8_t* a_keyState, std::size_t a_stateSize)
    {
        if ((a_code1 == 0 && a_code2 == 0) || a_keyState == nullptr)
        {
            return false;
        }
        const auto held = [&](std::uint32_t code) {
            return code == 0 || (code < a_stateSize && (a_keyState[code] & 0x80) != 0);
        };
        return held(a_code1) && held(a_code2);
    }
}
