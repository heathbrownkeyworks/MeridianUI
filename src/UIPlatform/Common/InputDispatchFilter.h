#pragma once

namespace Meridian::Common
{
    /// Selects the batch forwarded to an earlier-installed input hook. A batch
    /// already consumed by Meridian must be hidden before that hook can inspect
    /// it; otherwise the original pointer is preserved byte-for-byte.
    template <class InputList>
    constexpr InputList SelectForwardedInput(
        bool a_consumed,
        InputList a_original,
        InputList a_empty) noexcept
    {
        return a_consumed ? a_empty : a_original;
    }
}
