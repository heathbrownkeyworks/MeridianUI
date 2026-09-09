#include "CpuTextureSurface.h"

namespace Meridian::Render
{
    HRESULT CpuTextureSurface::UploadLatest(ID3D11Device* device, ID3D11DeviceContext* context)
    {
        if (!device || !context) return E_POINTER;
        auto update = m_frames.TakeUpdate();
        if (!update) return S_FALSE;
        auto hr = device->GetDeviceRemovedReason();
        if (FAILED(hr)) { m_frames.RequestFullUpload(); return hr; }
        if (!m_texture || m_width != update->width || m_height != update->height)
        {
            D3D11_TEXTURE2D_DESC desc{};
            desc.Width = update->width;
            desc.Height = update->height;
            desc.MipLevels = desc.ArraySize = 1;
            desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            desc.SampleDesc.Count = 1;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
            Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
            hr = device->CreateTexture2D(&desc, nullptr, texture.GetAddressOf());
            if (SUCCEEDED(hr)) hr = device->CreateShaderResourceView(texture.Get(), nullptr, view.GetAddressOf());
            if (FAILED(hr)) { m_frames.RequestFullUpload(); return hr; }
            m_texture = std::move(texture);
            m_view = std::move(view);
            m_width = update->width;
            m_height = update->height;
        }
        const auto& rect = update->dirty;
        const D3D11_BOX box{UINT(rect.x), UINT(rect.y), 0, UINT(rect.x + rect.width), UINT(rect.y + rect.height), 1};
        context->UpdateSubresource(m_texture.Get(), 0, &box, update->pixels.data(), UINT(rect.width * 4), 0);
        m_epoch = update->epoch;
        return S_OK;
    }
}
