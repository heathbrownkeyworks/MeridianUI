#pragma once

#include "Render/NifPreviewMesh.h"

#include <cmath>

namespace Meridian::Render::NifPreview
{
    struct CameraBasis
    {
        Float3 eyeDirection{};
        Float3 right{};
        Float3 up{};
    };

    inline CameraBasis BuildCameraBasis(float a_yawRadians, float a_pitchRadians)
    {
        const auto cosYaw = std::cos(a_yawRadians);
        const auto sinYaw = std::sin(a_yawRadians);
        const auto cosPitch = std::cos(a_pitchRadians);
        const auto sinPitch = std::sin(a_pitchRadians);
        return {
            .eyeDirection = {cosPitch * cosYaw, cosPitch * sinYaw, sinPitch},
            .right = {-sinYaw, cosYaw, 0.0f},
            .up = {-sinPitch * cosYaw, -sinPitch * sinYaw, cosPitch},
        };
    }

    inline Float3 ApplyNormalizedPan(const Float3& a_center,
                                     float a_radius,
                                     const CameraBasis& a_basis,
                                     float a_panX,
                                     float a_panY,
                                     float a_panZ)
    {
        return {
            a_center.x + a_radius * (a_basis.right.x * a_panX +
                                     a_basis.up.x * a_panY +
                                     a_basis.eyeDirection.x * a_panZ),
            a_center.y + a_radius * (a_basis.right.y * a_panX +
                                     a_basis.up.y * a_panY +
                                     a_basis.eyeDirection.y * a_panZ),
            a_center.z + a_radius * (a_basis.right.z * a_panX +
                                     a_basis.up.z * a_panY +
                                     a_basis.eyeDirection.z * a_panZ),
        };
    }
}
