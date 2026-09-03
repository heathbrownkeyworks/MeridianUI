#pragma once

#include "MeridianUIAPI/Settings.h"

namespace Meridian::Render
{
    /// Selects a browser renderer only after the actual game adapter has been
    /// checked. A private D3D device by itself is not enough: the shared
    /// keyed-texture path must work end-to-end or the ring produces an
    /// invisible browser surface.
    [[nodiscard]] constexpr Meridian::UI::RendererType ResolveBrowserRenderer(
        Meridian::UI::RendererType a_requested,
        bool a_platformDeviceAvailable,
        bool a_sharedKeyedTransportSupported) noexcept
    {
        if (a_requested == Meridian::UI::RendererType::RingBuffer &&
            (!a_platformDeviceAvailable || !a_sharedKeyedTransportSupported))
        {
            return Meridian::UI::RendererType::SyncCopy;
        }
        return a_requested;
    }
}
