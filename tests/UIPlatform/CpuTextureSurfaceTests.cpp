#define NOMINMAX
#include <windows.h>
#include "Render/CpuTextureSurface.h"
#include "Render/GraphicsBackend.h"
#include "Render/BrowserTransport.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string_view>

using namespace Meridian::Render;
using Microsoft::WRL::ComPtr;
void Check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }

std::vector<std::uint32_t> ReadPixels(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11ShaderResourceView* view)
{
    Check(view != nullptr, "visible texture expected");
    ComPtr<ID3D11Resource> resource;
    view->GetResource(resource.GetAddressOf());
    ComPtr<ID3D11Texture2D> texture;
    Check(SUCCEEDED(resource.As(&texture)), "texture interface");
    D3D11_TEXTURE2D_DESC desc{};
    texture->GetDesc(&desc);
    Check(desc.MiscFlags == 0, "CPU path must not create shared resources");
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging;
    Check(SUCCEEDED(device->CreateTexture2D(&desc, nullptr, staging.GetAddressOf())), "readback allocation");
    context->CopyResource(staging.Get(), texture.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    Check(SUCCEEDED(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)), "readback map");
    std::vector<std::uint32_t> pixels(std::size_t(desc.Width) * desc.Height);
    for (UINT y = 0; y < desc.Height; ++y)
        std::memcpy(pixels.data() + std::size_t(y) * desc.Width,
            static_cast<const std::uint8_t*>(mapped.pData) + std::size_t(y) * mapped.RowPitch, desc.Width * 4);
    context->Unmap(staging.Get(), 0);
    return pixels;
}

int main(int argc, char** argv)
{
    try
    {
        const bool dxvkExpected = argc > 1 && std::string_view(argv[1]) == "--expect-dxvk";
        ComPtr<ID3D11Device> device;
        ComPtr<ID3D11DeviceContext> context;
        const auto hr = D3D11CreateDevice(nullptr, dxvkExpected ? D3D_DRIVER_TYPE_HARDWARE : D3D_DRIVER_TYPE_WARP,
            nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION, device.GetAddressOf(), nullptr, context.GetAddressOf());
        Check(SUCCEEDED(hr), "D3D11 device creation");
        const bool detected = IsDxvkDevice(device.Get());
        Check(detected == dxvkExpected, "actual device backend detection");
        Check(ResolveBrowserTransport(BrowserTransport::Auto, detected, IsWine()) ==
            (dxvkExpected && !IsWine() ? BrowserTransport::CpuUpload : BrowserTransport::SharedTexture), "automatic transport selection");
        std::cout << "DXVK detected=" << detected << " Wine=" << IsWine() << '\n';
        CpuTextureSurface view, popup;
        std::vector<std::uint32_t> paint(12, 0xff112233);
        view.Frames().Submit(paint.data(), 4, 3, {});
        const D3D11_VIEWPORT viewport{3, 7, 100, 80, 0, 1};
        context->RSSetViewports(1, &viewport);
        Check(view.UploadLatest(nullptr, context.Get()) == E_POINTER, "missing device rejected without discarding pixels");
        Check(view.UploadLatest(device.Get(), context.Get()) == S_OK, "first upload");
        Check(ReadPixels(device.Get(), context.Get(), view.View()) == paint, "BGRA first frame readback");
        Check(view.UploadLatest(device.Get(), context.Get()) == S_FALSE && view.View(), "retained frame without redundant upload");
        paint[5] = 0xffabcdef;
        const PixelRect one{1, 1, 1, 1};
        view.Frames().Submit(paint.data(), 4, 3, std::span(&one, 1));
        paint[11] = 0xff998877;
        const PixelRect last{3, 2, 1, 1};
        view.Frames().Submit(paint.data(), 4, 3, std::span(&last, 1));
        Check(view.UploadLatest(device.Get(), context.Get()) == S_OK, "coalesced update");
        Check(ReadPixels(device.Get(), context.Get(), view.View()) == paint, "partial uploads preserve all unchanged pixels");
        std::uint32_t translucent = 0x80402010;
        popup.Frames().Submit(&translucent, 1, 1, {});
        Check(popup.UploadLatest(device.Get(), context.Get()) == S_OK, "popup upload");
        Check(ReadPixels(device.Get(), context.Get(), popup.View())[0] == 0x80804020, "premultiplied paint converted to straight alpha");
        popup.Frames().Reset();
        Check(popup.View() == nullptr, "popup reuse cannot show stale pixels");
        Check(ReadPixels(device.Get(), context.Get(), view.View()) == paint, "popup does not alter the main surface");
        std::vector<std::uint32_t> resized(35, 0xffa0b0c0);
        view.Frames().Submit(resized.data(), 7, 5, std::span(&one, 1));
        Check(view.View() == nullptr, "resize invalidates old surface until upload");
        Check(view.UploadLatest(device.Get(), context.Get()) == S_OK, "resized upload");
        Check(ReadPixels(device.Get(), context.Get(), view.View()) == resized, "resize contents");
        UINT count = 1;
        D3D11_VIEWPORT actual{};
        context->RSGetViewports(&count, &actual);
        Check(count == 1 && std::memcmp(&actual, &viewport, sizeof(viewport)) == 0, "upload preserves game viewport state");
        view.Frames().Stop();
        Check(!view.View() && view.UploadLatest(device.Get(), context.Get()) == S_FALSE, "shutdown suppresses stale frames and upload work");
        std::cout << "CPU texture upload/readback checks passed\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
