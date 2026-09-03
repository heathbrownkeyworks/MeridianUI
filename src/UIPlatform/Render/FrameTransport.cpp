#include "FrameTransport.h"

#include "FrameTransportLog.h"
#include "RenderDevice.h"

#include <dxgi1_2.h>

#include <thread>

namespace Meridian::Render
{
    bool FrameTransport::Initialize(RenderDevice& a_platformDevice, ID3D11Device* a_gameDevice, int a_width, int a_height)
    {
        if (!a_platformDevice.IsValid() || a_gameDevice == nullptr || a_width <= 0 || a_height <= 0)
        {
            MERIDIAN_FT_LOG_ERROR("FrameTransport::Initialize: invalid arguments");
            return false;
        }

        m_platformDevice = &a_platformDevice;

        const auto hr = a_gameDevice->QueryInterface(IID_PPV_ARGS(m_gameDevice1.GetAddressOf()));
        if (FAILED(hr))
        {
            MERIDIAN_FT_LOG_ERROR("FrameTransport::Initialize: game ID3D11Device1 unavailable");
            return false;
        }

        std::lock_guard lock(m_ringMutex);
        return BuildRingLocked(a_width, a_height);
    }

    bool FrameTransport::BuildRingLocked(int a_width, int a_height)
    {
        if (m_gameOwnedSlot >= 0)
        {
            return false;
        }
        for (auto& slot : m_slots)
        {
            slot.platformMutex.Reset();
            slot.gameMutex.Reset();
            slot.gameSRV.Reset();
            slot.gameTexture.Reset();
            slot.platformTexture.Reset();
            slot.awaitingGameAcquire.store(false, std::memory_order_release);
        }
        m_ring = Menus::CompositorMath::RingState{};

        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = static_cast<UINT>(a_width);
        desc.Height = static_cast<UINT>(a_height);
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

        for (auto& slot : m_slots)
        {
            auto hr = m_platformDevice->Device()->CreateTexture2D(&desc, nullptr, slot.platformTexture.ReleaseAndGetAddressOf());
            if (FAILED(hr))
            {
                MERIDIAN_FT_LOG_ERROR("FrameTransport: slot texture creation failed ({:#010x})", static_cast<std::uint32_t>(hr));
                return false;
            }

            hr = slot.platformTexture.As(&slot.platformMutex);
            if (FAILED(hr))
            {
                MERIDIAN_FT_LOG_ERROR("FrameTransport: platform keyed mutex unavailable on slot ({:#010x})", static_cast<std::uint32_t>(hr));
                return false;
            }

            Microsoft::WRL::ComPtr<IDXGIResource1> dxgiResource;
            hr = slot.platformTexture.As(&dxgiResource);
            if (FAILED(hr))
            {
                MERIDIAN_FT_LOG_ERROR("FrameTransport: IDXGIResource1 unavailable on slot ({:#010x})", static_cast<std::uint32_t>(hr));
                return false;
            }

            HANDLE sharedHandle = nullptr;
            hr = dxgiResource->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr, &sharedHandle);
            if (FAILED(hr))
            {
                MERIDIAN_FT_LOG_ERROR("FrameTransport: CreateSharedHandle failed ({:#010x})", static_cast<std::uint32_t>(hr));
                return false;
            }

            hr = m_gameDevice1->OpenSharedResource1(sharedHandle, IID_PPV_ARGS(slot.gameTexture.ReleaseAndGetAddressOf()));
            ::CloseHandle(sharedHandle);
            if (FAILED(hr))
            {
                MERIDIAN_FT_LOG_ERROR("FrameTransport: OpenSharedResource1 on game device failed ({:#010x})", static_cast<std::uint32_t>(hr));
                return false;
            }

            hr = slot.gameTexture.As(&slot.gameMutex);
            if (FAILED(hr))
            {
                MERIDIAN_FT_LOG_ERROR("FrameTransport: game keyed mutex unavailable on slot ({:#010x})", static_cast<std::uint32_t>(hr));
                return false;
            }

            hr = m_gameDevice1->CreateShaderResourceView(slot.gameTexture.Get(), nullptr, slot.gameSRV.ReleaseAndGetAddressOf());
            if (FAILED(hr))
            {
                MERIDIAN_FT_LOG_ERROR("FrameTransport: game-device SRV creation failed ({:#010x})", static_cast<std::uint32_t>(hr));
                return false;
            }
        }

        m_width.store(a_width, std::memory_order_release);
        m_height.store(a_height, std::memory_order_release);
        m_rebuildRequested.store(false, std::memory_order_release);
        return true;
    }

    void FrameTransport::ReportDeviceRemovedIfAny()
    {
        if (m_deviceRemovedLogged.load(std::memory_order_acquire))
        {
            return;
        }

        const auto removedReason = m_platformDevice->Device()->GetDeviceRemovedReason();
        if (FAILED(removedReason) && !m_deviceRemovedLogged.exchange(true, std::memory_order_acq_rel))
        {
            MERIDIAN_FT_LOG_ERROR("FrameTransport: DEVICE REMOVED ({:X}) — platform rendering is dead until restart", removedReason);
        }
    }

    bool FrameTransport::WaitForPlatformGpuIdle()
    {
        D3D11_QUERY_DESC queryDesc = {};
        queryDesc.Query = D3D11_QUERY_EVENT;

        Microsoft::WRL::ComPtr<ID3D11Query> query;
        auto hr = m_platformDevice->Device()->CreateQuery(&queryDesc, query.GetAddressOf());
        if (FAILED(hr))
        {
            MERIDIAN_FT_LOG_ERROR("FrameTransport: CreateQuery failed, code {:X}", hr);
            ReportDeviceRemovedIfAny();
            return false;
        }

        m_platformDevice->Context()->End(query.Get());
        m_platformDevice->Context()->Flush();

        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
        while (m_platformDevice->Context()->GetData(query.Get(), nullptr, 0, 0) == S_FALSE)
        {
            if (m_stopRequested.load(std::memory_order_acquire))
            {
                return false;
            }
            if (std::chrono::steady_clock::now() >= deadline)
            {
                std::uint32_t suppressed = 0;
                if (m_gpuWaitThrottle.ShouldLog(suppressed))
                {
                    // "+1" anticipates the m_droppedFrames.fetch_add(1) that ProduceFrame
                    // performs after this function returns false — keep this in sync if
                    // that increment is ever reordered ahead of/away from this call.
                    MERIDIAN_FT_LOG_ERROR(
                        "FrameTransport: platform GPU copy wait timed out ({} more suppressed in the last window, {} dropped frames total)",
                        suppressed, m_droppedFrames.load(std::memory_order_relaxed) + 1);
                }
                ReportDeviceRemovedIfAny();
                return false;
            }
            std::this_thread::yield();
        }
        return true;
    }

    bool FrameTransport::ProduceFrame(ID3D11Texture2D* a_sourceOnPlatformDevice)
    {
        if (a_sourceOnPlatformDevice == nullptr || m_platformDevice == nullptr ||
            m_stopRequested.load(std::memory_order_acquire))
        {
            return false;
        }

        D3D11_TEXTURE2D_DESC sourceDesc = {};
        a_sourceOnPlatformDevice->GetDesc(&sourceDesc);

        int writeSlot = -1;
        {
            std::lock_guard lock(m_ringMutex);
            if (m_rebuildRequested.load(std::memory_order_acquire) ||
                static_cast<int>(sourceDesc.Width) != m_width.load(std::memory_order_acquire) ||
                static_cast<int>(sourceDesc.Height) != m_height.load(std::memory_order_acquire))
            {
                if (!BuildRingLocked(static_cast<int>(sourceDesc.Width), static_cast<int>(sourceDesc.Height)))
                {
                    m_droppedFrames.fetch_add(1, std::memory_order_relaxed);
                    return false;
                }
            }
            writeSlot = m_ring.writeSlot;
        }

        auto& slot = m_slots[writeSlot];
        if (slot.awaitingGameAcquire.load(std::memory_order_acquire))
        {
            const auto reclaimResult = slot.platformMutex->AcquireSync(1, 0);
            if (reclaimResult != S_OK)
            {
                m_droppedFrames.fetch_add(1, std::memory_order_relaxed);
                return false;
            }
            slot.platformMutex->ReleaseSync(0);
            slot.awaitingGameAcquire.store(false, std::memory_order_release);
        }

        if (slot.platformMutex->AcquireSync(0, 0) != S_OK)
        {
            m_droppedFrames.fetch_add(1, std::memory_order_relaxed);
            return false;
        }

        {
            // The private immediate context is shared by every CEF transport
            // and native producer. Keep each copy/query sequence atomic so a
            // Present-thread NIF draw cannot interleave with CEF paint work.
            std::lock_guard contextLock(m_platformDevice->ContextMutex());
            m_platformDevice->Context()->CopyResource(
                slot.platformTexture.Get(), a_sourceOnPlatformDevice);

            if (!WaitForPlatformGpuIdle())
            {
                slot.platformMutex->ReleaseSync(0);
                m_droppedFrames.fetch_add(1, std::memory_order_relaxed);
                return false;
            }
        }

        if (slot.platformMutex->ReleaseSync(1) != S_OK)
        {
            m_droppedFrames.fetch_add(1, std::memory_order_relaxed);
            return false;
        }

        {
            std::lock_guard lock(m_ringMutex);
            slot.awaitingGameAcquire.store(true, std::memory_order_release);
            Menus::CompositorMath::Publish(m_ring, kSlotCount);
        }
        return true;
    }

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> FrameTransport::ConsumeSRV()
    {
        std::lock_guard lock(m_ringMutex);
        if (m_ring.publishedSlot < 0)
        {
            return nullptr;
        }
        auto& slot = m_slots[m_ring.publishedSlot];
        if (slot.awaitingGameAcquire.load(std::memory_order_acquire))
        {
            if (slot.gameMutex->AcquireSync(1, 0) != S_OK)
            {
                return nullptr;
            }
            slot.awaitingGameAcquire.store(false, std::memory_order_release);
            m_gameOwnedSlot = m_ring.publishedSlot;
        }
        m_ring.readerSlot = m_ring.publishedSlot;
        return slot.gameSRV;
    }

    void FrameTransport::ReleaseConsumedFrame()
    {
        std::lock_guard lock(m_ringMutex);
        if (m_gameOwnedSlot < 0)
        {
            return;
        }
        auto& slot = m_slots[m_gameOwnedSlot];
        if (slot.gameMutex->ReleaseSync(0) != S_OK)
        {
            MERIDIAN_FT_LOG_ERROR("FrameTransport: failed to release game-device slot ownership");
        }
        m_gameOwnedSlot = -1;
        m_ring.readerSlot = -1;
    }

    void FrameTransport::RequestResize(int a_width, int a_height)
    {
        if (a_width > 0 && a_height > 0 &&
            (a_width != m_width.load(std::memory_order_acquire) ||
             a_height != m_height.load(std::memory_order_acquire)))
        {
            m_rebuildRequested.store(true, std::memory_order_release);
        }
    }

    void FrameTransport::RequestStop()
    {
        m_stopRequested.store(true, std::memory_order_release);
    }
}
