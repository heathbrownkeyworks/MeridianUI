#pragma once

#include "Common/SpinLock.h"
#include "RenderDevice.h"
#include <directxtk/CommonStates.h>
#include <directxtk/SimpleMath.h>
#include <directxtk/SpriteBatch.h>

#include <memory>

namespace Meridian::Render
{
    struct RenderData
    {
        ID3D11Device* device = nullptr;
        ID3D11DeviceContext3* deviceContext = nullptr;
        std::shared_ptr<::DirectX::CommonStates> commonStates = nullptr;
        std::shared_ptr<::DirectX::SpriteBatch> spriteBatch = nullptr;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        Common::SpinLock drawLock;
        std::shared_ptr<RenderDevice> platformDevice = nullptr;
    };
}
