#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace Meridian::Render::NifPreview
{
    inline constexpr std::size_t MAX_TEXTURE_RESOURCE_BYTES = 256u * 1024u * 1024u;

    enum class TextureReadError
    {
        None,
        InvalidPath,
        NotFound,
        Empty,
        TooLarge,
        ReadFailed,
    };

    struct TextureReadResult
    {
        TextureReadError error = TextureReadError::None;
        std::string resourcePath;
        std::vector<std::uint8_t> bytes;
    };

    TextureReadResult ReadSkyrimTexture(std::string_view a_texturePath);
}
