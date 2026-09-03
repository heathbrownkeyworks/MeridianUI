#pragma once

#include <cstdint>
#include <cstring>

namespace Meridian::UI
{
    /// <summary>
    /// Size-versioned struct ingestion. The caller's struct begins with its
    /// structSize; we copy min(caller size, our size) bytes over a
    /// default-initialized TSettings, so fields the caller predates keep
    /// their defaults. Callers older than a_minSize (the 1.0 layout) are
    /// refused — the pre-1.0 layouts had no structSize and cannot be read.
    ///
    /// Precondition: a_caller must be either nullptr or point to at least
    /// sizeof(std::uint32_t) readable bytes (the leading structSize field).
    /// Passing a non-null pointer backed by fewer bytes is undefined
    /// behavior — this function reads that prefix before any size check can
    /// run, the same as any other pointer contract in this API.
    ///
    /// A claimed structSize above 4096 is treated as corrupt and refused,
    /// regardless of a_minSize.
    /// </summary>
    template <typename TSettings>
    inline bool IngestSettings(const void* a_caller, std::uint32_t a_minSize, TSettings& a_out)
    {
        a_out = TSettings{};
        if (a_caller == nullptr)
        {
            return true;  // no settings supplied — defaults are the contract
        }

        std::uint32_t callerSize = 0;
        std::memcpy(&callerSize, a_caller, sizeof(callerSize));
        if (callerSize < a_minSize || callerSize > 4096)
        {
            return false;  // pre-1.0 caller or garbage — refuse cleanly
        }

        const std::uint32_t copySize = callerSize < sizeof(TSettings)
                                           ? callerSize
                                           : static_cast<std::uint32_t>(sizeof(TSettings));
        std::memcpy(&a_out, a_caller, copySize);
        a_out.structSize = sizeof(TSettings);
        return true;
    }
}
