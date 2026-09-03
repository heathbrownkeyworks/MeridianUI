#pragma once

#include "Render/NifPreviewMesh.h"

namespace Meridian::Render::NifPreview
{
    [[nodiscard]] constexpr Float3 FaceGenRgbDetailColor()
    {
        return {1.01172f, 0.996094f, 1.01172f};
    }

    [[nodiscard]] constexpr Float3 ApplyFaceGenOverlay(Float3 a_diffuse,
                                                       Float3 a_tint,
                                                       Float3 a_detail)
    {
        const auto blend = [](float a_base, float a_overlay) {
            return a_base * a_base +
                   2.0f * a_overlay * a_base -
                   2.0f * a_overlay * a_base * a_base;
        };
        return {
            blend(a_diffuse.x, a_tint.x) * a_detail.x,
            blend(a_diffuse.y, a_tint.y) * a_detail.y,
            blend(a_diffuse.z, a_tint.z) * a_detail.z,
        };
    }
}
