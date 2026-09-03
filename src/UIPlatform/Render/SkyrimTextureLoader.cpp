#include "Render/SkyrimTextureLoader.h"

#include "Render/NifTexturePath.h"

#include <limits>

namespace Meridian::Render::NifPreview
{
    TextureReadResult ReadSkyrimTexture(std::string_view a_texturePath)
    {
        TextureReadResult result{};
        if (!NormalizeTextureResourcePath(a_texturePath, result.resourcePath))
        {
            result.error = TextureReadError::InvalidPath;
            return result;
        }

        RE::BSResourceNiBinaryStream stream(result.resourcePath);
        if (!stream.good())
        {
            result.error = TextureReadError::NotFound;
            return result;
        }

        RE::NiBinaryStream::BufferInfo info{};
        stream.get_info(info);
        std::uint64_t resourceSize = info.totalSize;
        if (resourceSize == 0 && stream.stream != nullptr)
        {
            resourceSize = stream.stream->totalSize;
        }
        if (resourceSize == 0)
        {
            result.error = TextureReadError::Empty;
            return result;
        }
        if (resourceSize > MAX_TEXTURE_RESOURCE_BYTES ||
            resourceSize > std::numeric_limits<std::uint32_t>::max())
        {
            result.error = TextureReadError::TooLarge;
            return result;
        }

        result.bytes.resize(static_cast<std::size_t>(resourceSize));
        if (!stream.read(result.bytes.data(), static_cast<std::uint32_t>(result.bytes.size())) ||
            stream.tell() != result.bytes.size())
        {
            result.bytes.clear();
            result.error = TextureReadError::ReadFailed;
            return result;
        }
        return result;
    }
}
