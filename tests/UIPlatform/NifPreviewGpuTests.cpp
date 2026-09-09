#include "Render/NifPreviewRenderer.h"
#include "Render/GraphicsBackend.h"
#include "Render/SkyrimNifExtractor.h"
#include "Render/SkyrimTextureLoader.h"
#include <d3d11sdklayers.h>
#include <cstring>
#include <iostream>
#include <stdexcept>

using Microsoft::WRL::ComPtr;
using namespace Meridian::Render;
using namespace Meridian::Render::NifPreview;
namespace NifAPI = Meridian::UI::NifView;
void Check(bool passed, const char* message) { if (!passed) throw std::runtime_error(message); }

namespace Meridian::Render::NifPreview
{
    // Exercise the production GPU renderer with decoded geometry. Any accidental
    // use of Skyrim extraction/resource IO fails instead of loading game state.
    ExtractionResult ExtractModelMesh(RE::NiNode*, ID3D11Device*, ID3D11DeviceContext*,
        std::span<const ModelTextureOverride>, const ActorMaterialTintOverride*) { throw std::runtime_error("unexpected Skyrim extraction"); }
    ExtractionResult ExtractLiveActorMesh(RE::NiNode*, std::span<RE::NiAVObject* const>,
        ID3D11Device*, ID3D11DeviceContext*) { throw std::runtime_error("unexpected actor extraction"); }
    TextureReadResult ReadSkyrimTexture(std::string_view) { throw std::runtime_error("unexpected Skyrim texture IO"); }

    struct NifPreviewRendererTestAccess
    {
        static bool Load(NifPreviewRenderer& renderer, RenderData& data, PreviewMesh mesh)
        {
            if (!renderer.InitializeGraphics(data, 64, 64) || !renderer.BuildMeshResources(data.device, std::move(mesh))) return false;
            renderer.m_sceneVisibility = {{1, true}};
            renderer.m_status = NifAPI::Status::Ready;
            return renderer.FrameModel();
        }
        static bool Deferred(const NifPreviewRenderer& renderer)
        {
            return renderer.m_platformDevice && renderer.m_platformDevice->IsDeferred() &&
                renderer.m_platformDevice->Context()->GetType() == D3D11_DEVICE_CONTEXT_DEFERRED &&
                !renderer.m_platformDevice->SupportsSharedKeyedTransport();
        }
    };
}

PreviewMesh Triangle(float opacity = 1.0f)
{
    PreviewMesh mesh;
    for (Float3 position : {Float3{-1, 0, -1}, Float3{1, 0, -1}, Float3{0.2f, 0, 1}})
    {
        PreviewVertex vertex;
        vertex.position = position;
        vertex.normal = {0, -1, 0};
        mesh.vertices.push_back(vertex);
        mesh.bounds.Include(position);
    }
    mesh.indices = {0, 1, 2};
    PreviewDrawRange draw;
    draw.sceneObject = 1;
    draw.indexCount = 3;
    draw.material.tintColor = {1, 0.05f, 0.02f};
    draw.material.opacity = opacity;
    draw.material.alphaBlend = opacity < 1;
    mesh.draws.push_back(draw);
    return mesh;
}

std::vector<std::uint32_t> Readback(RenderData& data, ID3D11ShaderResourceView* view, UINT width, UINT height)
{
    Check(view != nullptr, "preview SRV missing");
    ComPtr<ID3D11Device> owner;
    view->GetDevice(owner.GetAddressOf());
    Check(owner.Get() == data.device, "preview must belong to the game device");
    ComPtr<ID3D11Resource> resource;
    view->GetResource(resource.GetAddressOf());
    ComPtr<ID3D11Texture2D> texture;
    Check(SUCCEEDED(resource.As(&texture)), "preview is a texture");
    D3D11_TEXTURE2D_DESC desc{};
    texture->GetDesc(&desc);
    Check(desc.MiscFlags == 0 && desc.Width == width && desc.Height == height, "non-shared texture dimensions");
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging;
    Check(SUCCEEDED(data.device->CreateTexture2D(&desc, nullptr, staging.GetAddressOf())), "readback texture");
    data.deviceContext->CopyResource(staging.Get(), texture.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    Check(SUCCEEDED(data.deviceContext->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)), "readback map");
    std::vector<std::uint32_t> pixels(std::size_t(width) * height);
    for (UINT row = 0; row < height; ++row)
        std::memcpy(pixels.data() + std::size_t(row) * width,
            static_cast<const char*>(mapped.pData) + std::size_t(row) * mapped.RowPitch, width * 4);
    data.deviceContext->Unmap(staging.Get(), 0);
    return pixels;
}

std::size_t VisiblePixels(const std::vector<std::uint32_t>& pixels)
{
    return std::count_if(pixels.begin(), pixels.end(), [](auto value) { return (value >> 24) != 0; });
}

int main(int argc, char** argv)
{
    try
    {
        const bool expectDxvk = argc > 1 && std::string_view(argv[1]) == "--expect-dxvk";
        ComPtr<ID3D11Device> device;
        ComPtr<ID3D11DeviceContext> context;
        const auto driver = expectDxvk ? D3D_DRIVER_TYPE_HARDWARE : D3D_DRIVER_TYPE_WARP;
        auto hr = D3D11CreateDevice(nullptr, driver, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_DEBUG,
            nullptr, 0, D3D11_SDK_VERSION, device.GetAddressOf(), nullptr, context.GetAddressOf());
        if (hr == DXGI_ERROR_SDK_COMPONENT_MISSING)
            hr = D3D11CreateDevice(nullptr, driver, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                nullptr, 0, D3D11_SDK_VERSION, device.GetAddressOf(), nullptr, context.GetAddressOf());
        Check(SUCCEEDED(hr), "game device creation");
        Check(IsDxvkDevice(device.Get()) == expectDxvk, "actual DXVK device detection");
        ComPtr<ID3D11DeviceContext3> context3;
        Check(SUCCEEDED(context.As(&context3)), "context3");
        RenderData data;
        data.device = device.Get();
        data.deviceContext = context3.Get();
        data.gameDeviceNifRendering = true; // Exercise the same path on WARP too.
        Check(!data.platformDevice, "DXVK preview needs no separate shared device");
        RenderDevice invalid;
        Check(!invalid.CreateDeferred(nullptr) && invalid.SubmitDeferredFrame() == E_UNEXPECTED, "invalid setup rejected");

        // Sentinel game bindings include slots/stages not touched by the NIF
        // pass. This catches an accidental ClearState or Execute(..., FALSE).
        const D3D11_VIEWPORT viewport{3, 7, 19, 23, 0.2f, 0.8f};
        const D3D11_RECT scissor{2, 4, 17, 21};
        context->RSSetViewports(1, &viewport);
        context->RSSetScissorRects(1, &scissor);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
        D3D11_BUFFER_DESC bufferDesc{};
        bufferDesc.ByteWidth = 32;
        bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        ComPtr<ID3D11Buffer> constant;
        Check(SUCCEEDED(device->CreateBuffer(&bufferDesc, nullptr, constant.GetAddressOf())), "sentinel constant buffer");
        ID3D11Buffer* constantPointer = constant.Get();
        context->VSSetConstantBuffers(0, 1, &constantPointer);
        context->PSSetConstantBuffers(0, 1, &constantPointer);
        context->GSSetConstantBuffers(3, 1, &constantPointer);
        context->CSSetConstantBuffers(5, 1, &constantPointer);
        DirectX::CommonStates states(device.Get());
        const float blendFactor[4]{0.2f, 0.3f, 0.4f, 0.5f};
        context->OMSetBlendState(states.Additive(), blendFactor, 0x12345678);
        context->OMSetDepthStencilState(states.DepthRead(), 7);
        context->RSSetState(states.CullClockwise());
        D3D11_TEXTURE2D_DESC targetDesc{};
        targetDesc.Width = targetDesc.Height = 32;
        targetDesc.MipLevels = targetDesc.ArraySize = targetDesc.SampleDesc.Count = 1;
        targetDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        targetDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
        ComPtr<ID3D11Texture2D> target;
        ComPtr<ID3D11RenderTargetView> targetView;
        Check(SUCCEEDED(device->CreateTexture2D(&targetDesc, nullptr, target.GetAddressOf())) &&
            SUCCEEDED(device->CreateRenderTargetView(target.Get(), nullptr, targetView.GetAddressOf())), "game render target");
        ID3D11RenderTargetView* targetPointer = targetView.Get();
        context->OMSetRenderTargets(1, &targetPointer, nullptr);
        const auto checkState = [&] {
            D3D11_VIEWPORT actual{}; UINT count = 1;
            context->RSGetViewports(&count, &actual);
            Check(count == 1 && std::memcmp(&actual, &viewport, sizeof(actual)) == 0, "game viewport preserved");
            D3D11_RECT actualScissor{}; count = 1;
            context->RSGetScissorRects(&count, &actualScissor);
            Check(count == 1 && std::memcmp(&actualScissor, &scissor, sizeof(scissor)) == 0, "scissor preserved");
            D3D11_PRIMITIVE_TOPOLOGY topology{};
            context->IAGetPrimitiveTopology(&topology);
            Check(topology == D3D11_PRIMITIVE_TOPOLOGY_LINELIST, "topology preserved");
            ComPtr<ID3D11Buffer> actualBuffer;
            context->VSGetConstantBuffers(0, 1, actualBuffer.GetAddressOf()); Check(actualBuffer == constant, "VS buffer preserved");
            context->PSGetConstantBuffers(0, 1, actualBuffer.ReleaseAndGetAddressOf()); Check(actualBuffer == constant, "PS buffer preserved");
            context->GSGetConstantBuffers(3, 1, actualBuffer.ReleaseAndGetAddressOf()); Check(actualBuffer == constant, "GS buffer preserved");
            context->CSGetConstantBuffers(5, 1, actualBuffer.ReleaseAndGetAddressOf()); Check(actualBuffer == constant, "CS buffer preserved");
            ComPtr<ID3D11BlendState> blend; float factor[4]{}; UINT mask = 0;
            context->OMGetBlendState(blend.GetAddressOf(), factor, &mask);
            Check(blend.Get() == states.Additive() && mask == 0x12345678 && std::memcmp(factor, blendFactor, sizeof(factor)) == 0, "blend preserved");
            ComPtr<ID3D11DepthStencilState> depth; UINT stencil = 0;
            context->OMGetDepthStencilState(depth.GetAddressOf(), &stencil);
            Check(depth.Get() == states.DepthRead() && stencil == 7, "depth state preserved");
            ComPtr<ID3D11RasterizerState> raster;
            context->RSGetState(raster.GetAddressOf()); Check(raster.Get() == states.CullClockwise(), "rasterizer preserved");
            ComPtr<ID3D11RenderTargetView> actualTarget;
            context->OMGetRenderTargets(1, actualTarget.GetAddressOf(), nullptr);
            Check(actualTarget == targetView, "game target preserved");
        };
        NifPreviewRenderer preview, second;
        Check(NifPreviewRendererTestAccess::Load(preview, data, Triangle()), "production mesh upload and shaders");
        Check(NifPreviewRendererTestAccess::Deferred(preview), "isolated context on game device");
        preview.Prepare(data, 64, 64);
        checkState();
        auto view = preview.GetSurfaceView();
        const auto first = Readback(data, view.Get(), 64, 64);
        Check(VisiblePixels(first) > 100 && VisiblePixels(first) < first.size(), "triangle plus transparent background rendered");
        Check(std::any_of(first.begin(), first.end(), [](auto p) { return (p >> 24) && ((p >> 16) & 255) > ((p >> 8) & 255); }), "material tint rendered");
        preview.Prepare(data, 64, 64);
        Check(preview.GetSurfaceView() == view, "retained frame without recreation");
        Check(NifPreviewRendererTestAccess::Load(second, data, Triangle(0.5f)), "second independent preview");
        second.Prepare(data, 64, 64);
        Check(second.GetSurfaceView() != view, "preview surfaces are independent");
        const auto translucent = Readback(data, second.GetSurfaceView().Get(), 64, 64);
        Check(std::any_of(translucent.begin(), translucent.end(), [](auto p) { return (p >> 24) > 0 && (p >> 24) < 255; }), "transparent material alpha retained");
        ID3D11ShaderResourceView* retained = view.Get();
        context->PSSetShaderResources(0, 1, &retained); // Previous compositor draw still bound.
        NifAPI::CameraState camera;
        camera.yawDegrees = 75;
        preview.SetCamera(camera);
        preview.Prepare(data, 64, 64);
        checkState();
        ComPtr<ID3D11ShaderResourceView> actualSrv;
        context->PSGetShaderResources(0, 1, actualSrv.GetAddressOf());
        Check(actualSrv == view, "retained compositor binding restored after preview update");
        Check(Readback(data, preview.GetSurfaceView().Get(), 64, 64) != first, "camera update changes rendered pixels");
        preview.Prepare(data, 96, 80);
        Check(VisiblePixels(Readback(data, preview.GetSurfaceView().Get(), 96, 80)) > 100, "resize rendered");
        preview.SetObjectVisible(1, false);
        preview.Prepare(data, 96, 80);
        Check(VisiblePixels(Readback(data, preview.GetSurfaceView().Get(), 96, 80)) == 0, "hide clears old model pixels");
        preview.SetObjectVisible(1, true);
        preview.Prepare(data, 96, 80);
        Check(VisiblePixels(Readback(data, preview.GetSurfaceView().Get(), 96, 80)) > 100, "show redraws model");
        preview.Clear(2);
        Check(!preview.GetSurfaceView(), "clear hides retained output immediately");
        preview.Prepare(data, 64, 64);
        Check(NifPreviewRendererTestAccess::Load(preview, data, Triangle()), "reload after clear");
        preview.Prepare(data, 64, 64);
        Check(VisiblePixels(Readback(data, preview.GetSurfaceView().Get(), 64, 64)) > 100, "reloaded model rendered");
        preview.BeginShutdown();
        Check(!preview.GetSurfaceView() && !preview.SetCamera(camera), "shutdown blocks stale output and updates");
        second.BeginShutdown();
        checkState();
        ComPtr<ID3D11InfoQueue> messages;
        if (SUCCEEDED(device.As(&messages)))
        {
            for (UINT64 i = 0; i < messages->GetNumStoredMessagesAllowedByRetrievalFilter(); ++i)
            {
                SIZE_T size = 0; messages->GetMessage(i, nullptr, &size);
                std::vector<std::uint8_t> storage(size);
                auto* message = reinterpret_cast<D3D11_MESSAGE*>(storage.data());
                messages->GetMessage(i, message, &size);
                if (message->Severity <= D3D11_MESSAGE_SEVERITY_ERROR)
                    throw std::runtime_error(message->pDescription);
            }
        }
        std::cout << "Production NIF GPU tests passed. DXVK=" << expectDxvk << " debugQueue=" << bool(messages) << '\n';
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
