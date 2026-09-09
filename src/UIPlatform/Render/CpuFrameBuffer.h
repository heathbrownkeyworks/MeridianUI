#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <span>
#include <vector>

namespace Meridian::Render
{
    struct PixelRect { int x = 0, y = 0, width = 0, height = 0; };
    struct CpuFrameUpdate
    {
        int width = 0, height = 0;
        PixelRect dirty;
        std::uint64_t epoch = 0;
        std::vector<std::uint32_t> pixels; // tightly packed dirty region, straight-alpha BGRA
    };

    // One retained image, one coalesced dirty rectangle. No unbounded queue and
    // no borrowed CEF pointers. A consumer snapshot leaves this lock before GPU work.
    class CpuFrameBuffer
    {
    public:
        static constexpr std::size_t kMaxBytes = 128u * 1024u * 1024u;
        static constexpr int kMaxDimension = 16384;
        bool Submit(const void* pixels, int width, int height, std::span<const PixelRect> dirty);
        std::optional<CpuFrameUpdate> TakeUpdate();
        void RequestFullUpload();
        void Reset();
        void Stop();
        [[nodiscard]] bool IsCurrent(std::uint64_t epoch) const;

    private:
        void Clear(); // caller holds mutex
        mutable std::mutex m_mutex;
        std::vector<std::uint32_t> m_pixels;
        int m_width = 0, m_height = 0;
        PixelRect m_dirty;
        bool m_pending = false, m_stopped = false;
        std::uint64_t m_epoch = 1;
    };
}
