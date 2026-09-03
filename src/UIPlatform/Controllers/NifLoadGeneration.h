#pragma once

#include <atomic>
#include <cstdint>

namespace Meridian::Controllers
{
    inline std::uint64_t NextNifLoadGeneration()
    {
        static std::atomic<std::uint64_t> next{1};
        auto generation = next.fetch_add(1, std::memory_order_relaxed);
        while (generation == 0)
        {
            generation = next.fetch_add(1, std::memory_order_relaxed);
        }
        return generation;
    }
}
