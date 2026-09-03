#pragma once

// NOMINMAX before any Windows header: the NO_PCH test build otherwise inherits min/max macros that break <algorithm>.
#define NOMINMAX

#include "Menus/CompositorMath.h"
#include "Render/LogThrottle.h"

#include <d3d11_1.h>
#include <wrl/client.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>

namespace Meridian::Render
{
    class RenderDevice;

    /// <summary>
    /// Triple-buffered cross-device frame ring. Producer (CEF UI thread) copies
    /// a platform-device texture into the next free slot, waits for that copy on
    /// the PLATFORM device, then publishes it through a keyed mutex. Consumer
    /// (game render thread) acquires the published slot, returns its game-device
    /// SRV, and releases GPU ownership after the draw is submitted. The internal
    /// mutex guards index bookkeeping and ring rebuilds only; the consumer never
    /// holds it across GPU work.
    /// </summary>
    class FrameTransport
    {
    protected:
        struct Slot
        {
            Microsoft::WRL::ComPtr<ID3D11Texture2D> platformTexture;
            Microsoft::WRL::ComPtr<ID3D11Texture2D> gameTexture;
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> gameSRV;
            Microsoft::WRL::ComPtr<IDXGIKeyedMutex> platformMutex;
            Microsoft::WRL::ComPtr<IDXGIKeyedMutex> gameMutex;
            std::atomic_bool awaitingGameAcquire{false};
        };

        static constexpr int kSlotCount = 3;

        RenderDevice* m_platformDevice = nullptr;
        Microsoft::WRL::ComPtr<ID3D11Device1> m_gameDevice1 = nullptr;
        Slot m_slots[kSlotCount];
        Menus::CompositorMath::RingState m_ring{};
        std::mutex m_ringMutex;
        std::atomic<int> m_width{0};
        std::atomic<int> m_height{0};
        int m_gameOwnedSlot = -1;
        std::atomic_bool m_rebuildRequested{false};
        std::atomic_bool m_stopRequested{false};
        std::atomic<int> m_droppedFrames{0};
        LogThrottle m_gpuWaitThrottle;
        std::atomic_bool m_deviceRemovedLogged{false};

        // Runs under m_ringMutex, including driver resource creation. A resize
        // rebuild briefly blocks ConsumeSRV, but the work is bounded and only
        // occurs when dimensions change.
        bool BuildRingLocked(int a_width, int a_height);
        bool WaitForPlatformGpuIdle();
        // Logs the device-removed reason once per transport lifetime, on the
        // GPU-wait failure paths. No-op when the device hasn't been removed.
        void ReportDeviceRemovedIfAny();

    public:
        bool Initialize(RenderDevice& a_platformDevice, ID3D11Device* a_gameDevice, int a_width, int a_height);
        bool ProduceFrame(ID3D11Texture2D* a_sourceOnPlatformDevice);
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ConsumeSRV();
        void ReleaseConsumedFrame();
        void RequestResize(int a_width, int a_height);
        void RequestStop();

        int Width() const { return m_width.load(std::memory_order_acquire); }
        int Height() const { return m_height.load(std::memory_order_acquire); }
    };
}
