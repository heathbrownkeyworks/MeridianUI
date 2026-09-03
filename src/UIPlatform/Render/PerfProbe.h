#pragma once

#ifdef MERIDIAN_PERF_PROBE

    #include <algorithm>
    #include <array>
    #include <chrono>
    #include <cstdint>

    #include <spdlog/spdlog.h>

namespace Meridian::Render
{
    /// <summary>
    /// Rolling frame-cost probe for PostDisplay. Single-threaded by contract:
    /// only the game render thread calls Record.
    /// </summary>
    class PerfProbe
    {
    protected:
        static constexpr std::size_t kWindow = 600;
        std::array<std::uint64_t, kWindow> m_samples{};
        std::size_t m_count = 0;
        std::size_t m_next = 0;
        std::chrono::steady_clock::time_point m_lastReport = std::chrono::steady_clock::now();

    public:
        void Record(std::uint64_t a_microseconds, std::size_t a_layerCount)
        {
            m_samples[m_next] = a_microseconds;
            m_next = (m_next + 1) % kWindow;
            m_count = std::min(m_count + 1, kWindow);

            const auto now = std::chrono::steady_clock::now();
            if (now - m_lastReport < std::chrono::seconds(5) || m_count == 0)
            {
                return;
            }
            m_lastReport = now;

            std::array<std::uint64_t, kWindow> sorted{};
            std::copy_n(m_samples.begin(), m_count, sorted.begin());
            std::sort(sorted.begin(), sorted.begin() + m_count);

            const auto p50 = sorted[m_count / 2];
            const auto p99 = sorted[std::min(m_count - 1, (m_count * 99) / 100)];
            spdlog::info("PerfProbe: layers={} p50={}us p99={}us over {} frames", a_layerCount, p50, p99, m_count);
        }
    };
}

    #define MERIDIAN_PROBE_SCOPE_BEGIN() const auto probeStart_ = std::chrono::steady_clock::now()
    #define MERIDIAN_PROBE_SCOPE_END(probe, layerCount)                                                              \
        (probe).Record(static_cast<std::uint64_t>(                                                                   \
                           std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() -  \
                                                                                 probeStart_)                        \
                               .count()),                                                                            \
                       (layerCount))

#else

namespace Meridian::Render
{
    class PerfProbe
    {
    };
}

    #define MERIDIAN_PROBE_SCOPE_BEGIN() ((void)0)
    #define MERIDIAN_PROBE_SCOPE_END(probe, layerCount) ((void)0)

#endif
