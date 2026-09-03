#include "MeridianCefApp.h"

namespace Meridian::CEF
{
    namespace
    {
        std::string GetAdapterLuid()
        {
            const auto device = reinterpret_cast<ID3D11Device*>(RE::BSGraphics::Renderer::GetDevice());
            ThrowIfNullptr(MeridianCefApp, device);

            HRESULT hr;
            Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
            hr = device->QueryInterface<IDXGIDevice>(dxgiDevice.ReleaseAndGetAddressOf());
            CheckHresultThrow(hr, fmt::format("{}: failed to query interface {}", NameOf(MeridianCefApp), NameOf(IDXGIDevice)));

            Microsoft::WRL::ComPtr<IDXGIAdapter> dxgiAdapter;
            hr = dxgiDevice->GetAdapter(dxgiAdapter.ReleaseAndGetAddressOf());
            CheckHresultThrow(hr, fmt::format("{}: failed to get dxgi adapter", NameOf(MeridianCefApp)));

            DXGI_ADAPTER_DESC adapterDesc;
            hr = dxgiAdapter->GetDesc(&adapterDesc);
            CheckHresultThrow(hr, fmt::format("{}: failed to get dxgi adapter desc", NameOf(MeridianCefApp)));

            return fmt::format("{},{}", adapterDesc.AdapterLuid.HighPart, adapterDesc.AdapterLuid.LowPart);
        }
    }

    // command line switches https://peter.sh/experiments/chromium-command-line-switches/
    void MeridianCefApp::OnBeforeCommandLineProcessing(CefString const& process_type, CefRefPtr<CefCommandLine> command_line)
    {
        // disable creation of a GPUCache/ folder on disk
        // command_line->AppendSwitch("disable-gpu-shader-disk-cache");

        // command_line->AppendSwitch("disable-accelerated-video-decode");

        // un-comment to show the built-in Chromium fps meter
        // command_line->AppendSwitch("show-fps-counter");

        // command_line->AppendSwitch("disable-gpu-vsync");

        // Disables lazy loading of images and frames
        // command_line->AppendSwitch("disable-lazy-loading");

        // Most systems would not need to use this switch - but on older hardware,
        // Chromium may still choose to disable D3D11 for gpu workarounds.
        // Accelerated OSR will not at all with D3D11 disabled, so we force it on.
        command_line->AppendSwitchWithValue("use-angle", "d3d11");

        // Ensure Chromium runs on the same GPU (will not be able to copy frames otherwise)
        // Also requires D3D11
        auto luid = GetAdapterLuid();
        command_line->AppendSwitchWithValue("use-adapter-luid", luid);
        spdlog::info(NameOf(MeridianCefApp) ": using adapter luid={}", luid);

        // tell Chromium to autoplay <video> elements without
        // requiring the muted attribute or user interaction
        command_line->AppendSwitchWithValue("autoplay-policy", "no-user-gesture-required");

        // Disable task throttling of timer tasks from background pages
        command_line->AppendSwitch("disable-background-timer-throttling");

        // Prevent renderer process backgrounding when set
        command_line->AppendSwitch("disable-renderer-backgrounding");

        // Specifies that the main-thread Isolate should initialize in foreground mode
        // If not specified, the the Isolate will start in background mode for extension processes and foreground mode otherwise
        command_line->AppendSwitch("init-isolate-as-foreground");

        // Set default encoding to UTF-8
        command_line->AppendSwitchWithValue("default-encoding", "utf-8");

        // https://chromestatus.com/features
        // command_line->AppendSwitchWithValue("disable-features", "PrintCompositorLPAC");
    }

    void MeridianCefApp::OnRegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar)
    {
        Meridian::Scheme::RegisterModScheme(registrar);
    }

    CefRefPtr<CefBrowserProcessHandler> CEF::MeridianCefApp::GetBrowserProcessHandler()
    {
        return this;
    }

    void CEF::MeridianCefApp::OnBeforeChildProcessLaunch(CefRefPtr<CefCommandLine> command_line)
    {
        command_line->AppendSwitchWithValue(IPC_CL_PROCESS_ID_NAME, std::to_string(::GetCurrentProcessId()).c_str());
    }
}
