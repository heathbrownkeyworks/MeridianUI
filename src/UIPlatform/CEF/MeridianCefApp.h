#pragma once

#include "Services/UIPlatformService.h"
#include "Scheme/ModSchemeHandler.h"

namespace Meridian::CEF
{
    class MeridianCefApp : public CefApp,
                          public CefBrowserProcessHandler
    {
        IMPLEMENT_REFCOUNTING(MeridianCefApp);

    public:
        ~MeridianCefApp() override = default;

        // CefApp
        void OnBeforeCommandLineProcessing(CefString const& process_type, CefRefPtr<CefCommandLine> command_line) override;
        void OnRegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar) override;
        CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override;

        // CefBrowserProcessHandler
        void OnBeforeChildProcessLaunch(CefRefPtr<CefCommandLine> command_line) override;
    };
}
