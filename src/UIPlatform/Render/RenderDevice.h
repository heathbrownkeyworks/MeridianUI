#pragma once

// NOMINMAX before any Windows header: the NO_PCH test build otherwise inherits min/max macros that break <algorithm>.
#define NOMINMAX

#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <cstdint>
#include <mutex>

namespace Meridian::Render
{
    /// <summary>
    /// The platform's private D3D11 device, created on the same adapter as the
    /// game's device. All frame copies run here so the game's immediate context
    /// is never touched from CEF threads and the game render thread never waits
    /// on platform GPU work.
    /// </summary>
    class RenderDevice
    {
    protected:
        Microsoft::WRL::ComPtr<ID3D11Device1> m_device = nullptr;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context = nullptr;
        std::mutex m_contextMutex;
        bool m_sharedKeyedTransportSupported = false;

        bool ProbeSharedKeyedTransport(ID3D11Device* a_gameDevice);

    public:
        /// <summary>
        /// Creates the device on a_gameDevice's adapter. Returns false on any
        /// failure (caller falls back to the SyncCopy renderer).
        /// </summary>
        bool Create(ID3D11Device* a_gameDevice);

        bool IsValid() const { return m_device != nullptr && m_context != nullptr; }
        bool SupportsSharedKeyedTransport() const { return m_sharedKeyedTransportSupported; }
        ID3D11Device1* Device() const { return m_device.Get(); }
        ID3D11DeviceContext* Context() const { return m_context.Get(); }
        std::mutex& ContextMutex() { return m_contextMutex; }
    };
}
