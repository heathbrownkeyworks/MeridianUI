#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace Meridian::Render::NifPreview
{
    inline bool NormalizeTextureResourcePath(std::string_view a_input, std::string& a_output)
    {
        constexpr std::size_t MAX_TEXTURE_PATH = 512;
        a_output.clear();
        if (a_input.empty() || a_input.size() > MAX_TEXTURE_PATH ||
            a_input.front() == '/' || a_input.front() == '\\' ||
            a_input.find(':') != std::string_view::npos)
        {
            return false;
        }

        a_output.assign(a_input);
        std::replace(a_output.begin(), a_output.end(), '/', '\\');

        const auto hasPrefix = [](std::string_view a_value, std::string_view a_prefix) {
            return a_value.size() >= a_prefix.size() &&
                   std::equal(a_prefix.begin(), a_prefix.end(), a_value.begin(), [](char a_left, char a_right) {
                       return std::tolower(static_cast<unsigned char>(a_left)) ==
                              std::tolower(static_cast<unsigned char>(a_right));
                   });
        };
        if (hasPrefix(a_output, "data\\"))
        {
            a_output.erase(0, 5);
        }
        if (!hasPrefix(a_output, "textures\\"))
        {
            a_output.insert(0, "textures\\");
        }

        std::size_t componentStart = 0;
        while (componentStart <= a_output.size())
        {
            const auto separator = a_output.find('\\', componentStart);
            const auto componentLength =
                (separator == std::string::npos ? a_output.size() : separator) - componentStart;
            const auto component = std::string_view(a_output).substr(componentStart, componentLength);
            if (component.empty() || component == "." || component == ".." ||
                std::any_of(component.begin(), component.end(), [](char a_character) {
                    const auto value = static_cast<unsigned char>(a_character);
                    return value < 0x20 || value == 0x7F;
                }))
            {
                a_output.clear();
                return false;
            }
            if (separator == std::string::npos)
            {
                break;
            }
            componentStart = separator + 1;
        }

        constexpr std::string_view extension = ".dds";
        if (a_output.size() <= extension.size() ||
            !std::equal(extension.rbegin(), extension.rend(), a_output.rbegin(), [](char a_left, char a_right) {
                return std::tolower(static_cast<unsigned char>(a_left)) ==
                       std::tolower(static_cast<unsigned char>(a_right));
            }))
        {
            a_output.clear();
            return false;
        }

        std::transform(a_output.begin(), a_output.end(), a_output.begin(), [](char a_character) {
            return static_cast<char>(std::tolower(static_cast<unsigned char>(a_character)));
        });
        return true;
    }
}
