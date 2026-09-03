#pragma once

#include "PCH.h"
#include "ICEFSettingsProvider.h"
#include "DefaultCEFSettingsProvider.h"
#include "MeridianUIAPI/Settings.h"

namespace Meridian::Providers
{
    class CustomCEFSettingsProvider : public ICEFSettingsProvider
    {
    protected:
        std::unique_ptr<DefaultCEFSettingsProvider> m_defaultSettings = std::make_unique<DefaultCEFSettingsProvider>();
        Meridian::UI::Settings m_settings;

    public:
        CustomCEFSettingsProvider(Meridian::UI::Settings a_settings);
        virtual ~CustomCEFSettingsProvider() override = default;

        virtual Meridian::UI::Settings GetGlobalSettings() override;
        virtual CefSettings GetCefSettings() override;
        virtual CefBrowserSettings GetCefBrowserSettings() override;
        virtual CefBrowserSettings MergeAndGetCefBrowserSettings(Meridian::UI::BrowserSettings* a_settings) override;
        virtual CefWindowInfo GetCefWindowInfo() override;
    };
}
