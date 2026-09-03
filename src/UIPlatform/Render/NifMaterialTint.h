#pragma once

#include "Render/NifPreviewMesh.h"

#include <cmath>
#include <cstdint>

namespace Meridian::Render::NifPreview
{
    struct ActorMaterialTintOverride
    {
        Float3 bodyTint{1.0f, 1.0f, 1.0f};
        Float3 hairTint{1.0f, 1.0f, 1.0f};
        bool hasBodyTint = false;
        bool hasHairTint = false;
    };

    [[nodiscard]] inline bool IsValidMaterialTint(Float3 a_color)
    {
        return std::isfinite(a_color.x) &&
               std::isfinite(a_color.y) &&
               std::isfinite(a_color.z) &&
               a_color.x >= 0.0f && a_color.x <= 2.0f &&
               a_color.y >= 0.0f && a_color.y <= 2.0f &&
               a_color.z >= 0.0f && a_color.z <= 2.0f;
    }

    [[nodiscard]] inline ActorMaterialTintOverride MakeActorMaterialTintOverride(
        std::uint8_t a_bodyRed,
        std::uint8_t a_bodyGreen,
        std::uint8_t a_bodyBlue,
        std::uint8_t a_hairRed,
        std::uint8_t a_hairGreen,
        std::uint8_t a_hairBlue,
        bool a_hasHairTint)
    {
        ActorMaterialTintOverride result{};
        result.bodyTint = {
            static_cast<float>(a_bodyRed) / 255.0f,
            static_cast<float>(a_bodyGreen) / 255.0f,
            static_cast<float>(a_bodyBlue) / 255.0f,
        };
        result.hairTint = {
            static_cast<float>(a_hairRed) / 128.0f,
            static_cast<float>(a_hairGreen) / 128.0f,
            static_cast<float>(a_hairBlue) / 128.0f,
        };
        result.hasBodyTint = true;
        result.hasHairTint = a_hasHairTint;
        return result;
    }

    inline bool ApplyActorMaterialTint(
        PreviewMaterial& a_material,
        const ActorMaterialTintOverride& a_override)
    {
        switch (a_material.tintKind)
        {
        case MaterialTintKind::Body:
            if (a_override.hasBodyTint && IsValidMaterialTint(a_override.bodyTint))
            {
                a_material.tintColor = a_override.bodyTint;
                return true;
            }
            break;
        case MaterialTintKind::Hair:
            if (a_override.hasHairTint && IsValidMaterialTint(a_override.hairTint))
            {
                a_material.tintColor = a_override.hairTint;
                return true;
            }
            break;
        case MaterialTintKind::None:
        default:
            break;
        }
        return false;
    }
}
