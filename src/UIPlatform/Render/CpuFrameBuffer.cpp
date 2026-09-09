#include "CpuFrameBuffer.h"

#include <algorithm>
#include <cstring>
#include <new>

namespace Meridian::Render
{
    namespace
    {
        PixelRect Clip(PixelRect rect, int width, int height)
        {
            if (rect.width <= 0 || rect.height <= 0) return {};
            const auto left = std::clamp<std::int64_t>(rect.x, 0, width);
            const auto top = std::clamp<std::int64_t>(rect.y, 0, height);
            const auto right = std::clamp<std::int64_t>(std::int64_t(rect.x) + rect.width, 0, width);
            const auto bottom = std::clamp<std::int64_t>(std::int64_t(rect.y) + rect.height, 0, height);
            return {int(left), int(top), int(std::max<std::int64_t>(0, right - left)), int(std::max<std::int64_t>(0, bottom - top))};
        }

        std::uint32_t StraightAlpha(std::uint32_t bgra)
        {
            const auto alpha = bgra >> 24;
            if (alpha == 255) return bgra;
            if (alpha == 0) return 0;
            auto result = alpha << 24;
            for (unsigned shift = 0; shift < 24; shift += 8)
                result |= std::min(255u, (((bgra >> shift) & 255u) * 255u + alpha / 2) / alpha) << shift;
            return result;
        }
    }

    bool CpuFrameBuffer::Submit(const void* pixels, int width, int height, std::span<const PixelRect> dirty)
    {
        if (!pixels || width <= 0 || height <= 0 || width > kMaxDimension || height > kMaxDimension ||
            std::size_t(width) * height > kMaxBytes / sizeof(std::uint32_t)) return false;
        std::lock_guard lock(m_mutex);
        if (m_stopped) return false;
        const bool resized = width != m_width || height != m_height;
        if (resized)
        {
            try
            {
                std::vector<std::uint32_t> resizedPixels(std::size_t(width) * height);
                m_pixels.swap(resizedPixels);
            }
            catch (const std::bad_alloc&) { return false; }
            m_width = width;
            m_height = height;
            ++m_epoch;
            m_pending = false;
        }
        const PixelRect full{0, 0, width, height};
        if (resized || dirty.empty()) dirty = std::span(&full, 1);
        const auto* source = static_cast<const std::uint8_t*>(pixels);
        for (auto rect : dirty)
        {
            rect = Clip(rect, width, height);
            if (rect.width == 0 || rect.height == 0) continue;
            for (int y = rect.y; y < rect.y + rect.height; ++y)
            {
                auto offset = std::size_t(y) * width + rect.x;
                for (int x = 0; x < rect.width; ++x, ++offset)
                {
                    std::uint32_t pixel;
                    std::memcpy(&pixel, source + offset * 4, 4);
                    m_pixels[offset] = StraightAlpha(pixel);
                }
            }
            if (!m_pending) m_dirty = rect;
            else
            {
                const auto right = std::max(m_dirty.x + m_dirty.width, rect.x + rect.width);
                const auto bottom = std::max(m_dirty.y + m_dirty.height, rect.y + rect.height);
                m_dirty.x = std::min(m_dirty.x, rect.x);
                m_dirty.y = std::min(m_dirty.y, rect.y);
                m_dirty.width = right - m_dirty.x;
                m_dirty.height = bottom - m_dirty.y;
            }
            m_pending = true;
        }
        return true;
    }

    std::optional<CpuFrameUpdate> CpuFrameBuffer::TakeUpdate()
    {
        std::lock_guard lock(m_mutex);
        if (m_stopped || !m_pending) return std::nullopt;
        CpuFrameUpdate update{m_width, m_height, m_dirty, m_epoch, {}};
        try { update.pixels.resize(std::size_t(m_dirty.width) * m_dirty.height); }
        catch (const std::bad_alloc&) { return std::nullopt; }
        for (int y = 0; y < m_dirty.height; ++y)
            std::memcpy(update.pixels.data() + std::size_t(y) * m_dirty.width,
                m_pixels.data() + std::size_t(m_dirty.y + y) * m_width + m_dirty.x,
                std::size_t(m_dirty.width) * 4);
        m_pending = false;
        return update;
    }

    void CpuFrameBuffer::RequestFullUpload()
    {
        std::lock_guard lock(m_mutex);
        if (!m_stopped && !m_pixels.empty())
        {
            m_dirty = {0, 0, m_width, m_height};
            m_pending = true;
        }
    }

    void CpuFrameBuffer::Clear()
    {
        std::vector<std::uint32_t>().swap(m_pixels);
        m_width = m_height = 0;
        m_pending = false;
        ++m_epoch;
    }
    void CpuFrameBuffer::Reset() { std::lock_guard lock(m_mutex); Clear(); }
    void CpuFrameBuffer::Stop() { std::lock_guard lock(m_mutex); m_stopped = true; Clear(); }
    bool CpuFrameBuffer::IsCurrent(std::uint64_t epoch) const
    {
        std::lock_guard lock(m_mutex);
        return !m_stopped && !m_pixels.empty() && m_epoch == epoch;
    }
}
