#pragma once

#include "Controllers/NifModelPath.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>
#include <string_view>

namespace Meridian::Controllers
{
    inline constexpr std::int8_t ARMA_WEIGHT_SLIDER_ENABLED = 2;

    enum class ArmorModelPathError
    {
        None,
        InvalidPath,
        MissingWeightSuffix,
    };

    struct ArmorModelPaths
    {
        ArmorModelPathError error = ArmorModelPathError::InvalidPath;
        std::string lowModelPath;
        std::string highModelPath;

        explicit operator bool() const
        {
            return error == ArmorModelPathError::None;
        }
    };

    inline bool EndsWithInsensitive(std::string_view a_value, std::string_view a_suffix)
    {
        return a_value.size() >= a_suffix.size() &&
               std::equal(
                   a_suffix.rbegin(),
                   a_suffix.rend(),
                   a_value.rbegin(),
                   [](char a_left, char a_right) {
                       return std::tolower(static_cast<unsigned char>(a_left)) ==
                              std::tolower(static_cast<unsigned char>(a_right));
                   });
    }

    inline bool EqualsInsensitive(std::string_view a_left, std::string_view a_right)
    {
        return a_left.size() == a_right.size() &&
               std::equal(
                   a_left.begin(),
                   a_left.end(),
                   a_right.begin(),
                   [](char a_lhs, char a_rhs) {
                       return std::tolower(static_cast<unsigned char>(a_lhs)) ==
                              std::tolower(static_cast<unsigned char>(a_rhs));
                   });
    }

    inline bool IsIntentionalBlankArmorModelPath(std::string_view a_modelPath)
    {
        const auto separator = a_modelPath.find_last_of("\\/");
        const auto filename = separator == std::string_view::npos ?
            a_modelPath : a_modelPath.substr(separator + 1);
        return EqualsInsensitive(filename, "blank.nif") ||
               EndsWithInsensitive(filename, "_blank.nif");
    }

    inline ArmorModelPaths ResolveArmorModelPaths(
        std::string_view a_modelPath,
        std::int8_t a_weightSlider)
    {
        ArmorModelPaths result{};
        std::string normalized;
        if (!NormalizeNifModelPath(a_modelPath, normalized))
        {
            return result;
        }

        result.error = ArmorModelPathError::None;
        result.lowModelPath = normalized;
        if (a_weightSlider != ARMA_WEIGHT_SLIDER_ENABLED)
        {
            return result;
        }

        if (EndsWithInsensitive(normalized, "_0.nif"))
        {
            result.highModelPath = normalized;
            result.highModelPath[result.highModelPath.size() - 5] = '1';
            return result;
        }
        if (EndsWithInsensitive(normalized, "_1.nif"))
        {
            result.highModelPath = normalized;
            result.lowModelPath[result.lowModelPath.size() - 5] = '0';
            return result;
        }

        return {
            .error = ArmorModelPathError::MissingWeightSuffix,
        };
    }
}
