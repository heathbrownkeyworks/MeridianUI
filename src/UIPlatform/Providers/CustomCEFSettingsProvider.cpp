#include "CustomCEFSettingsProvider.h"
#include "Render/RenderHost.h"

namespace Meridian::Providers
{
    CustomCEFSettingsProvider::CustomCEFSettingsProvider(Meridian::UI::Settings a_settings)
        : m_settings(a_settings)
    {
    }

    Meridian::UI::Settings CustomCEFSettingsProvider::GetGlobalSettings()
    {
        return m_settings;
    }

    CefSettings CustomCEFSettingsProvider::GetCefSettings()
    {
        auto settings = m_defaultSettings->GetCefSettings();
        settings.remote_debugging_port = m_settings.remoteDebuggingPort;

        return settings;
    }

    CefBrowserSettings CustomCEFSettingsProvider::GetCefBrowserSettings()
    {
        return m_defaultSettings->GetCefBrowserSettings();
    }

    CefBrowserSettings CustomCEFSettingsProvider::MergeAndGetCefBrowserSettings(Meridian::UI::BrowserSettings* a_settings)
    {
        auto browserSettings = m_defaultSettings->MergeAndGetCefBrowserSettings(a_settings);
        const auto* renderData = Render::RenderHost::GetSingleton().GetRenderData();
        browserSettings.windowless_frame_rate = Render::BrowserFrameRate(renderData->browserTransport, a_settings->frameRate, renderData->cpuUploadFrameRate);

        return browserSettings;
    }

    CefWindowInfo CustomCEFSettingsProvider::GetCefWindowInfo()
    {
        return m_defaultSettings->GetCefWindowInfo();
    }
}
