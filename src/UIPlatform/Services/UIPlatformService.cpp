#include "UIPlatformService.h"
#include "Services/ControllerInputService.h"
#include "Hooks/InputDispatchHook.h"
#include "Hooks/PresentHook.h"
#include "Hooks/ShutdownHook.hpp"
#include "Menus/FocusMenu.h"
#include "Render/RenderHost.h"
#include "Services/InputLangSwitchService.h"

namespace Meridian::Services
{
    bool UIPlatformService::IsInited()
    {
        return s_isUIPInited.load(std::memory_order_acquire);
    }

    bool UIPlatformService::Init(std::shared_ptr<Meridian::Providers::ICEFSettingsProvider> a_settingsProvider)
    {
        std::lock_guard locker(s_uipInitMutex);
        if (s_isUIPInited.load(std::memory_order_acquire))
        {
            LOG_WARN("{}: already inited", NameOf(UIPlatformService));
            return false;
        }

        // Reject retries before settings providers can construct CEF values.
        if (CEFService::GetLifecycleState() == CEFService::LifecycleState::InitializationFailed)
        {
            LOG_ERROR("{}: CEF initialization previously failed; restart Skyrim before trying again", NameOf(UIPlatformService));
            return false;
        }

        if (a_settingsProvider == nullptr)
        {
            LOG_ERROR("{}: has null {}", NameOf(UIPlatformService), NameOf(a_settingsProvider));
            return false;
        }

        const auto globalSettings = a_settingsProvider->GetGlobalSettings();

        if (!Meridian::Hooks::PresentHook::IsInstalled())
        {
            LOG_ERROR("{}: present hook not installed — refusing to initialize (no rendering path)", NameOf(UIPlatformService));
            return false;
        }

        if (!Meridian::Hooks::InputDispatchHook::IsInstalled())
        {
            LOG_ERROR("{}: input dispatch hook not installed — refusing to initialize (focused input cannot be isolated)", NameOf(UIPlatformService));
            return false;
        }

        if (!Meridian::Hooks::ShutdownHook::IsInstalled())
        {
            LOG_ERROR("{}: shutdown hook not installed - refusing to initialize (CEF cannot be shut down safely)", NameOf(UIPlatformService));
            return false;
        }

        if (!Meridian::Render::RenderHost::GetSingleton().Init())
        {
            LOG_ERROR("{}: RenderHost init failed", NameOf(UIPlatformService));
            return false;
        }

        const auto app = CefRefPtr<Meridian::CEF::MeridianCefApp>(new Meridian::CEF::MeridianCefApp());
        try
        {
            Meridian::Services::CEFService::CEFInitialize(app, a_settingsProvider->GetCefSettings());
        }
        catch (const std::exception& error)
        {
            LOG_ERROR("{}: failed to CEFInitialize, {}", NameOf(UIPlatformService), error.what());
            return false;
        }
        catch (...)
        {
            LOG_ERROR("{}: failed to CEFInitialize", NameOf(UIPlatformService));
            return false;
        }

        RE::UI::GetSingleton()->Register(Meridian::Menus::FocusMenu::MENU_NAME, Meridian::Menus::FocusMenu::Creator);
        Meridian::Menus::FocusMenu::RegisterOpenCloseSink();

        Meridian::Services::InputRouter::GetSingleton().Register();
        Meridian::Services::InputRouter::GetSingleton().SetShuttingDown(false);

        Meridian::Services::InputLangSwitchService::GetSingleton().SetActive(globalSettings.nativeMenuLangSwitching);

        m_isShuttingDown.store(false, std::memory_order_release);
        s_isUIPInited.store(true, std::memory_order_release);
        return true;
    }

    bool UIPlatformService::InitAndShowMenuWithSettings(std::shared_ptr<Meridian::Providers::ICEFSettingsProvider> a_settingsProvider)
    {
        return Init(a_settingsProvider);
    }

    void UIPlatformService::Shutdown()
    {
        if (m_isShuttingDown.exchange(true, std::memory_order_acq_rel))
        {
            return;
        }

        LOG_INFO("{}: stopping menu work and draining CEF browsers", NameOf(UIPlatformService));

        Meridian::Services::InputRouter::GetSingleton().SetShuttingDown(true);
        Meridian::Services::ControllerInputService::GetSingleton().Shutdown();
        Meridian::Services::InputLangSwitchService::GetSingleton().SetActive(false);
        Meridian::Render::RenderHost::GetSingleton().BeginShutdown();

        constexpr auto browserCloseTimeout = std::chrono::seconds(10);
        const auto browsersClosed = Meridian::Services::CEFService::CloseAllBrowsersAndWait(browserCloseTimeout);

        Meridian::Render::RenderHost::GetSingleton().ClearAllSubMenu();
        LOG_INFO("{}: released all browser menu owners", NameOf(UIPlatformService));

        if (!browsersClosed)
        {
            LOG_CRITICAL("{}: browser drain failed; skipping unsafe CefShutdown", NameOf(UIPlatformService));
            s_isUIPInited.store(false, std::memory_order_release);
            return;
        }

        try
        {
            Meridian::Services::CEFService::CEFShutdown();
        }
        catch (const std::exception& error)
        {
            LOG_ERROR("{}: error while CEFShutdown, {}", NameOf(UIPlatformService), error.what());
        }
        catch (...)
        {
            LOG_ERROR("{}: error while CEFShutdown", NameOf(UIPlatformService));
        }

        s_isUIPInited.store(false, std::memory_order_release);
    }

    std::shared_ptr<Meridian::Menus::CEFMenu> UIPlatformService::CreateCefMenu(std::shared_ptr<Meridian::JS::JSFunctionStorage> a_funcStorage,
                                                                         Meridian::JS::JSEventFuncInfo& a_eventFuncInfo,
                                                                         std::shared_ptr<Meridian::Providers::ICEFSettingsProvider> a_settingsProvider)
    {
        return std::make_shared<Meridian::Menus::CEFMenu>(a_funcStorage,
                                                    a_eventFuncInfo,
                                                    a_settingsProvider);
    }
}
