#pragma once

#include "MeridianUIAPI/NifViewAPI.h"
#include "Render/NifCameraMath.h"

#include <array>
#include <algorithm>
#include <cmath>

namespace Meridian::Render::NifPreview
{
    struct LightingRig
    {
        Float3 ambientTop{};
        Float3 ambientBottom{};
        std::array<Float3, 3> lightDirection{};
        std::array<Float3, 3> lightColor{};
        float exposureMultiplier = 1.0f;
    };

    namespace LightingDetail
    {
        struct Recipe
        {
            Float3 ambientTop;
            Float3 ambientBottom;
            std::array<Float3, 3> lightColor;
            float baseExposureStops;
        };

        inline Float3 Normalize(const Float3& a_value)
        {
            const auto length = std::sqrt(
                a_value.x * a_value.x + a_value.y * a_value.y + a_value.z * a_value.z);
            if (!std::isfinite(length) || length <= 0.00001f)
            {
                return {0.0f, 0.0f, 1.0f};
            }
            return {a_value.x / length, a_value.y / length, a_value.z / length};
        }

        inline Float3 CameraRelative(const CameraBasis& a_basis,
                                     float a_eye,
                                     float a_right,
                                     float a_up)
        {
            return Normalize({
                a_basis.eyeDirection.x * a_eye + a_basis.right.x * a_right +
                    a_basis.up.x * a_up,
                a_basis.eyeDirection.y * a_eye + a_basis.right.y * a_right +
                    a_basis.up.y * a_up,
                a_basis.eyeDirection.z * a_eye + a_basis.right.z * a_right +
                    a_basis.up.z * a_up,
            });
        }

        inline Recipe SelectRecipe(Meridian::UI::NifView::LightingPreset a_preset)
        {
            using Meridian::UI::NifView::LightingPreset;
            switch (a_preset)
            {
            case LightingPreset::Bright:
                return {
                    .ambientTop = {0.45f, 0.48f, 0.54f},
                    .ambientBottom = {0.18f, 0.20f, 0.24f},
                    .lightColor = {{{1.18f, 1.10f, 0.98f},
                                    {0.62f, 0.70f, 0.84f},
                                    {0.34f, 0.36f, 0.42f}}},
                    .baseExposureStops = 0.45f,
                };
            case LightingPreset::Dramatic:
                return {
                    .ambientTop = {0.13f, 0.14f, 0.17f},
                    .ambientBottom = {0.025f, 0.03f, 0.045f},
                    .lightColor = {{{1.30f, 1.12f, 0.90f},
                                    {0.12f, 0.16f, 0.24f},
                                    {0.62f, 0.70f, 0.86f}}},
                    .baseExposureStops = 0.05f,
                };
            case LightingPreset::Neutral:
            default:
                return {
                    .ambientTop = {0.32f, 0.35f, 0.40f},
                    .ambientBottom = {0.10f, 0.12f, 0.16f},
                    .lightColor = {{{1.08f, 1.01f, 0.91f},
                                    {0.43f, 0.50f, 0.61f},
                                    {0.28f, 0.31f, 0.37f}}},
                    .baseExposureStops = 0.15f,
                };
            }
        }
    }

    inline LightingRig BuildLightingRig(
        Meridian::UI::NifView::LightingPreset a_preset,
        float a_exposureStops,
        const CameraBasis& a_basis)
    {
        if (!Meridian::UI::NifView::IsValidLightingPreset(a_preset))
        {
            a_preset = Meridian::UI::NifView::LightingPreset::Neutral;
        }
        const auto recipe = LightingDetail::SelectRecipe(a_preset);
        const auto exposureStops = std::clamp(
            std::isfinite(a_exposureStops) ? a_exposureStops : 0.0f, -2.0f, 2.0f);

        return {
            .ambientTop = recipe.ambientTop,
            .ambientBottom = recipe.ambientBottom,
            .lightDirection = {{
                LightingDetail::CameraRelative(a_basis, 0.72f, -0.46f, 0.52f),
                LightingDetail::CameraRelative(a_basis, 0.52f, 0.78f, 0.12f),
                LightingDetail::CameraRelative(a_basis, -0.58f, -0.18f, 0.76f),
            }},
            .lightColor = recipe.lightColor,
            .exposureMultiplier = std::exp2(recipe.baseExposureStops + exposureStops),
        };
    }
}
