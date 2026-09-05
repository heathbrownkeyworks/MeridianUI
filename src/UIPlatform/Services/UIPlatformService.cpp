#include "UIPlatformService.h"
#include "Hooks/InputDispatchHook.h"
#include "Hooks/PresentHook.h"
#include "Hooks/ShutdownHook.hpp"
#include "Menus/FocusMenu.h"
#include "Render/RenderHost.h"
#include "Services/InputLangSwitchService.h"

namespace Meridian::Services
{
    UIPlatformService::UIPlatformService()
    {
        m_logger = spdlog::default_logger();
    }

    bool UIPlatformService::IsInited()
    {
        return s_isUIPInited.load(std::memory_order_acquire);
    }

    bool UIPlatformService::Init(std::shared_ptr<spdlog::logger> a_logger,
                                 std::shared_ptr<Meridian::Providers::ICEFSettingsProvider> a_settingsProvider)
    {
        std::lock_guard locker(s_uipInitMutex);
        if (s_isUIPInited.load(std::memory_order_acquire))
        {
            m_logger->warn("{}: already inited", NameOf(UIPlatformService));
            return false;
        }

        if (a_logger == nullptr)
        {
            spdlog::error("{}: has null {}", NameOf(UIPlatformService), NameOf(a_logger));
            return false;
        }
        m_logger = a_logger;

        // Reject retries before settings providers can construct CEF values.
        if (CEFService::GetLifecycleState() == CEFService::LifecycleState::InitializationFailed)
        {
            m_logger->error("{}: CEF initialization previously failed; restart Skyrim before trying again", NameOf(UIPlatformService));
            return false;
        }

        if (a_settingsProvider == nullptr)
        {
            m_logger->error("{}: has null {}", NameOf(UIPlatformService), NameOf(a_settingsProvider));
            return false;
        }

        const auto globalSettings = a_settingsProvider->GetGlobalSettings();

        if (!Meridian::Hooks::PresentHook::IsInstalled())
        {
            m_logger->error("{}: present hook not installed — refusing to initialize (no rendering path)", NameOf(UIPlatformService));
            return false;
        }

        if (!Meridian::Hooks::InputDispatchHook::IsInstalled())
        {
            m_logger->error("{}: input dispatch hook not installed — refusing to initialize (focused input cannot be isolated)", NameOf(UIPlatformService));
            return false;
        }

        if (!Meridian::Hooks::ShutdownHook::IsInstalled())
        {
            m_logger->error("{}: shutdown hook not installed - refusing to initialize (CEF cannot be shut down safely)", NameOf(UIPlatformService));
            return false;
        }

        if (!Meridian::Render::RenderHost::GetSingleton().Init(m_logger))
        {
            m_logger->error("{}: RenderHost init failed", NameOf(UIPlatformService));
            return false;
        }

        const auto app = CefRefPtr<Meridian::CEF::MeridianCefApp>(new Meridian::CEF::MeridianCefApp());
        try
        {
            Meridian::Services::CEFService::CEFInitialize(app, a_settingsProvider->GetCefSettings());
        }
        catch (const std::exception& error)
        {
            m_logger->error("{}: failed to CEFInitialize, {}", NameOf(UIPlatformService), error.what());
            return false;
        }
        catch (...)
        {
            m_logger->error("{}: failed to CEFInitialize", NameOf(UIPlatformService));
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
        const auto logger = spdlog::default_logger();
        return Init(logger, a_settingsProvider);
    }

    void UIPlatformService::Shutdown()
    {
        if (m_isShuttingDown.exchange(true, std::memory_order_acq_rel))
        {
            return;
        }

        m_logger->info("{}: stopping menu work and draining CEF browsers", NameOf(UIPlatformService));

        Meridian::Services::InputRouter::GetSingleton().SetShuttingDown(true);
        Meridian::Services::InputLangSwitchService::GetSingleton().SetActive(false);
        Meridian::Render::RenderHost::GetSingleton().BeginShutdown();

        constexpr auto browserCloseTimeout = std::chrono::seconds(10);
        const auto browsersClosed = Meridian::Services::CEFService::CloseAllBrowsersAndWait(browserCloseTimeout);

        Meridian::Render::RenderHost::GetSingleton().ClearAllSubMenu();
        m_logger->info("{}: released all browser menu owners", NameOf(UIPlatformService));

        if (!browsersClosed)
        {
            m_logger->critical("{}: browser drain failed; skipping unsafe CefShutdown", NameOf(UIPlatformService));
            s_isUIPInited.store(false, std::memory_order_release);
            return;
        }

        try
        {
            Meridian::Services::CEFService::CEFShutdown();
        }
        catch (const std::exception& error)
        {
            m_logger->error("{}: error while CEFShutdown, {}", NameOf(UIPlatformService), error.what());
        }
        catch (...)
        {
            m_logger->error("{}: error while CEFShutdown", NameOf(UIPlatformService));
        }

        s_isUIPInited.store(false, std::memory_order_release);
    }

    std::shared_ptr<Meridian::Menus::CEFMenu> UIPlatformService::CreateCefMenu(std::shared_ptr<Meridian::JS::JSFunctionStorage> a_funcStorage,
                                                                         Meridian::JS::JSEventFuncInfo& a_eventFuncInfo,
                                                                         std::shared_ptr<Meridian::Providers::ICEFSettingsProvider> a_settingsProvider)
    {
        return std::make_shared<Meridian::Menus::CEFMenu>(m_logger,
                                                    a_funcStorage,
                                                    a_eventFuncInfo,
                                                    a_settingsProvider);
    }
}
