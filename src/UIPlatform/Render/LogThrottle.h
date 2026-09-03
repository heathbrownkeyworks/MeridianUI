#pragma once

#include <chrono>
#include <cstdint>

namespace Meridian::Render
{
    // One log line per window, carrying the suppressed count.
    struct LogThrottle
    {
        std::chrono::steady_clock::time_point windowStart{};
        std::uint32_t suppressed = 0;

        // Returns true when the caller should log now; a_outSuppressed is the
        // count of occurrences swallowed since the last logged line.
        bool ShouldLog(std::uint32_t& a_outSuppressed)
        {
            const auto now = std::chrono::steady_clock::now();
            if (windowStart == std::chrono::steady_clock::time_point{} ||
                now - windowStart >= std::chrono::seconds(5))
            {
                windowStart = now;
                a_outSuppressed = suppressed;
                suppressed = 0;
                return true;
            }
            ++suppressed;
            return false;
        }
    };
}
