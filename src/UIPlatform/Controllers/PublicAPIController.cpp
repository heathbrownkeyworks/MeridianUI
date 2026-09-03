#include "PublicAPIController.h"
#include "Controllers/PublicAPIValidation.h"
#include "Controllers/NifSceneAPIController.h"
#include "Controllers/NifViewAPIController.h"
#include "Controllers/RenderLayerAPIController.h"
#include "Controllers/ViewAPIController.h"
#include "Render/RenderHost.h"
#include "MeridianUIAPI/SettingsIngest.h"
#include "Config/IniConfig.h"

namespace Meridian::Controllers
{
    Meridian::UI::ResponseVersionMessage* PublicAPIController::GetVersionMessage()
    {
        return &m_rvMessage;
    }

    Meridian::UI::ResponseAPIMessage* PublicAPIController::GetAPIMessage()
    {
        return &m_rAPIMessage;
    }

    void PublicAPIController::Init()
    {
        const auto applicationThreadId = std::this_thread::get_id();
        {
            std::lock_guard platformInitLock(m_initPlatformServiceMutex);
            m_applicationThreadId = applicationThreadId;
        }
        spdlog::info("{}: application thread designated as {}",
                     NameOf(PublicAPIController),
                     std::hash<std::thread::id>{}(applicationThreadId));

        m_onShutdownConnection = Meridian::Hooks::ShutdownHook::OnShutdown.connect([this]() {
            Shutdown();
        });

        // Coexistence probe for diagnostics. Cursor ownership is focus-scoped:
        // Meridian draws the final cursor and suppresses CursorMenu only while
        // a Meridian browser owns focus. Merely loading PrismaUI must not hand
        // ownership to its vanilla cursor because Meridian composites afterward.
        //
        // Runs on kPostPostLoad — SKSE loads plugin DLLs alphabetically, so
        // MeridianUIPlugin is in-process before PrismaUI.dll and a probe at
        // load entry would always miss; kPostPostLoad fires exactly once,
        // after every plugin's load entry has run. Registered on its own
        // sender-"SKSE" listener: SKSE's lifecycle broadcasts do not reach
        // the all-senders listener below, so the probe needs this explicit
        // form.
        SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* a_msg) {
            if (a_msg->type == SKSE::MessagingInterface::kPostPostLoad &&
                ::GetModuleHandleW(L"PrismaUI.dll") != nullptr)
            {
                spdlog::info("{}: PrismaUI detected in-process — focus-scoped cursor arbitration active", NameOf(PublicAPIController));
            }
        });

        SKSE::GetMessagingInterface()->RegisterListener(nullptr, [](SKSE::MessagingInterface::Message* a_msg) {
            if (std::strcmp(a_msg->sender, "SKSE") == 0)
            {
                return;
            }

            auto& controller = PublicAPIController::GetSingleton();
            switch (a_msg->type)
            {
            case Meridian::UI::APIMessageType::RequestVersion:
                spdlog::info("{}: Request version data from \"{}\"", NameOf(PublicAPIController), a_msg->sender);
                SKSE::GetMessagingInterface()->Dispatch(Meridian::UI::APIMessageType::ResponseVersion,
                                                        static_cast<void*>(controller.GetVersionMessage()),
                                                        sizeof(*controller.GetVersionMessage()),
                                                        a_msg->sender);
                break;
            case Meridian::UI::APIMessageType::RequestAPI: {
                if (a_msg->data == nullptr || a_msg->dataLen != sizeof(Meridian::UI::RequestAPIMessage))
                {
                    spdlog::error("{}: INVALID request api from \"{}\". No data.", NameOf(PublicAPIController), a_msg->sender);
                    return;
                }
                else
                {
                    spdlog::info("{}: Request api from \"{}\"", NameOf(PublicAPIController), a_msg->sender);
                }

                if (!controller.InitIfNotPlatformService(static_cast<Meridian::UI::Settings*>(a_msg->data)))
                {
                    spdlog::error("{}: Can't response API because ui platform failed to init", NameOf(PublicAPIController));
                    break;
                }
                SKSE::GetMessagingInterface()->Dispatch(Meridian::UI::APIMessageType::ResponseAPI,
                                                        static_cast<void*>(controller.GetAPIMessage()),
                                                        sizeof(*controller.GetAPIMessage()),
                                                        a_msg->sender);
                break;
            }
            default:
                break;
            }
        });
    }

    void PublicAPIController::Shutdown()
    {
        {
            // This lock is the lifecycle linearization point shared with every
            // AddOrGetBrowser call. Once the gate changes here, no browser
            // acquisition that has not already returned can enter CEF/menu work.
            std::lock_guard admissionLock(m_lifecycleAdmissionMutex);
            if (m_isShuttingDown.exchange(true, std::memory_order_acq_rel))
            {
                return;
            }
        }

        // Synchronize with an initialization that passed the outer gate before
        // shutdown began. New initializations fail the in-lock gate below.
        {
            std::lock_guard platformInitLock(m_initPlatformServiceMutex);
        }

        spdlog::info("{}: shutdown requested; rejecting new browser work", NameOf(PublicAPIController));

        // Quiesce rendering and JS callback dispatch before consumer-owned state
        // is torn down by the callbacks below. Browser closure remains tracked
        // and is drained by UIPlatformService::Shutdown after consumers release.
        auto& platformService = Meridian::Services::UIPlatformService::GetSingleton();
        if (platformService.IsInited())
        {
            Meridian::Render::RenderHost::GetSingleton().BeginShutdown();
        }

        // Invalidate all extension handles before any consumer shutdown
        // callback can tear down its own static state.
        Meridian::Controllers::NifSceneAPIController::GetSingleton().BeginShutdown();
        Meridian::Controllers::NifViewAPIController::GetSingleton().BeginShutdown();
        Meridian::Controllers::ViewAPIController::GetSingleton().BeginShutdown();
        Meridian::Controllers::RenderLayerAPIController::GetSingleton().BeginShutdown();

        std::queue<OnShutdownFunc_t> shutdownCallbacks;
        {
            std::lock_guard lock(m_onShutdownFuncsMutex);
            shutdownCallbacks.swap(m_onShutdownFuncs);
        }

        while (!shutdownCallbacks.empty())
        {
            const auto callback = shutdownCallbacks.front();
            shutdownCallbacks.pop();

            if (callback == nullptr)
            {
                continue;
            }

            try
            {
                callback();
            }
            catch (const std::exception& error)
            {
                spdlog::error("{}: consumer shutdown callback failed: {}", NameOf(PublicAPIController), error.what());
            }
            catch (...)
            {
                spdlog::error("{}: consumer shutdown callback failed", NameOf(PublicAPIController));
            }
        }

        {
            std::lock_guard lock(m_mapMutex);
            m_browserHandleMap.clear();
            m_browserNameMap.clear();
            m_closingBrowserNames.clear();
        }

        Meridian::Services::UIPlatformService::GetSingleton().Shutdown();
        spdlog::default_logger()->flush();
    }

    bool PublicAPIController::InitIfNotPlatformService(const Meridian::UI::Settings* a_settings)
    {
        std::lock_guard admissionLock(m_lifecycleAdmissionMutex);
        std::lock_guard locker(m_initPlatformServiceMutex);
        if (m_isShuttingDown.load(std::memory_order_acquire))
        {
            spdlog::warn("{}: refusing initialization during shutdown", NameOf(PublicAPIController));
            return false;
        }

        auto& platformService = Meridian::Services::UIPlatformService::GetSingleton();
        if (platformService.IsInited())
        {
            return true;
        }

        const auto currentThreadId = std::this_thread::get_id();
        if (m_applicationThreadId == std::thread::id{} || currentThreadId != m_applicationThreadId)
        {
            spdlog::error("{}: refusing first CEF initialization on thread {}; application thread is {}",
                          NameOf(PublicAPIController),
                          std::hash<std::thread::id>{}(currentThreadId),
                          std::hash<std::thread::id>{}(m_applicationThreadId));
            return false;
        }

        Meridian::UI::Settings ingested{};
        if (!Meridian::UI::IngestSettings(a_settings, Meridian::UI::kSettingsMinSize10, ingested))
        {
            spdlog::error("PublicAPIController: caller Settings rejected — structSize below the 1.0 minimum (pre-1.0 layout or corrupt); refusing API");
            return false;
        }

        const auto& iniOverrides = Meridian::Config::LoadIniOverrides();
        Meridian::Config::ApplyOverrides(iniOverrides, ingested);
        {
            std::string overriddenKeys;
            if (iniOverrides.rendererType)
            {
                overriddenKeys += "RendererType ";
            }
            if (iniOverrides.remoteDebuggingEnabled)
            {
                overriddenKeys += "RemoteDebuggingEnabled ";
            }
            if (iniOverrides.remoteDebuggingPort)
            {
                overriddenKeys += "RemoteDebuggingPort ";
            }
            if (iniOverrides.allowRemoteContent)
            {
                overriddenKeys += "AllowRemoteContent ";
            }
            if (iniOverrides.nativeMenuLangSwitching)
            {
                overriddenKeys += "NativeMenuLangSwitching ";
            }
            if (!overriddenKeys.empty())
            {
                spdlog::info("{}: MeridianUI.ini overrode: {}", NameOf(PublicAPIController), overriddenKeys);
            }
        }

        Meridian::Converters::KeyInputConverter::UpdateKeyboardLayouts();
        Meridian::Converters::KeyInputConverter::SetNativeMenuLangSwitching(ingested.nativeMenuLangSwitching);
        if (ingested.nativeMenuLangSwitching)
        {
            // Some plugins (like SkyrimSouls) can hook translate functions, we need to hook them after all plugins
            if (!Meridian::Hooks::CharGeneratorHook::Install())
            {
                spdlog::warn("Native-menu language switching is unavailable on this Skyrim runtime");
            }
        }

        SetSettingsProvider(&ingested);
        return platformService.InitAndShowMenuWithSettings(GetSettingsProvider());
    }

    void PublicAPIController::SetSettingsProvider(const Meridian::UI::Settings* a_settings)
    {
        if (a_settings == nullptr || m_settingsProvider != nullptr)
        {
            return;
        }

        m_settingsProvider = std::make_shared<Meridian::Providers::CustomCEFSettingsProvider>(*a_settings);
    }

    std::shared_ptr<Meridian::Providers::ICEFSettingsProvider> PublicAPIController::GetSettingsProvider()
    {
        return m_settingsProvider;
    }

#pragma region Meridian::UI::IUIPlatformAPI

    void __cdecl PublicAPIController::ReleaseBrowserHandle(BrowserRefHandle a_handle)
    {
        std::string browserName;
        bool removeBrowser = false;
        {
            std::lock_guard lock(m_mapMutex);
            const auto handleIt = m_browserHandleMap.find(a_handle);
            if (handleIt == m_browserHandleMap.cend())
            {
                if (!m_isShuttingDown.load(std::memory_order_relaxed))
                {
                    spdlog::warn("{}: not found browser ref handle", NameOf(PublicAPIController));
                }
                return;
            }

            browserName = handleIt->second;
            m_browserHandleMap.erase(handleIt);
            const auto browserIt = m_browserNameMap.find(browserName);
            if (browserIt == m_browserNameMap.cend())
            {
                spdlog::warn("{}: not found browser pointer", NameOf(PublicAPIController));
                return;
            }

            browserIt->second.refHandles.erase(a_handle);
            if (browserIt->second.refHandles.empty())
            {
                m_browserNameMap.erase(browserIt);
                m_closingBrowserNames.insert(browserName);
                removeBrowser = true;
            }
        }

        if (!removeBrowser)
        {
            return;
        }

        if (Meridian::Services::UIPlatformService::GetSingleton().IsInited())
        {
            Meridian::Render::RenderHost::GetSingleton().RemoveSubMenu(browserName);
        }
        else
        {
            spdlog::error("{}: platform service is not initialized", NameOf(PublicAPIController));
        }

        {
            std::lock_guard lock(m_mapMutex);
            m_closingBrowserNames.erase(browserName);
        }
    }

    BrowserRefHandle __cdecl PublicAPIController::AddOrGetBrowser(const char* a_browserName,
                                                                  Meridian::JS::JSFuncInfo* const* a_funcInfoArr,
                                                                  const std::uint32_t a_funcInfoArrSize,
                                                                  const char* a_startUrl,
                                                                  Meridian::CEF::IBrowser*& a_outBrowser)
    {
        Meridian::UI::BrowserSettings defaultSettings{};
        return AddOrGetBrowser(a_browserName, a_funcInfoArr, a_funcInfoArrSize, a_startUrl, &defaultSettings, a_outBrowser);
    }

    BrowserRefHandle __cdecl PublicAPIController::AddOrGetBrowser(const char* a_browserName,
                                                                  Meridian::JS::JSFuncInfo* const* a_funcInfoArr,
                                                                  const std::uint32_t a_funcInfoArrSize,
                                                                  const char* a_startUrl,
                                                                  Meridian::UI::BrowserSettings* a_settings,
                                                                  Meridian::CEF::IBrowser*& a_outBrowser)
    {
        a_outBrowser = nullptr;
        const auto validationError = PublicAPIValidation::ValidateBrowserRequest(
            a_browserName, a_funcInfoArr, a_funcInfoArrSize, a_startUrl);
        if (validationError != PublicAPIValidation::Error::None)
        {
            spdlog::error("{}: rejected browser request: {}",
                          NameOf(PublicAPIController),
                          PublicAPIValidation::ToString(validationError));
            return Meridian::UI::IUIPlatformAPI::InvalidBrowserRefHandle;
        }

        // Hold admission through the complete acquire/create transaction. The
        // shutdown transition takes the same lock before closing the gate, so a
        // delayed caller can never return a valid raw browser after that point.
        std::lock_guard admissionLock(m_lifecycleAdmissionMutex);
        if (m_isShuttingDown.load(std::memory_order_acquire))
        {
            return Meridian::UI::IUIPlatformAPI::InvalidBrowserRefHandle;
        }

        if (!Meridian::Services::UIPlatformService::GetSingleton().IsInited())
        {
            spdlog::error("{}: platform service is not initialized", NameOf(PublicAPIController));
            a_outBrowser = nullptr;
            return Meridian::UI::IUIPlatformAPI::InvalidBrowserRefHandle;
        }
        auto& renderHost = Meridian::Render::RenderHost::GetSingleton();

        if (m_settingsProvider == nullptr)
        {
            a_outBrowser = nullptr;
            return Meridian::UI::IUIPlatformAPI::InvalidBrowserRefHandle;
        }

        std::lock_guard lock(m_mapMutex);
        if (m_closingBrowserNames.contains(a_browserName))
        {
            spdlog::warn("{}: browser \"{}\" is still closing", NameOf(PublicAPIController), a_browserName);
            a_outBrowser = nullptr;
            return Meridian::UI::IUIPlatformAPI::InvalidBrowserRefHandle;
        }

        const auto browserIt = m_browserNameMap.find(a_browserName);
        if (browserIt != m_browserNameMap.cend())
        {
            const auto refHandle = m_currentRefHandle++;
            browserIt->second.refHandles.insert(refHandle);
            m_browserHandleMap.insert({refHandle, a_browserName});

            a_outBrowser = browserIt->second.browser;
            if (a_funcInfoArr != nullptr)
            {
                for (std::uint32_t i = 0; i < a_funcInfoArrSize; ++i)
                {
                    if (!a_funcInfoArr[i]->callbackData.isEventFunction)
                    {
                        a_outBrowser->AddFunctionCallback(*a_funcInfoArr[i]);
                    }
                }
            }

            return refHandle;
        }

        const auto cefMenu = renderHost.GetSubMenu(a_browserName);
        if (cefMenu == nullptr)
        {
            Meridian::JS::JSEventFuncInfo a_eventFuncInfo{"", ""};
            auto jsFuncStorage = std::make_shared<Meridian::JS::JSFunctionStorage>();
            if (a_funcInfoArr != nullptr)
            {
                for (std::uint32_t i = 0; i < a_funcInfoArrSize; ++i)
                {
                    if (a_funcInfoArr[i]->callbackData.isEventFunction)
                    {
                        a_eventFuncInfo = Meridian::JS::JSEventFuncInfo::CreateFromFuncInfo(*a_funcInfoArr[i]);
                    }
                    else
                    {
                        jsFuncStorage->AddFunctionCallback(*a_funcInfoArr[i]);
                    }
                }
            }
            
            Meridian::UI::BrowserSettings ingestedBrowserSettings{};
            if (!Meridian::UI::IngestSettings(a_settings, Meridian::UI::kBrowserSettingsMinSize10, ingestedBrowserSettings))
            {
                spdlog::error("{}: caller BrowserSettings rejected — structSize below the 1.0 minimum (pre-1.0 layout or corrupt); refusing browser creation", NameOf(PublicAPIController));
                a_outBrowser = nullptr;
                return Meridian::UI::IUIPlatformAPI::InvalidBrowserRefHandle;
            }

            auto newCefMenu = Meridian::Services::UIPlatformService::GetSingleton().CreateCefMenu(jsFuncStorage, a_eventFuncInfo, m_settingsProvider);
            if (!newCefMenu->LoadBrowser(a_startUrl, m_settingsProvider->GetCefWindowInfo(), m_settingsProvider->MergeAndGetCefBrowserSettings(&ingestedBrowserSettings)))
            {
                spdlog::error("{}: failed to load browser ({}) with name \"{}\"", NameOf(PublicAPIController), a_startUrl, a_browserName);
                a_outBrowser = nullptr;
                return Meridian::UI::IUIPlatformAPI::InvalidBrowserRefHandle;
            }
            if (!renderHost.AddSubMenu(a_browserName, newCefMenu))
            {
                spdlog::error("{}: failed to add cef menu with name \"{}\"", NameOf(PublicAPIController), a_browserName);
                a_outBrowser = nullptr;
                return Meridian::UI::IUIPlatformAPI::InvalidBrowserRefHandle;
            }
            a_outBrowser = newCefMenu->GetBrowser().get();
        }
        else
        {
            if (cefMenu->GetMenuType() != Meridian::Menus::SubMenuType::CEFMenu)
            {
                spdlog::error("{}: trying to get not a browser menu", NameOf(PublicAPIController));
                a_outBrowser = nullptr;
                return Meridian::UI::IUIPlatformAPI::InvalidBrowserRefHandle;
            }

            a_outBrowser = std::static_pointer_cast<Meridian::Menus::CEFMenu>(cefMenu)->GetBrowser().get();
            if (a_funcInfoArr != nullptr)
            {
                for (std::uint32_t i = 0; i < a_funcInfoArrSize; ++i)
                {
                    if (!a_funcInfoArr[i]->callbackData.isEventFunction)
                    {
                        a_outBrowser->AddFunctionCallback(*a_funcInfoArr[i]);
                    }
                }
            }
        }

        const auto refHandle = m_currentRefHandle++;

        auto& handleData = m_browserNameMap.insert({a_browserName, {}}).first->second;
        handleData.browser = a_outBrowser;
        handleData.refHandles.insert(refHandle);

        m_browserHandleMap.insert({refHandle, a_browserName});

        return refHandle;
    }

    void PublicAPIController::RegisterOnShutdown(OnShutdownFunc_t a_callback)
    {
        if (a_callback == nullptr)
        {
            return;
        }

        bool invokeNow = false;
        {
            std::lock_guard lock(m_onShutdownFuncsMutex);
            if (m_isShuttingDown.load(std::memory_order_acquire))
            {
                invokeNow = true;
            }
            else
            {
                m_onShutdownFuncs.push(a_callback);
            }
        }

        if (invokeNow)
        {
            a_callback();
        }
    }

#pragma endregion
}
