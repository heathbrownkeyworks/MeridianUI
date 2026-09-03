#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace Meridian::Hooks
{
    enum class CallEncoding
    {
        Relative5,
        RipRelativeIndirect6,
    };

    constexpr std::size_t RequiredCallBytes(CallEncoding a_encoding)
    {
        return a_encoding == CallEncoding::Relative5 ? 5 : 6;
    }

    constexpr bool MatchesExpectedCall(
        std::span<const std::uint8_t> a_bytes,
        CallEncoding a_encoding)
    {
        if (a_bytes.size() < RequiredCallBytes(a_encoding))
        {
            return false;
        }

        switch (a_encoding)
        {
        case CallEncoding::Relative5:
            return a_bytes[0] == 0xE8;
        case CallEncoding::RipRelativeIndirect6:
            return a_bytes[0] == 0xFF && a_bytes[1] == 0x15;
        }

        return false;
    }
}
