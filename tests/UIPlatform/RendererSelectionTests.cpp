#include "Render/RendererSelection.h"
#include "Render/BrowserTransport.h"

#include <iostream>

namespace
{
    int g_failures = 0;

    void Expect(bool a_condition, const char* a_message)
    {
        if (!a_condition)
        {
            ++g_failures;
            std::cerr << "FAILED: " << a_message << '\n';
        }
    }
}

int main()
{
    using Meridian::Render::ResolveBrowserRenderer;
    using Meridian::UI::RendererType;
    using namespace Meridian::Render;
    Expect(ResolveBrowserTransport(BrowserTransport::Auto, true, false) == BrowserTransport::CpuUpload, "Windows DXVK selects CPU uploads");
    Expect(ResolveBrowserTransport(BrowserTransport::Auto, true, true) == BrowserTransport::SharedTexture, "Proton retains its existing shared path");
    Expect(ResolveBrowserTransport(BrowserTransport::Auto, false, false) == BrowserTransport::SharedTexture, "ordinary D3D11 unchanged");
    Expect(ResolveBrowserTransport(BrowserTransport::CpuUpload, false, false) == BrowserTransport::CpuUpload, "forced CPU override");
    Expect(ResolveBrowserTransport(BrowserTransport::SharedTexture, true, false) == BrowserTransport::SharedTexture, "explicit shared override");
    Expect(BrowserFrameRate(BrowserTransport::CpuUpload, 60, 30) == 30 && BrowserFrameRate(BrowserTransport::CpuUpload, 15, 30) == 15, "CPU cap respects slower consumer rates");
    Expect(BrowserFrameRate(BrowserTransport::SharedTexture, 60, 30) == 60, "shared frame rates unchanged");

    Expect(ResolveBrowserRenderer(RendererType::RingBuffer, true, true) == RendererType::RingBuffer,
           "supported ring transport remains selected");
    Expect(ResolveBrowserRenderer(RendererType::RingBuffer, false, false) == RendererType::SyncCopy,
           "missing platform device falls back to SyncCopy");
    Expect(ResolveBrowserRenderer(RendererType::RingBuffer, true, false) == RendererType::SyncCopy,
           "failed shared-keyed capability probe falls back to SyncCopy");
    Expect(ResolveBrowserRenderer(RendererType::SyncCopy, false, false) == RendererType::SyncCopy,
           "explicit SyncCopy selection is unchanged");
    Expect(ResolveBrowserRenderer(RendererType::DeferredContext, false, false) == RendererType::DeferredContext,
           "legacy diagnostic selection is unchanged");

    if (g_failures != 0)
    {
        std::cerr << g_failures << " renderer-selection test(s) failed\n";
        return 1;
    }
    std::cout << "All renderer-selection tests passed\n";
    return 0;
}
