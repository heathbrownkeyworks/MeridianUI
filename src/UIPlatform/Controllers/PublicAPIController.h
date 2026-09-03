#pragma once

#include "PCH.h"
#include "Hooks/ShutdownHook.hpp"
#include "Hooks/CharGeneratorHook.h"
#include "Common/Singleton.h"
#include "Services/UIPlatformService.h"
#include "Providers/CustomCEFSettingsProvider.h"
#include "Converters/KeyInputConverter.h"

#include <thread>

namespace Meridian::Controllers
{
    using BrowserRefHandle = Meridian::UI::IUIPlatformAPI::BrowserRefHandle;

    class PublicAPIController : public Meridian::UI::IUIPlatformAPI,
                                public Meridian::Common::Singleton<PublicAPIController>

    {
    protected:
        struct BrowserHandleData
        {
            Meridian::CEF::IBrowser* browser = nullptr;
            std::unordered_set<BrowserRefHandle> refHandles;
        };

        sigslot::scoped_connection m_onShutdownConnection;
        std::mutex m_onShutdownFuncsMutex;
        std::queue<OnShutdownFunc_t> m_onShutdownFuncs;
        std::atomic_bool m_isShuttingDown{false};
        std::mutex m_lifecycleAdmissionMutex;

    public:
        // Meridian::UI::IUIPlatformAPI
        BrowserRefHandle __cdecl AddOrGetBrowser(const char* a_browserName,
                                                 Meridian::JS::JSFuncInfo* const* a_funcInfoArr,
                                                 const std::uint32_t a_funcInfoArrSize,
                                                 const char* a_startUrl,
                                                 Meridian::CEF::IBrowser*& a_outBrowser) override;

        void __cdecl ReleaseBrowserHandle(BrowserRefHandle a_handle) override;

        BrowserRefHandle __cdecl AddOrGetBrowser(const char* a_browserName,
                                                 Meridian::JS::JSFuncInfo* const* a_funcInfoArr,
                                                 const std::uint32_t a_funcInfoArrSize,
                                                 const char* a_startUrl,
                                                 Meridian::UI::BrowserSettings* a_settings,
                                                 Meridian::CEF::IBrowser*& a_outBrowser) override;

        void RegisterOnShutdown(OnShutdownFunc_t a_callback) override;

    protected:
        Meridian::UI::ResponseVersionMessage m_rvMessage{Meridian::UI::LibVersion::AS_INT, Meridian::UI::APIVersion::AS_INT};
        Meridian::UI::ResponseAPIMessage m_rAPIMessage{this};

        std::atomic<BrowserRefHandle> m_currentRefHandle{1};

        std::mutex m_mapMutex;
        std::unordered_map<std::string, BrowserHandleData> m_browserNameMap;
        std::unordered_map<BrowserRefHandle, std::string> m_browserHandleMap;
        std::unordered_set<std::string> m_closingBrowserNames;

        std::mutex m_initPlatformServiceMutex;
        std::thread::id m_applicationThreadId{};
        std::shared_ptr<Meridian::Providers::ICEFSettingsProvider> m_settingsProvider = nullptr;

    public:
        Meridian::UI::ResponseVersionMessage* GetVersionMessage();
        Meridian::UI::ResponseAPIMessage* GetAPIMessage();

        void Init();
        void Shutdown();
        bool InitIfNotPlatformService(const Meridian::UI::Settings* a_settings);
        void SetSettingsProvider(const Meridian::UI::Settings* a_settings);
        std::shared_ptr<Meridian::Providers::ICEFSettingsProvider> GetSettingsProvider();
    };
}
