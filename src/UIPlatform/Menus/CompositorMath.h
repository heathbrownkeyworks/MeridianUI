#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Meridian::Menus::CompositorMath
{
    /// <summary>
    /// On-screen destination rect (pixels) plus the resolution scale.
    /// The browser's logical page size is rect × resolutionScale.
    /// </summary>
    struct LayerGeometry
    {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        float resolutionScale = 1.0f;
    };

    inline int LogicalWidth(const LayerGeometry& a_geometry)
    {
        return std::max(1, static_cast<int>(std::lround(a_geometry.width * a_geometry.resolutionScale)));
    }

    inline int LogicalHeight(const LayerGeometry& a_geometry)
    {
        return std::max(1, static_cast<int>(std::lround(a_geometry.height * a_geometry.resolutionScale)));
    }

    /// <summary>
    /// Draw-order key: lower zOrder draws first (further back); equal zOrder
    /// breaks ties by creation order. Strict weak ordering, safe for std::sort.
    /// </summary>
    struct LayerKey
    {
        int zOrder = 0;
        std::uint64_t creationSeq = 0;
    };

    inline bool DrawsBefore(const LayerKey& a_left, const LayerKey& a_right)
    {
        if (a_left.zOrder != a_right.zOrder)
        {
            return a_left.zOrder < a_right.zOrder;
        }
        return a_left.creationSeq < a_right.creationSeq;
    }

    /// <summary>
    /// Half-open containment: [x, x+width) × [y, y+height).
    /// </summary>
    inline bool HitTest(const LayerGeometry& a_geometry, int a_screenX, int a_screenY)
    {
        return a_screenX >= a_geometry.x && a_screenX < a_geometry.x + a_geometry.width &&
               a_screenY >= a_geometry.y && a_screenY < a_geometry.y + a_geometry.height;
    }

    /// <summary>
    /// Screen point to browser-local (logical page) coordinates.
    /// The page is rect × resolutionScale, so: local = (screen − origin) × scale.
    /// </summary>
    inline void ScreenToBrowser(const LayerGeometry& a_geometry, float a_screenX, float a_screenY, int& a_outX, int& a_outY)
    {
        a_outX = static_cast<int>((a_screenX - static_cast<float>(a_geometry.x)) * a_geometry.resolutionScale);
        a_outY = static_cast<int>((a_screenY - static_cast<float>(a_geometry.y)) * a_geometry.resolutionScale);
    }

    /// <summary>
    /// Screen-pixel rect, kept as a plain POD so this header stays free of
    /// Windows headers (no RECT). Convert to a platform RECT at the call site.
    /// </summary>
    struct ScreenRect
    {
        long left = 0;
        long top = 0;
        long right = 0;
        long bottom = 0;
    };

    /// <summary>Inverse of ScreenToBrowser for a rect in browser-logical
    /// coordinates: screen = geometry origin + local / resolutionScale.
    /// Returns {left, top, right, bottom} in screen pixels.</summary>
    inline ScreenRect BrowserRectToScreen(const LayerGeometry& a_geometry, int a_x, int a_y, int a_width, int a_height)
    {
        const float inverseScale = a_geometry.resolutionScale > 0.0f ? 1.0f / a_geometry.resolutionScale : 1.0f;
        return ScreenRect{
            a_geometry.x + static_cast<long>(a_x * inverseScale),
            a_geometry.y + static_cast<long>(a_y * inverseScale),
            a_geometry.x + static_cast<long>((a_x + a_width) * inverseScale),
            a_geometry.y + static_cast<long>((a_y + a_height) * inverseScale)};
    }

    /// <summary>
    /// Proportional rescale for a display-resolution change. resolutionScale is
    /// preserved. A degenerate old resolution returns the geometry unchanged.
    /// </summary>
    inline LayerGeometry RescaleForResolution(const LayerGeometry& a_geometry, int a_oldW, int a_oldH, int a_newW, int a_newH)
    {
        if (a_oldW <= 0 || a_oldH <= 0 || a_newW <= 0 || a_newH <= 0)
        {
            return a_geometry;
        }

        const float sx = static_cast<float>(a_newW) / static_cast<float>(a_oldW);
        const float sy = static_cast<float>(a_newH) / static_cast<float>(a_oldH);

        LayerGeometry result = a_geometry;
        result.x = static_cast<int>(std::lround(a_geometry.x * sx));
        result.y = static_cast<int>(std::lround(a_geometry.y * sy));
        result.width = static_cast<int>(std::lround(a_geometry.width * sx));
        result.height = static_cast<int>(std::lround(a_geometry.height * sy));
        return result;
    }

    /// <summary>
    /// Ring-slot bookkeeping for the frame transport, kept as pure logic so it
    /// is testable without a GPU. The writer must never pick the slot the
    /// reader last latched (readerSlot) nor the currently published slot.
    /// With 3 slots and one writer + one reader a free slot always exists.
    /// </summary>
    struct RingState
    {
        int writeSlot = 0;
        int publishedSlot = -1;
        int readerSlot = -1;
    };

    inline int NextWriteSlot(const RingState& a_state, int a_slotCount)
    {
        int candidate = a_state.writeSlot;
        for (int i = 0; i < a_slotCount; ++i)
        {
            candidate = (candidate + 1) % a_slotCount;
            if (candidate != a_state.publishedSlot && candidate != a_state.readerSlot)
            {
                return candidate;
            }
        }
        return a_state.writeSlot; // unreachable with slotCount >= 3
    }

    inline void Publish(RingState& a_state, int a_slotCount)
    {
        a_state.publishedSlot = a_state.writeSlot;
        a_state.writeSlot = NextWriteSlot(a_state, a_slotCount);
    }
}
