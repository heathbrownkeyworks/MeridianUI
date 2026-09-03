#pragma once

#include "PCH.h"
#include "MeridianUIAPI/Settings.h"

namespace Meridian::Providers
{
    class ICEFSettingsProvider
    {
    public:
        virtual ~ICEFSettingsProvider() = default;

        virtual Meridian::UI::Settings GetGlobalSettings() = 0;
        virtual CefSettings GetCefSettings() = 0;
        virtual CefBrowserSettings GetCefBrowserSettings() = 0;
        virtual CefBrowserSettings MergeAndGetCefBrowserSettings(Meridian::UI::BrowserSettings* a_settings) = 0;
        virtual CefWindowInfo GetCefWindowInfo() = 0;
    };
}
