#pragma once

#include <algorithm>

namespace Meridian::Render
{
    // Internal policy. Public RendererType and Settings ABI remain unchanged.
    enum class BrowserTransport { Auto, SharedTexture, CpuUpload };

    [[nodiscard]] constexpr BrowserTransport ResolveBrowserTransport(
        BrowserTransport requested, bool dxvkDevice, bool wine) noexcept
    {
        return requested == BrowserTransport::Auto ?
            (dxvkDevice && !wine ? BrowserTransport::CpuUpload : BrowserTransport::SharedTexture) : requested;
    }

    [[nodiscard]] constexpr const char* ToString(BrowserTransport transport) noexcept
    {
        switch (transport)
        {
        case BrowserTransport::CpuUpload: return "CpuUpload";
        case BrowserTransport::SharedTexture: return "SharedTexture";
        default: return "Auto";
        }
    }

    [[nodiscard]] constexpr int BrowserFrameRate(BrowserTransport transport, int requested, int cpuLimit) noexcept
    {
        return transport == BrowserTransport::CpuUpload ? std::min(requested, cpuLimit) : requested;
    }
}
