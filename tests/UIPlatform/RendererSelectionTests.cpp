#include "Render/RendererSelection.h"

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
