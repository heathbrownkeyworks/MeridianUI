#include "RenderDevice.h"

#include "FrameTransportLog.h"

namespace Meridian::Render
{
    bool RenderDevice::ProbeSharedKeyedTransport(ID3D11Device* a_gameDevice)
    {
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = 1;
        desc.Height = 1;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE |
                         D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> platformTexture;
        auto hr = m_device->CreateTexture2D(&desc, nullptr, platformTexture.GetAddressOf());
        if (FAILED(hr))
        {
            MERIDIAN_FT_LOG_WARN(
                "RenderDevice: shared keyed-texture probe CreateTexture2D failed ({:#010x})",
                static_cast<std::uint32_t>(hr));
            return false;
        }

        Microsoft::WRL::ComPtr<IDXGIKeyedMutex> platformMutex;
        hr = platformTexture.As(&platformMutex);
        if (FAILED(hr))
        {
            MERIDIAN_FT_LOG_WARN(
                "RenderDevice: shared keyed-texture probe lacks platform mutex ({:#010x})",
                static_cast<std::uint32_t>(hr));
            return false;
        }

        Microsoft::WRL::ComPtr<IDXGIResource1> resource;
        hr = platformTexture.As(&resource);
        if (FAILED(hr))
        {
            MERIDIAN_FT_LOG_WARN(
                "RenderDevice: shared keyed-texture probe lacks IDXGIResource1 ({:#010x})",
                static_cast<std::uint32_t>(hr));
            return false;
        }

        HANDLE sharedHandle = nullptr;
        hr = resource->CreateSharedHandle(
            nullptr,
            DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
            nullptr,
            &sharedHandle);
        if (FAILED(hr))
        {
            MERIDIAN_FT_LOG_WARN(
                "RenderDevice: shared keyed-texture probe CreateSharedHandle failed ({:#010x})",
                static_cast<std::uint32_t>(hr));
            return false;
        }

        Microsoft::WRL::ComPtr<ID3D11Device1> gameDevice1;
        hr = a_gameDevice->QueryInterface(IID_PPV_ARGS(gameDevice1.GetAddressOf()));
        if (FAILED(hr))
        {
            ::CloseHandle(sharedHandle);
            MERIDIAN_FT_LOG_WARN(
                "RenderDevice: shared keyed-texture probe lacks game ID3D11Device1 ({:#010x})",
                static_cast<std::uint32_t>(hr));
            return false;
        }

        Microsoft::WRL::ComPtr<ID3D11Texture2D> gameTexture;
        hr = gameDevice1->OpenSharedResource1(
            sharedHandle,
            IID_PPV_ARGS(gameTexture.GetAddressOf()));
        ::CloseHandle(sharedHandle);
        if (FAILED(hr))
        {
            MERIDIAN_FT_LOG_WARN(
                "RenderDevice: shared keyed-texture probe OpenSharedResource1 failed ({:#010x})",
                static_cast<std::uint32_t>(hr));
            return false;
        }

        Microsoft::WRL::ComPtr<IDXGIKeyedMutex> gameMutex;
        hr = gameTexture.As(&gameMutex);
        if (FAILED(hr))
        {
            MERIDIAN_FT_LOG_WARN(
                "RenderDevice: shared keyed-texture probe lacks game mutex ({:#010x})",
                static_cast<std::uint32_t>(hr));
            return false;
        }

        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> gameSRV;
        hr = gameDevice1->CreateShaderResourceView(gameTexture.Get(), nullptr, gameSRV.GetAddressOf());
        if (FAILED(hr))
        {
            MERIDIAN_FT_LOG_WARN(
                "RenderDevice: shared keyed-texture probe SRV creation failed ({:#010x})",
                static_cast<std::uint32_t>(hr));
            return false;
        }

        MERIDIAN_FT_LOG_INFO("RenderDevice: shared keyed-texture transport probe passed");
        return true;
    }

    bool RenderDevice::Create(ID3D11Device* a_gameDevice)
    {
        if (a_gameDevice == nullptr)
        {
            MERIDIAN_FT_LOG_ERROR("RenderDevice::Create: game device is null");
            return false;
        }

        Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
        auto hr = a_gameDevice->QueryInterface(IID_PPV_ARGS(dxgiDevice.GetAddressOf()));
        if (FAILED(hr))
        {
            MERIDIAN_FT_LOG_ERROR("RenderDevice::Create: QueryInterface(IDXGIDevice) failed");
            return false;
        }

        Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
        hr = dxgiDevice->GetAdapter(adapter.GetAddressOf());
        if (FAILED(hr))
        {
            MERIDIAN_FT_LOG_ERROR("RenderDevice::Create: GetAdapter failed");
            return false;
        }

        const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
        Microsoft::WRL::ComPtr<ID3D11Device> device;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
        hr = D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr,
                               D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                               levels, 2, D3D11_SDK_VERSION,
                               device.GetAddressOf(), nullptr, context.GetAddressOf());
        if (FAILED(hr))
        {
            MERIDIAN_FT_LOG_ERROR("RenderDevice::Create: D3D11CreateDevice failed");
            return false;
        }

        hr = device.As(&m_device);
        if (FAILED(hr))
        {
            MERIDIAN_FT_LOG_ERROR("RenderDevice::Create: ID3D11Device1 unavailable");
            return false;
        }

        m_context = context;
        m_sharedKeyedTransportSupported = ProbeSharedKeyedTransport(a_gameDevice);
        if (!m_sharedKeyedTransportSupported)
        {
            MERIDIAN_FT_LOG_WARN(
                "RenderDevice: RingBuffer browser transport is unsupported on this game adapter; SyncCopy fallback enabled");
        }
        return true;
    }
}
