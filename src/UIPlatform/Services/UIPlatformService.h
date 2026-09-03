#pragma once

#include "PCH.h"
#include "CEFService.h"
#include "Common/Singleton.h"
#include "Render/IRenderLayer.h"
#include "CEF/MeridianCefApp.h"
#include "CEF/DefaultBrowser.h"
#include "Menus/CEFMenu.h"
#include "Providers/CustomCEFSettingsProvider.h"
#include "JS/JSFunctionStorage.h"
#include "MeridianUIAPI/Settings.h"
#include "Services/InputRouter.h"

namespace Meridian::Services
{
    class UIPlatformService : public Meridian::Common::Singleton<UIPlatformService>
    {
    protected:
        friend class Meridian::Common::Singleton<UIPlatformService>;

        static inline std::mutex s_uipInitMutex;
        static inline std::atomic_bool s_isUIPInited{false};

        std::shared_ptr<spdlog::logger> m_logger = nullptr;
        std::atomic_bool m_isShuttingDown{false};

    public:
        UIPlatformService();
        ~UIPlatformService() override = default;

        bool IsInited();

        /// <summary>
        /// Init ui service and it's dependencies
        /// </summary>
        /// <returns></returns>
        bool Init(std::shared_ptr<spdlog::logger> a_logger,
                  std::shared_ptr<Meridian::Providers::ICEFSettingsProvider> a_settingsProvider);

        /// <summary>
        /// Init ui service with custom settings
        /// </summary>
        /// <returns></returns>
        bool InitAndShowMenuWithSettings(std::shared_ptr<Meridian::Providers::ICEFSettingsProvider> a_settingsProvider);

        /// <summary>
        /// Close ui service and it's dependencies
        /// </summary>
        void Shutdown();

        std::shared_ptr<Meridian::Menus::CEFMenu> CreateCefMenu(std::shared_ptr<Meridian::JS::JSFunctionStorage> a_funcStorage,
                                                          Meridian::JS::JSEventFuncInfo& a_eventFuncInfo,
                                                          std::shared_ptr<Meridian::Providers::ICEFSettingsProvider> a_settingsProvider);
    };
}
