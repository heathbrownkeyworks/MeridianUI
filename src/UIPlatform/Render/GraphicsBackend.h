#pragma once

#include <unknwn.h>

namespace Meridian::Render
{
    // DXVK's published IDXGIVkInteropDevice IID. Query only; no Vulkan calls or
    // dependency on DXVK's C++ interface layout beyond IUnknown.
    // https://github.com/doitsujin/dxvk/blob/v3.1/src/dxgi/dxgi_interfaces.h
    inline constexpr GUID kDxvkInteropDevice =
        {0xe2ef5fa5, 0xdc21, 0x4af7, {0x90, 0xc4, 0xf6, 0x7e, 0xf6, 0xa0, 0x93, 0x23}};

    [[nodiscard]] inline bool IsDxvkDevice(IUnknown* device) noexcept
    {
        if (device == nullptr) return false;
        IUnknown* interop = nullptr;
        const auto hr = device->QueryInterface(kDxvkInteropDevice, reinterpret_cast<void**>(&interop));
        const bool detected = SUCCEEDED(hr) && interop != nullptr;
        if (interop != nullptr) interop->Release();
        return detected;
    }

    [[nodiscard]] inline bool IsWine() noexcept
    {
        const auto ntdll = ::GetModuleHandleW(L"ntdll.dll");
        return ntdll != nullptr && ::GetProcAddress(ntdll, "wine_get_version") != nullptr;
    }
}
