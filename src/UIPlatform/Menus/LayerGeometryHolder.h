#pragma once

#include "CompositorMath.h"

#include <algorithm>
#include <mutex>

namespace Meridian::Menus
{
    /// <summary>
    /// Thread-safe geometry cell shared by the draft API (writer), the render
    /// layer (GetViewRect / draw destination), and the compositor (z, hit test).
    /// </summary>
    class LayerGeometryHolder
    {
    protected:
        mutable std::mutex m_mutex;
        CompositorMath::LayerGeometry m_geometry{};
        int m_zOrder = 0;
        bool m_explicitRect = false;

    public:
        CompositorMath::LayerGeometry Get() const
        {
            std::lock_guard lock(m_mutex);
            return m_geometry;
        }

        void SetRect(int a_x, int a_y, int a_width, int a_height)
        {
            std::lock_guard lock(m_mutex);
            m_geometry.x = a_x;
            m_geometry.y = a_y;
            m_geometry.width = std::max(1, a_width);
            m_geometry.height = std::max(1, a_height);
            m_explicitRect = true;
        }

        void SetResolutionScale(float a_scale)
        {
            std::lock_guard lock(m_mutex);
            m_geometry.resolutionScale = std::clamp(a_scale, 0.01f, 2.0f);
        }

        int GetZOrder() const
        {
            std::lock_guard lock(m_mutex);
            return m_zOrder;
        }

        void SetZOrder(int a_zOrder)
        {
            std::lock_guard lock(m_mutex);
            m_zOrder = a_zOrder;
        }

        bool HasExplicitRect() const
        {
            std::lock_guard lock(m_mutex);
            return m_explicitRect;
        }

        /// <summary>Fullscreen default; never overwrites an explicit rect.</summary>
        void SetFromResolution(int a_width, int a_height)
        {
            std::lock_guard lock(m_mutex);
            if (!m_explicitRect)
            {
                m_geometry.x = 0;
                m_geometry.y = 0;
                m_geometry.width = std::max(1, a_width);
                m_geometry.height = std::max(1, a_height);
            }
        }

        /// <summary>Proportional rescale on resolution change (applies to all rects).</summary>
        void Rescale(int a_oldW, int a_oldH, int a_newW, int a_newH)
        {
            std::lock_guard lock(m_mutex);
            m_geometry = CompositorMath::RescaleForResolution(m_geometry, a_oldW, a_oldH, a_newW, a_newH);
        }
    };
}
