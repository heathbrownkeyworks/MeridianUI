#include "Render/NifTexturePath.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace
{
    void Expect(bool a_condition, const char* a_message)
    {
        if (!a_condition)
        {
            std::cerr << "FAILED: " << a_message << '\n';
            std::exit(EXIT_FAILURE);
        }
    }
}

int main()
{
    using Meridian::Render::NifPreview::NormalizeTextureResourcePath;

    std::string normalized;
    Expect(NormalizeTextureResourcePath("textures/clutter/Coin01.DDS", normalized),
           "Data-relative texture path is accepted");
    Expect(normalized == "textures\\clutter\\coin01.dds",
           "texture separators and cache-key casing are normalized");
    Expect(NormalizeTextureResourcePath("clutter\\Coin01.dds", normalized) &&
               normalized == "textures\\clutter\\coin01.dds",
           "texture paths without the textures prefix are normalized under it");
    Expect(NormalizeTextureResourcePath("Data\\Textures\\Clutter\\Coin01.dds", normalized) &&
               normalized == "textures\\clutter\\coin01.dds",
           "an optional Data prefix is stripped");

    Expect(!NormalizeTextureResourcePath("C:\\temp\\coin.dds", normalized),
           "drive-absolute paths are rejected");
    Expect(!NormalizeTextureResourcePath("\\\\server\\share\\coin.dds", normalized),
           "UNC paths are rejected");
    Expect(!NormalizeTextureResourcePath("textures\\..\\outside.dds", normalized),
           "parent traversal is rejected");
    Expect(!NormalizeTextureResourcePath("textures\\clutter\\.\\coin.dds", normalized),
           "dot components are rejected");
    Expect(!NormalizeTextureResourcePath("textures\\clutter\\coin.png", normalized),
           "non-DDS textures are rejected by the DDS-only loader boundary");
    Expect(!NormalizeTextureResourcePath("textures\\clutter\\bad\x01.dds", normalized),
           "control characters are rejected");
    Expect(!NormalizeTextureResourcePath("", normalized), "empty texture paths are rejected");

    std::cout << "NIF texture path tests passed\n";
    return EXIT_SUCCESS;
}
