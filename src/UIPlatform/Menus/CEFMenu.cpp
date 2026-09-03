#include "CEFMenu.h"

namespace Meridian::Menus
{
    CEFMenu::CEFMenu(std::shared_ptr<spdlog::logger> a_logger,
                     std::shared_ptr<Meridian::JS::JSFunctionStorage> a_jsFuncStorage,
                     Meridian::JS::JSEventFuncInfo& a_eventFuncInfo,
                     std::shared_ptr<Meridian::Providers::ICEFSettingsProvider> a_settingsProvider)
    {
        ThrowIfNullptr(CEFMenu, a_logger);
        m_logger = a_logger;

        m_jsFuncStorage = a_jsFuncStorage == nullptr ? std::make_shared<Meridian::JS::JSFunctionStorage>() : a_jsFuncStorage;
        m_eventFuncInfo = a_eventFuncInfo;

        m_geometryHolder = std::make_shared<Meridian::Menus::LayerGeometryHolder>();

        const auto cefClient = CefRefPtr<Meridian::CEF::MeridianCefClient>(new Meridian::CEF::MeridianCefClient(a_settingsProvider, m_geometryHolder));
        m_browser = std::make_shared<Meridian::CEF::DefaultBrowser>(m_logger, cefClient, m_jsFuncStorage, m_geometryHolder);
        m_cefRenderLayer = m_browser->GetCefClient()->GetRenderLayer();
    }

    CEFMenu::~CEFMenu()
    {
        BeginShutdown();
        if (m_browser != nullptr)
        {
            m_browser->RequestClose();
        }
    }

    bool CEFMenu::LoadBrowser(std::string_view a_url,
                              const CefWindowInfo& a_cefWindowInfo,
                              const CefBrowserSettings& a_cefBrowserSettings)
    {
        std::lock_guard locker(m_startBrowserMutex);

        if (!m_started)
        {
            const bool hasNativeBindings = m_jsFuncStorage->GetSize() > 0 ||
                                           !m_eventFuncInfo.objectName.empty() ||
                                           !m_eventFuncInfo.funcName.empty();
            if (hasNativeBindings)
            {
                m_browser->GetCefClient()->EnableNativeBindings();
            }
            if (!m_browser->GetCefClient()->AllowInitialNavigation(a_url))
            {
                m_logger->error("{}: refused native-bound browser start URL \"{}\"", NameOf(CEFMenu), a_url);
                return false;
            }

            auto eventFuncInfo = CefListValue::Create();
            eventFuncInfo->SetString(0, m_eventFuncInfo.objectName);
            eventFuncInfo->SetString(1, m_eventFuncInfo.funcName);

            auto jsFuncInfo = CefDictionaryValue::Create();
            jsFuncInfo->SetList(IPC_JS_EVENT_FUNCTION_ADD_NAME, eventFuncInfo);

            const auto createBrowserResult =
                Meridian::Services::CEFService::CreateBrowser(m_browser->GetCefClient(),
                                                        jsFuncInfo,
                                                        CefString(a_url.data()),
                                                        a_cefWindowInfo,
                                                        a_cefBrowserSettings);
            if (!createBrowserResult)
            {
                m_logger->error("{}: failed to create browser", NameOf(CEFMenu));
                return false;
            }

            m_started = true;
        }
        else
        {
            m_browser->LoadBrowserURL(a_url.data());
        }

        return true;
    }

    std::shared_ptr<Meridian::CEF::IBrowser> CEFMenu::GetBrowser()
    {
        return m_browser;
    }

#pragma region Meridian::Render::IRenderLayer

    void CEFMenu::Draw()
    {
        m_cefRenderLayer->Draw();
    }

    void CEFMenu::AfterDraw()
    {
        m_cefRenderLayer->AfterDraw();
    }

    void CEFMenu::Init(Meridian::Render::RenderData* a_renderData)
    {
        IRenderLayer::Init(a_renderData);
        m_geometryHolder->SetFromResolution(static_cast<int>(a_renderData->width), static_cast<int>(a_renderData->height));
        m_cefRenderLayer->Init(a_renderData);
    }

    void CEFMenu::SetVisible(bool a_visible)
    {
        IRenderLayer::SetVisible(a_visible);
        m_cefRenderLayer->SetVisible(a_visible);
    }

    bool CEFMenu::GetVisible()
    {
        IRenderLayer::GetVisible();
        return m_cefRenderLayer->GetVisible();
    }

    void CEFMenu::BeginShutdown()
    {
        IRenderLayer::BeginShutdown();
        if (m_cefRenderLayer != nullptr)
        {
            m_cefRenderLayer->BeginShutdown();
        }
        if (m_browser != nullptr)
        {
            m_browser->BeginShutdown();
        }
    }

#pragma endregion

#pragma region RE::MenuEventHandler

    bool CEFMenu::CanProcess(RE::InputEvent* a_event)
    {
        return m_browser->CanProcess(a_event);
    }

    bool CEFMenu::ProcessMouseMove(RE::MouseMoveEvent* a_event)
    {
        return m_browser->ProcessMouseMove(a_event);
    }

    bool CEFMenu::ProcessButton(RE::ButtonEvent* a_event)
    {
        return m_browser->ProcessButton(a_event);
    }

#pragma endregion

    SubMenuType CEFMenu::GetMenuType()
    {
        return SubMenuType::CEFMenu;
    }

    void CEFMenu::OnResolutionChanged(int a_oldW, int a_oldH, int a_newW, int a_newH)
    {
        m_geometryHolder->Rescale(a_oldW, a_oldH, a_newW, a_newH);

        const auto browser = m_browser->GetCefClient()->GetBrowser();
        if (browser != nullptr)
        {
            browser->GetHost()->WasResized();
        }
    }

    bool CEFMenu::ProcessToggleKeys(RE::ButtonEvent* a_event)
    {
        if (m_browser == nullptr)
        {
            return false;
        }
        return m_browser->ProcessToggleKeys(a_event);
    }
}
