#pragma once

#include "PCH.h"
#include "Utils/PathUtils.h"
#include "ICEFSettingsProvider.h"

namespace Meridian::Providers
{
    class DefaultCEFSettingsProvider : public ICEFSettingsProvider
    {
    public:
        virtual ~DefaultCEFSettingsProvider() override = default;

        virtual Meridian::UI::Settings GetGlobalSettings() override;
        virtual CefSettings GetCefSettings() override;
        virtual CefBrowserSettings GetCefBrowserSettings() override;
        virtual CefBrowserSettings MergeAndGetCefBrowserSettings(Meridian::UI::BrowserSettings* a_settings) override;
        virtual CefWindowInfo GetCefWindowInfo() override;
    };
}
