#pragma once

// Deliberately NOT including PCH.h/RE::Skyrim.h: CommonLibSSE-NG's own
// headers refuse to compile if any real Windows API header was already
// included first (REX::W32::BASE.h fires "#error Windows API detected" —
// it wants sole control of what Windows surface is visible so its own
// REX::W32 reimplementation types don't collide with the real ones). GDI
// rasterization needs the real <wingdi.h> declarations that the project
// PCH excludes via NOGDI, and once NOGDI has gated wingdi.h once in a
// translation unit, its include guard makes that permanent for the rest of
// that TU — so this pure Win32 + D3D11 helper is kept in its own
// PCH-free translation unit (see CMakeLists.txt's SKIP_PRECOMPILE_HEADERS
// on CursorRasterizer.cpp) where it can pull in a plain, un-gated
// <Windows.h> as the very first include, with CommonLibSSE-NG never
// entering the picture at all.
#include <wrl/client.h>
#include <Windows.h>
#include <d3d11.h>

namespace Meridian::Render
{
    struct RasterizedCursor
    {
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
        int width = 0;
        int height = 0;
        int hotspotX = 0;
        int hotspotY = 0;
    };

    /// <summary>
    /// Rasterizes a_cursor (DrawIconEx, hotspot from GetIconInfo) into an
    /// immutable D3D11 texture on a_device. Monochrome cursors (e.g. the
    /// I-beam) are detected and given alpha derived from their AND mask.
    /// </summary>
    bool RasterizeCursor(ID3D11Device* a_device, HCURSOR a_cursor, RasterizedCursor& a_out);
}
