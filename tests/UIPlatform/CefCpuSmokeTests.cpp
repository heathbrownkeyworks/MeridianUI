#define NOMINMAX
#include <windows.h>
#include "include/cef_app.h"
#include "include/cef_client.h"
#include "include/cef_parser.h"
#include "Render/CpuTextureSurface.h"
#include "Render/GraphicsBackend.h"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <thread>

// Opt-in integration fixture: requires the matching CEF runtime beside the
// executable. --expect-dxvk additionally requires isolated DXVK DLLs there.
using namespace Meridian::Render;
using Microsoft::WRL::ComPtr;
using namespace std::chrono_literals;

class SmokeApp final : public CefApp
{
public:
    void OnBeforeCommandLineProcessing(const CefString&, CefRefPtr<CefCommandLine> command) override
    {
        command->AppendSwitch("do-not-de-elevate");
        command->AppendSwitchWithValue("use-angle", "d3d11");
    }
private:
    IMPLEMENT_REFCOUNTING(SmokeApp);
};

class SmokeClient final : public CefClient, public CefRenderHandler, public CefLifeSpanHandler
{
public:
    CpuTextureSurface surface;
    std::atomic_int width{64};
    std::atomic_bool closed{false};
    std::atomic_uint paints{0}, acceleratedPaints{0};
    CefRefPtr<CefRenderHandler> GetRenderHandler() override { return this; }
    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
    void GetViewRect(CefRefPtr<CefBrowser>, CefRect& rect) override { rect.Set(0, 0, width.load(), 64); }
    void OnPaint(CefRefPtr<CefBrowser>, PaintElementType type, const RectList& rects,
        const void* pixels, int w, int h) override
    {
        if (type != PET_VIEW) return;
        std::vector<PixelRect> dirty;
        for (const auto& rect : rects) dirty.push_back({rect.x, rect.y, rect.width, rect.height});
        if (surface.Frames().Submit(pixels, w, h, dirty)) ++paints;
    }
    void OnAcceleratedPaint(CefRefPtr<CefBrowser>, PaintElementType, const RectList&,
        const CefAcceleratedPaintInfo&) override { ++acceleratedPaints; }
    void OnAfterCreated(CefRefPtr<CefBrowser> browser) override { std::lock_guard lock(m_mutex); m_browser = browser; }
    void OnBeforeClose(CefRefPtr<CefBrowser>) override
    {
        std::lock_guard lock(m_mutex);
        m_browser = nullptr;
        closed = true;
    }
    CefRefPtr<CefBrowser> Browser() { std::lock_guard lock(m_mutex); return m_browser; }
private:
    std::mutex m_mutex;
    CefRefPtr<CefBrowser> m_browser;
    IMPLEMENT_REFCOUNTING(SmokeClient);
};

bool Matches(ID3D11Device* device, ID3D11DeviceContext* context, ID3D11ShaderResourceView* view,
    int expectedWidth, std::uint32_t opaque)
{
    if (!view) return false;
    ComPtr<ID3D11Resource> resource;
    view->GetResource(resource.GetAddressOf());
    ComPtr<ID3D11Texture2D> texture;
    if (FAILED(resource.As(&texture))) return false;
    D3D11_TEXTURE2D_DESC desc{};
    texture->GetDesc(&desc);
    if (desc.Width != UINT(expectedWidth) || desc.Height != 64 || desc.MiscFlags != 0) return false;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> staging;
    if (FAILED(device->CreateTexture2D(&desc, nullptr, staging.GetAddressOf()))) return false;
    context->CopyResource(staging.Get(), texture.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped))) return false;
    const auto* row = reinterpret_cast<const std::uint32_t*>(static_cast<const char*>(mapped.pData) + 16 * mapped.RowPitch);
    bool match = row[16] == opaque;
    const auto half = row[48];
    const std::uint32_t expected = 0x804080c0;
    for (unsigned shift = 0; shift < 32; shift += 8)
        match &= std::abs(int((half >> shift) & 255) - int((expected >> shift) & 255)) <= 1;
    if (expectedWidth == 96) match &= row[80] == 0;
    context->Unmap(staging.Get(), 0);
    return match;
}

int main()
{
    CefMainArgs args(GetModuleHandleW(nullptr));
    CefRefPtr<SmokeApp> app = new SmokeApp;
    const int childExit = CefExecuteProcess(args, app, nullptr);
    if (childExit >= 0) return childExit;
    const bool expectDxvk = std::wstring(GetCommandLineW()).find(L"--expect-dxvk") != std::wstring::npos;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    if (FAILED(D3D11CreateDevice(nullptr, expectDxvk ? D3D_DRIVER_TYPE_HARDWARE : D3D_DRIVER_TYPE_WARP,
        nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION,
        device.GetAddressOf(), nullptr, context.GetAddressOf())) || IsDxvkDevice(device.Get()) != expectDxvk) return 1;
    const auto root = std::filesystem::current_path();
    CefSettings settings;
    settings.no_sandbox = true;
    settings.multi_threaded_message_loop = true;
    settings.windowless_rendering_enabled = true;
    CefString(&settings.root_cache_path) = (root / "smoke-cache").wstring();
    CefString(&settings.log_file) = (root / "cef-smoke.log").wstring();
    if (expectDxvk)
        // Match Skyrim + Meridian's deployment: DXVK belongs beside the game,
        // while the CEF helper has its own directory without DXVK wrappers.
        CefString(&settings.browser_subprocess_path) = (root / "helper" / "CefCpuSmokeTests.exe").wstring();
    if (!CefInitialize(args, settings, app, nullptr)) return 2;
    CefRefPtr<SmokeClient> client = new SmokeClient;
    CefWindowInfo window;
    window.SetAsWindowless(nullptr);
    window.shared_texture_enabled = false;
    CefBrowserSettings browserSettings;
    browserSettings.windowless_frame_rate = 30;
    browserSettings.background_color = 0;
    const std::string html = "<html><body style='margin:0;background:transparent'>"
        "<div id='opaque' style='position:absolute;left:0;top:0;width:32px;height:64px;background:rgb(18,52,86)'></div>"
        "<div style='position:absolute;left:32px;top:0;width:32px;height:64px;background:rgba(64,128,192,0.5)'></div>"
        "</body></html>";
    const auto url = "data:text/html;base64," + CefBase64Encode(html.data(), html.size()).ToString();
    const bool created = CefBrowserHost::CreateBrowser(window, client, url, browserSettings, nullptr, nullptr);
    const auto waitFor = [&](int width, std::uint32_t color) {
        const auto deadline = std::chrono::steady_clock::now() + 20s;
        while (std::chrono::steady_clock::now() < deadline)
        {
            if (FAILED(client->surface.UploadLatest(device.Get(), context.Get()))) return false;
            if (Matches(device.Get(), context.Get(), client->surface.View(), width, color)) return true;
            std::this_thread::sleep_for(10ms);
        }
        return false;
    };
    bool passed = created && waitFor(64, 0xff123456);
    auto browser = client->Browser();
    if (passed && browser)
    {
        browser->GetMainFrame()->ExecuteJavaScript("document.getElementById('opaque').style.background='rgb(86,52,18)'", url, 0);
        passed = waitFor(64, 0xff563412);
        client->width = 96;
        browser->GetHost()->WasResized();
        passed = passed && waitFor(96, 0xff563412);
    }
    if (browser) browser->GetHost()->CloseBrowser(true);
    browser = nullptr;
    const auto deadline = std::chrono::steady_clock::now() + 10s;
    while (created && !client->closed && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(10ms);
    std::cout << "DXVK=" << expectDxvk << " paints=" << client->paints << " accelerated=" << client->acceleratedPaints
              << " closed=" << client->closed << " pixel/alpha/update/resize=" << passed << std::endl;
    if (created && !client->closed) std::_Exit(3); // Do not call CefShutdown with a live browser.
    passed = passed && client->paints > 0 && client->acceleratedPaints == 0;
    client->surface.Frames().Stop();
    client = nullptr;
    CefShutdown();
    std::cout << "CefShutdown returned" << std::endl;
    return passed ? 0 : 1;
}
