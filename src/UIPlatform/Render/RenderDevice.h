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
    /// Private D3D11 device for shared-texture producers, or an isolated deferred
    /// context on the game device for DXVK NIF previews. CEF threads only use
    /// the private device. Deferred submission is restricted to the render thread.
    /// </summary>
    class RenderDevice
    {
    protected:
        Microsoft::WRL::ComPtr<ID3D11Device1> m_device = nullptr;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context = nullptr;
        std::mutex m_contextMutex;
        bool m_sharedKeyedTransportSupported = false;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_submissionContext;

        bool ProbeSharedKeyedTransport(ID3D11Device* a_gameDevice);

    public:
        /// <summary>
        /// Creates the device on a_gameDevice's adapter. Returns false on any
        /// failure (caller falls back to the SyncCopy renderer).
        /// </summary>
        bool Create(ID3D11Device* a_gameDevice);

        // NIF previews on Windows DXVK record into an isolated deferred context
        // on the game device. No shared handles or CPU readback are involved.
        bool CreateDeferred(ID3D11Device* a_gameDevice);
        // Call on the game render thread with ContextMutex held. Submits the
        // recorded pass and restores all immediate-context pipeline state.
        HRESULT SubmitDeferredFrame();
        bool IsDeferred() const { return m_submissionContext != nullptr; }

        bool IsValid() const { return m_device != nullptr && m_context != nullptr; }
        bool SupportsSharedKeyedTransport() const { return m_sharedKeyedTransportSupported; }
        ID3D11Device1* Device() const { return m_device.Get(); }
        ID3D11DeviceContext* Context() const { return m_context.Get(); }
        std::mutex& ContextMutex() { return m_contextMutex; }
    };
}
