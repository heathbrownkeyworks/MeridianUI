// GDI must be available here (DrawIconEx, CreateDIBSection, CreateCompatibleDC,
// GdiFlush, et al. all live in wingdi.h). See CursorRasterizer.h for why this
// stays a plain, un-gated <Windows.h> with no CommonLibSSE-NG in the same
// translation unit.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include "CursorRasterizer.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace Meridian::Render
{
    bool RasterizeCursor(ID3D11Device* a_device, HCURSOR a_cursor, RasterizedCursor& a_out)
    {
        ICONINFO iconInfo{};
        if (!::GetIconInfo(a_cursor, &iconInfo))
        {
            return false;
        }
        // GetIconInfo hands us bitmap ownership — release on every path.
        const auto releaseBitmaps = [&]() {
            if (iconInfo.hbmColor) ::DeleteObject(iconInfo.hbmColor);
            if (iconInfo.hbmMask) ::DeleteObject(iconInfo.hbmMask);
        };

        int w = ::GetSystemMetrics(SM_CXCURSOR);
        int h = ::GetSystemMetrics(SM_CYCURSOR);
        BITMAP cursorBitmap{};
        if (iconInfo.hbmColor != nullptr &&
            ::GetObject(iconInfo.hbmColor, sizeof(cursorBitmap), &cursorBitmap) == sizeof(cursorBitmap))
        {
            w = std::max(1L, cursorBitmap.bmWidth);
            h = std::max(1L, std::abs(cursorBitmap.bmHeight));
        }
        else if (iconInfo.hbmMask != nullptr &&
                 ::GetObject(iconInfo.hbmMask, sizeof(cursorBitmap), &cursorBitmap) == sizeof(cursorBitmap))
        {
            w = std::max(1L, cursorBitmap.bmWidth);
            // A monochrome cursor stores its AND and XOR masks vertically.
            h = std::max(1L, std::abs(cursorBitmap.bmHeight) / 2);
        }

        BITMAPINFO bmi{};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = w;
        bmi.bmiHeader.biHeight = -h;  // top-down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        void* bits = nullptr;
        const HDC hdc = ::CreateCompatibleDC(nullptr);
        const HBITMAP dib = ::CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (dib == nullptr || bits == nullptr)
        {
            if (dib) ::DeleteObject(dib);
            ::DeleteDC(hdc);
            releaseBitmaps();
            return false;
        }
        const auto oldObj = ::SelectObject(hdc, dib);
        std::memset(bits, 0, static_cast<std::size_t>(w) * h * 4);
        ::DrawIconEx(hdc, 0, 0, a_cursor, w, h, 0, nullptr, DI_NORMAL);
        ::GdiFlush();

        // Monochrome cursors (e.g. I-beam) draw with zero alpha — detect and
        // derive alpha from the AND mask in a second pass.
        auto* pixels = static_cast<std::uint32_t*>(bits);
        bool hasAlpha = false;
        for (int i = 0; i < w * h; ++i)
        {
            if ((pixels[i] & 0xFF000000u) != 0)
            {
                hasAlpha = true;
                break;
            }
        }
        if (!hasAlpha)
        {
            std::vector<std::uint32_t> maskBits(static_cast<std::size_t>(w) * h);
            std::memset(bits, 0, static_cast<std::size_t>(w) * h * 4);
            ::DrawIconEx(hdc, 0, 0, a_cursor, w, h, 0, nullptr, DI_MASK);
            ::GdiFlush();
            std::memcpy(maskBits.data(), bits, maskBits.size() * 4);
            std::memset(bits, 0, static_cast<std::size_t>(w) * h * 4);
            ::DrawIconEx(hdc, 0, 0, a_cursor, w, h, 0, nullptr, DI_IMAGE);
            ::GdiFlush();
            for (int i = 0; i < w * h; ++i)
            {
                // AND mask black (0) = opaque; the DI_IMAGE pixel already in
                // `pixels[i]` is the source color, just needs its alpha set.
                // AND mask white (1) is the classic invert region (e.g. the
                // I-beam's caret): decode via the DI_IMAGE (XOR) pixel drawn
                // above — non-zero RGB there means the cursor draws opaque
                // white; zero means fully transparent, so it doesn't vanish
                // over dark pages.
                const bool andOpaque = (maskBits[static_cast<std::size_t>(i)] & 0x00FFFFFFu) == 0;
                if (andOpaque)
                {
                    pixels[i] |= 0xFF000000u;
                }
                else
                {
                    pixels[i] = ((pixels[i] & 0x00FFFFFFu) != 0) ? 0xFFFFFFFFu : 0u;
                }
            }
        }

        D3D11_TEXTURE2D_DESC texDesc{};
        texDesc.Width = static_cast<UINT>(w);
        texDesc.Height = static_cast<UINT>(h);
        texDesc.MipLevels = 1;
        texDesc.ArraySize = 1;
        texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        texDesc.SampleDesc.Count = 1;
        texDesc.Usage = D3D11_USAGE_IMMUTABLE;
        texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA init{};
        init.pSysMem = bits;
        init.SysMemPitch = static_cast<UINT>(w) * 4;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
        bool ok = SUCCEEDED(a_device->CreateTexture2D(&texDesc, &init, texture.GetAddressOf()));
        if (ok)
        {
            ok = SUCCEEDED(a_device->CreateShaderResourceView(texture.Get(), nullptr, a_out.srv.GetAddressOf()));
        }

        ::SelectObject(hdc, oldObj);
        ::DeleteObject(dib);
        ::DeleteDC(hdc);

        a_out.width = w;
        a_out.height = h;
        a_out.hotspotX = static_cast<int>(iconInfo.xHotspot);
        a_out.hotspotY = static_cast<int>(iconInfo.yHotspot);
        releaseBitmaps();
        return ok;
    }
}
