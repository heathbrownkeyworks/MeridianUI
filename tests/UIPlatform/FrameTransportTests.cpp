#define NOMINMAX
#include <windows.h>

#include "Render/FrameConsumerSync.h"
#include "Render/RenderDevice.h"
#include "Render/FrameTransport.h"

#include <d3d11_1.h>
#include <wrl/client.h>

#include <cstdint>
#include <cstring>
#include <atomic>
#include <iostream>
#include <thread>
#include <vector>

namespace
{
    using Microsoft::WRL::ComPtr;

    int g_failureCount = 0;

    void Expect(bool a_condition, const char* a_message)
    {
        if (!a_condition)
        {
            ++g_failureCount;
            std::cerr << "FAILED: " << a_message << '\n';
        }
    }

    ComPtr<ID3D11Device> CreateBareDevice()
    {
        ComPtr<ID3D11Device> device;
        const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0};
        const auto hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                                          D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                          levels, 1, D3D11_SDK_VERSION,
                                          device.GetAddressOf(), nullptr, nullptr);
        if (FAILED(hr))
        {
            std::cerr << "FATAL: no hardware D3D11 device available (hr=" << std::hex << hr << ")\n";
        }
        return device;
    }

    // Fills a platform-device texture with a solid BGRA color via UpdateSubresource.
    ComPtr<ID3D11Texture2D> MakeSourceTexture(Meridian::Render::RenderDevice& a_platform,
                                              int a_width, int a_height, std::uint32_t a_bgra)
    {
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = a_width;
        desc.Height = a_height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        ComPtr<ID3D11Texture2D> texture;
        if (FAILED(a_platform.Device()->CreateTexture2D(&desc, nullptr, texture.GetAddressOf())))
        {
            return nullptr;
        }

        std::vector<std::uint32_t> pixels(static_cast<size_t>(a_width) * a_height, a_bgra);
        a_platform.Context()->UpdateSubresource(texture.Get(), 0, nullptr, pixels.data(),
                                                a_width * sizeof(std::uint32_t), 0);
        return texture;
    }

    // Reads back the texture behind a game-device SRV, returns the first pixel.
    std::uint32_t ReadFirstPixelOnGameDevice(ID3D11Device* a_gameDevice, ID3D11ShaderResourceView* a_srv)
    {
        ComPtr<ID3D11Resource> resource;
        a_srv->GetResource(resource.GetAddressOf());
        ComPtr<ID3D11Texture2D> texture;
        resource.As(&texture);

        D3D11_TEXTURE2D_DESC desc = {};
        texture->GetDesc(&desc);
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        desc.MiscFlags = 0;

        ComPtr<ID3D11Texture2D> staging;
        if (FAILED(a_gameDevice->CreateTexture2D(&desc, nullptr, staging.GetAddressOf())))
        {
            return 0;
        }

        ComPtr<ID3D11DeviceContext> context;
        a_gameDevice->GetImmediateContext(context.GetAddressOf());
        context->CopyResource(staging.Get(), texture.Get());

        D3D11_MAPPED_SUBRESOURCE mapped = {};
        if (FAILED(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
        {
            return 0;
        }
        std::uint32_t pixel = 0;
        std::memcpy(&pixel, mapped.pData, sizeof(pixel));
        context->Unmap(staging.Get(), 0);
        return pixel;
    }

    void TestDeviceCreation(ID3D11Device* a_gameDevice)
    {
        Meridian::Render::RenderDevice platform;
        Expect(platform.Create(a_gameDevice), "platform device creates on the game adapter");
        Expect(platform.IsValid(), "platform device reports valid");
        Expect(platform.Device() != nullptr && platform.Context() != nullptr, "device and context are non-null");
    }

    void TestConsumerAcquirePlan()
    {
        const auto newFrame = Meridian::Render::PlanConsumerAcquire(true);
        Expect(newFrame.acquireKey == 1, "newly published frame is acquired with key 1");
        Expect(newFrame.clearsAwaitingGameAcquire,
               "newly published frame clears the producer-to-consumer handoff flag");

        const auto retainedFrame = Meridian::Render::PlanConsumerAcquire(false);
        Expect(retainedFrame.acquireKey == 0, "retained frame is reacquired with key 0");
        Expect(!retainedFrame.clearsAwaitingGameAcquire,
               "retained frame leaves the producer-to-consumer handoff flag clear");
    }

    void TestRoundTrip(ID3D11Device* a_gameDevice)
    {
        Meridian::Render::RenderDevice platform;
        if (!platform.Create(a_gameDevice))
        {
            Expect(false, "platform device required for round-trip test");
            return;
        }

        Meridian::Render::FrameTransport transport;
        Expect(transport.Initialize(platform, a_gameDevice, 64, 64), "transport initializes a 64x64 ring");

        Expect(transport.ConsumeSRV() == nullptr, "nothing published yet yields null SRV");

        constexpr std::uint32_t kRed = 0xFFFF0000; // B8G8R8A8: A=FF R=FF
        const auto source = MakeSourceTexture(platform, 64, 64, kRed);
        Expect(source != nullptr, "source texture creates on the platform device");

        Expect(transport.ProduceFrame(source.Get()), "produce succeeds");

        const auto srv = transport.ConsumeSRV();
        Expect(srv != nullptr, "published frame yields an SRV on the game device");
        if (srv != nullptr)
        {
            Expect(ReadFirstPixelOnGameDevice(a_gameDevice, srv.Get()) == kRed,
                   "game device reads back the exact pixel written on the platform device");
        }
        transport.ReleaseConsumedFrame();
    }

    void TestSlotRotationAndFreshness(ID3D11Device* a_gameDevice)
    {
        Meridian::Render::RenderDevice platform;
        platform.Create(a_gameDevice);
        Meridian::Render::FrameTransport transport;
        transport.Initialize(platform, a_gameDevice, 32, 32);

        const std::uint32_t colors[] = {0xFF0000FF, 0xFF00FF00, 0xFFFF0000, 0xFFFFFFFF};
        ComPtr<ID3D11ShaderResourceView> lastSrv;
        for (const auto color : colors)
        {
            const auto source = MakeSourceTexture(platform, 32, 32, color);
            Expect(transport.ProduceFrame(source.Get()), "each produce succeeds during rotation");
            const auto srv = transport.ConsumeSRV();
            Expect(srv != nullptr, "each consume yields an SRV");
            if (srv != nullptr)
            {
                Expect(ReadFirstPixelOnGameDevice(a_gameDevice, srv.Get()) == color,
                       "consumer always sees the most recently published frame");
                Expect(srv != lastSrv, "consecutive frames come from different ring slots");
                lastSrv = srv;
            }
            transport.ReleaseConsumedFrame();
        }
    }

    void TestResizeRebuild(ID3D11Device* a_gameDevice)
    {
        Meridian::Render::RenderDevice platform;
        platform.Create(a_gameDevice);
        Meridian::Render::FrameTransport transport;
        transport.Initialize(platform, a_gameDevice, 32, 32);

        const auto smallTex = MakeSourceTexture(platform, 32, 32, 0xFF112233);
        transport.ProduceFrame(smallTex.Get());
        const auto smallSrv = transport.ConsumeSRV();
        Expect(smallSrv != nullptr, "pre-resize frame is consumable");
        transport.ReleaseConsumedFrame();

        // A size-mismatched source triggers an automatic rebuild at its size.
        const auto largeTex = MakeSourceTexture(platform, 128, 96, 0xFFAABBCC);
        Expect(transport.ProduceFrame(largeTex.Get()), "size-mismatched produce rebuilds and succeeds");
        Expect(transport.Width() == 128 && transport.Height() == 96, "ring took the new dimensions");

        const auto srv = transport.ConsumeSRV();
        Expect(srv != nullptr, "post-rebuild frame is consumable");
        if (srv != nullptr)
        {
            Expect(ReadFirstPixelOnGameDevice(a_gameDevice, srv.Get()) == 0xFFAABBCC,
                   "post-rebuild content is the new frame");
        }
        transport.ReleaseConsumedFrame();
    }

    void TestProducerReclaimsSkippedFrames(ID3D11Device* a_gameDevice)
    {
        Meridian::Render::RenderDevice platform;
        platform.Create(a_gameDevice);
        Meridian::Render::FrameTransport transport;
        transport.Initialize(platform, a_gameDevice, 16, 16);

        for (std::uint32_t i = 0; i < 8; ++i)
        {
            const auto source = MakeSourceTexture(platform, 16, 16, 0xFF000000u | i);
            Expect(transport.ProduceFrame(source.Get()), "producer reclaims a published frame that was never consumed");
        }
        const auto srv = transport.ConsumeSRV();
        Expect(srv != nullptr && ReadFirstPixelOnGameDevice(a_gameDevice, srv.Get()) == 0xFF000007u,
               "consumer receives the newest frame after skipped intermediates");
        transport.ReleaseConsumedFrame();
    }

    void TestRetainedFrameCanBeConsumedRepeatedly(ID3D11Device* a_gameDevice)
    {
        Meridian::Render::RenderDevice platform;
        platform.Create(a_gameDevice);
        Meridian::Render::FrameTransport transport;
        transport.Initialize(platform, a_gameDevice, 24, 24);

        constexpr std::uint32_t kColor = 0xFF4269A5;
        const auto source = MakeSourceTexture(platform, 24, 24, kColor);
        Expect(transport.ProduceFrame(source.Get()), "retained-frame source is produced once");

        for (int present = 0; present < 8; ++present)
        {
            const auto srv = transport.ConsumeSRV();
            Expect(srv != nullptr, "retained frame remains consumable on every present");
            if (srv != nullptr)
            {
                Expect(ReadFirstPixelOnGameDevice(a_gameDevice, srv.Get()) == kColor,
                       "retained frame preserves its pixels across repeated presents");
            }
            transport.ReleaseConsumedFrame();
        }
    }

    void TestConcurrentDimensionAccess(ID3D11Device* a_gameDevice)
    {
        Meridian::Render::RenderDevice platform;
        platform.Create(a_gameDevice);
        Meridian::Render::FrameTransport transport;
        transport.Initialize(platform, a_gameDevice, 32, 32);

        std::atomic_bool start{false};
        std::thread requester([&]() {
            while (!start.load(std::memory_order_acquire)) {}
            for (int i = 0; i < 10000; ++i)
            {
                transport.RequestResize(64 + (i & 1), 64 + (i & 1));
            }
        });
        start.store(true, std::memory_order_release);
        for (int i = 0; i < 10000; ++i)
        {
            Expect(transport.Width() > 0 && transport.Height() > 0,
                   "dimension reads remain valid during resize requests");
        }
        requester.join();
    }
}

int main()
{
    const auto gameDevice = CreateBareDevice();
    if (gameDevice == nullptr)
    {
        std::cerr << "SKIP-AS-FAILURE: this test requires a hardware GPU\n";
        return 1;
    }

    TestDeviceCreation(gameDevice.Get());
    TestConsumerAcquirePlan();
    TestRoundTrip(gameDevice.Get());
    TestSlotRotationAndFreshness(gameDevice.Get());
    TestResizeRebuild(gameDevice.Get());
    TestProducerReclaimsSkippedFrames(gameDevice.Get());
    TestRetainedFrameCanBeConsumedRepeatedly(gameDevice.Get());
    TestConcurrentDimensionAccess(gameDevice.Get());

    if (g_failureCount != 0)
    {
        std::cerr << g_failureCount << " FrameTransport test(s) failed\n";
        return 1;
    }

    std::cout << "All FrameTransport tests passed\n";
    return 0;
}
