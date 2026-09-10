#pragma once
#include "Menus/CompositorMath.h"
#include <algorithm>
#include <utility>
namespace Meridian::Input
{
    inline std::pair<float, float> AdvanceCursor(float x, float y, float dx, float dy, const Menus::CompositorMath::LayerGeometry& geometry)
    {
        const float scale = std::max(1, geometry.height) / 1080.0f;
        return {std::clamp(x + dx * scale, static_cast<float>(geometry.x), static_cast<float>(geometry.x + std::max(1, geometry.width) - 1)),
                std::clamp(y + dy * scale, static_cast<float>(geometry.y), static_cast<float>(geometry.y + std::max(1, geometry.height) - 1))};
    }
    class PointerButtonState
    {
    public:
        int Apply(int request)
        {
            if (request == 1 && !m_down)
            {
                m_down = true;
                return 1;
            }
            if (request == 0 && m_down)
            {
                m_down = false;
                return 0;
            }
            return -1;
        }
        bool Down() const
        {
            return m_down;
        }

    private:
        bool m_down = false;
    };
}
