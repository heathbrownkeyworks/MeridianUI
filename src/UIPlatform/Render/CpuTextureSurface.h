#pragma once

#include "CpuFrameBuffer.h"
#include <d3d11.h>
#include <wrl/client.h>

namespace Meridian::Render
{
    class CpuTextureSurface
    {
    public:
        CpuFrameBuffer& Frames() { return m_frames; }
        // Call only on the game render thread. No immediate-context state is changed.
        HRESULT UploadLatest(ID3D11Device* device, ID3D11DeviceContext* context);
        ID3D11ShaderResourceView* View() const { return m_frames.IsCurrent(m_epoch) ? m_view.Get() : nullptr; }

    private:
        CpuFrameBuffer m_frames;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> m_texture;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_view;
        int m_width = 0, m_height = 0;
        std::uint64_t m_epoch = 0;
    };
}
