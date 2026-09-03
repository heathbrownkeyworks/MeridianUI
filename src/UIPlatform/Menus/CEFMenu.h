#pragma once

#include "PCH.h"
#include "Render/IRenderLayer.h"
#include "Menus/ISubMenu.h"
#include "CEF/DefaultBrowser.h"
#include "CEF/MeridianCefClient.h"
#include "JS/JSFunctionStorage.h"
#include "JS/JSEventFuncInfo.h"
#include "Services/CEFService.h"
#include "Providers/ICEFSettingsProvider.h"
#include "Menus/LayerGeometryHolder.h"

namespace Meridian::Menus
{
    class CEFMenu : public ISubMenu
    {
        IMPLEMENT_REFCOUNTING(CEFMenu);

    protected:
        std::mutex m_startBrowserMutex;
        bool m_started = false;

        Meridian::JS::JSEventFuncInfo m_eventFuncInfo{};

        std::shared_ptr<Meridian::Menus::LayerGeometryHolder> m_geometryHolder;
        std::shared_ptr<spdlog::logger> m_logger = nullptr;
        std::shared_ptr<Meridian::JS::JSFunctionStorage> m_jsFuncStorage = nullptr;
        CefRefPtr<Meridian::Render::IRenderLayer> m_cefRenderLayer = nullptr;
        std::shared_ptr<Meridian::CEF::DefaultBrowser> m_browser = nullptr;

    public:
        CEFMenu(std::shared_ptr<spdlog::logger> a_logger,
                std::shared_ptr<Meridian::JS::JSFunctionStorage> a_jsFuncStorage,
                Meridian::JS::JSEventFuncInfo& a_eventFuncInfo,
                std::shared_ptr<Meridian::Providers::ICEFSettingsProvider> a_settingsProvider);
        ~CEFMenu() override;

        bool LoadBrowser(std::string_view a_url,
                         const CefWindowInfo& a_cefWindowInfo,
                         const CefBrowserSettings& a_cefBrowserSettings);
        std::shared_ptr<Meridian::CEF::IBrowser> GetBrowser();
        std::shared_ptr<Meridian::Menus::LayerGeometryHolder> GetGeometryHolder() const { return m_geometryHolder; }

        // Meridian::Render::IRenderLayer
        void Draw() override;
        void AfterDraw() override;
        void Init(Meridian::Render::RenderData* a_renderData) override;
        void SetVisible(bool a_visible) override;
        bool GetVisible() override;
        void BeginShutdown() override;

        // RE::MenuEventHandler
        bool CanProcess(RE::InputEvent* a_event) override;
        bool ProcessMouseMove(RE::MouseMoveEvent* a_event) override;
        bool ProcessButton(RE::ButtonEvent* a_event) override;

        // Meridian::Menus::ISubMenu
        SubMenuType GetMenuType() override;
        void OnResolutionChanged(int a_oldW, int a_oldH, int a_newW, int a_newH) override;
        bool ProcessToggleKeys(RE::ButtonEvent* a_event) override;
    };
}
