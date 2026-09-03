#include "MeridianCefClient.h"

#include "Hooks/ShutdownHook.hpp"
#include "Services/CEFService.h"
#include "Services/UIPlatformService.h"
#include "Render/RenderHost.h"
#include "Render/CursorRenderer.h"
#include "Render/RendererSelection.h"

namespace Meridian::CEF
{
    MeridianCefClient::MeridianCefClient(std::shared_ptr<Meridian::Providers::ICEFSettingsProvider> a_settingsProvider,
                                         std::shared_ptr<Meridian::Menus::LayerGeometryHolder> a_geometryHolder)
    {
        const auto globalSettings = a_settingsProvider->GetGlobalSettings();
        auto rendererType = globalSettings.rendererType;
        m_contentPolicy.SetAllowRemoteContent(globalSettings.allowRemoteContent);

        if (rendererType == Meridian::UI::RendererType::RingBuffer)
        {
            const auto* renderData = Meridian::Render::RenderHost::GetSingleton().GetRenderData();
            const bool platformDeviceAvailable =
                renderData != nullptr && renderData->platformDevice != nullptr;
            const bool sharedKeyedTransportSupported =
                platformDeviceAvailable && renderData->platformDevice->SupportsSharedKeyedTransport();

            rendererType = Meridian::Render::ResolveBrowserRenderer(
                rendererType,
                platformDeviceAvailable,
                sharedKeyedTransportSupported);
            if (rendererType == Meridian::UI::RendererType::SyncCopy)
            {
                spdlog::warn(
                    "{}: {} unavailable on the active game adapter; falling back to SyncCopy",
                    NameOf(MeridianCefClient),
                    platformDeviceAvailable ? "shared keyed-texture transport" : "platform device");
            }
        }

        switch (rendererType)
        {
        case Meridian::UI::RendererType::RingBuffer: {
            auto renderLayer = new Meridian::Render::CEFRingRenderLayer(std::move(a_geometryHolder));
            m_cefRenderLayer = CefRefPtr<Meridian::Render::IRenderLayer>(renderLayer);
            m_cefRenderHandler = CefRefPtr<CefRenderHandler>(static_cast<CefRenderHandler*>(renderLayer));
            break;
        }
        case Meridian::UI::RendererType::DeferredContext: {
            using RenderLayer = Meridian::Render::CEFCopyRenderLayer;
            static_assert(std::is_base_of_v<CefRenderHandler, RenderLayer>);

            auto renderLayer = new RenderLayer();
            m_cefRenderLayer = CefRefPtr<Meridian::Render::IRenderLayer>(renderLayer);
            m_cefRenderHandler = CefRefPtr<CefRenderHandler>(static_cast<CefRenderHandler*>(renderLayer));
            break;
        }
        case Meridian::UI::RendererType::SyncCopy:
        default: {
            using RenderLayer = Meridian::Render::CEFSyncCopyRenderLayer;
            static_assert(std::is_base_of_v<CefRenderHandler, RenderLayer>);

            auto renderLayer = new RenderLayer(std::move(a_geometryHolder));
            m_cefRenderLayer = CefRefPtr<Meridian::Render::IRenderLayer>(renderLayer);
            m_cefRenderHandler = CefRefPtr<CefRenderHandler>(static_cast<CefRenderHandler*>(renderLayer));
            break;
        }
        }
    }

    CefRefPtr<Meridian::Render::IRenderLayer> MeridianCefClient::GetRenderLayer()
    {
        return m_cefRenderLayer;
    }

    CefRefPtr<CefBrowser> MeridianCefClient::GetBrowser()
    {
        std::lock_guard lock(m_browserStateMutex);
        return m_cefBrowser;
    }

    bool MeridianCefClient::IsBrowserReady()
    {
        return GetBrowser() != nullptr;
    }

    void MeridianCefClient::EnableNativeBindings()
    {
        std::string currentUrl;
        const auto browser = GetBrowser();
        const auto frame = browser != nullptr ? browser->GetMainFrame() : nullptr;
        if (frame != nullptr)
        {
            currentUrl = frame->GetURL().ToString();
        }

        std::lock_guard lock(m_contentPolicyMutex);
        m_contentPolicy.EnableNativeBindings(currentUrl);
    }

    bool MeridianCefClient::AllowInitialNavigation(std::string_view a_url)
    {
        std::lock_guard lock(m_contentPolicyMutex);
        return m_contentPolicy.AllowNavigation(a_url);
    }

    bool MeridianCefClient::CanExposeNativeBindings()
    {
        const auto browser = GetBrowser();
        const auto frame = browser != nullptr ? browser->GetMainFrame() : nullptr;
        if (frame == nullptr)
        {
            return false;
        }

        std::lock_guard lock(m_contentPolicyMutex);
        return m_contentPolicy.IsTrusted(frame->GetURL().ToString());
    }

    void MeridianCefClient::SetCursorUpdatesEnabled(bool a_enabled)
    {
        m_cursorUpdatesEnabled.store(a_enabled, std::memory_order_release);
    }

    void MeridianCefClient::BeginShutdown()
    {
        {
            std::lock_guard lock(m_browserStateMutex);
            m_quiesceRequested = true;
        }

        // Wait for an admitted callback to return without holding browser state.
        // The recursive mutex permits a callback to request its own closure.
        {
            std::lock_guard dispatchLock(m_callbackDispatchMutex);
        }

        if (m_cefRenderLayer != nullptr)
        {
            m_cefRenderLayer->BeginShutdown();
        }
    }

    void MeridianCefClient::RequestClose()
    {
        {
            std::lock_guard lock(m_browserStateMutex);
            m_closeRequested = true;
        }

        BeginShutdown();
        Meridian::Services::CEFService::RequestClientClose(CefRefPtr<CefClient>(this));
    }

    CefRefPtr<CefLifeSpanHandler> MeridianCefClient::GetLifeSpanHandler()
    {
        return this;
    }

    CefRefPtr<CefLoadHandler> MeridianCefClient::GetLoadHandler()
    {
        return this;
    }

    CefRefPtr<CefRenderHandler> MeridianCefClient::GetRenderHandler()
    {
        return m_cefRenderHandler;
    }

    CefRefPtr<CefJSDialogHandler> MeridianCefClient::GetJSDialogHandler()
    {
        return this;
    }

    CefRefPtr<CefRequestHandler> MeridianCefClient::GetRequestHandler()
    {
        return this;
    }

    CefRefPtr<CefDisplayHandler> MeridianCefClient::GetDisplayHandler()
    {
        return this;
    }

    bool MeridianCefClient::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                                    CefRefPtr<CefFrame> frame,
                                                    CefProcessId source_process,
                                                    CefRefPtr<CefProcessMessage> message)
    {
        {
            std::lock_guard dispatchLock(m_callbackDispatchMutex);
            {
                std::lock_guard stateLock(m_browserStateMutex);
                if (m_quiesceRequested ||
                    m_closeRequested ||
                    Meridian::Hooks::ShutdownHook::IsGameClosing.load(std::memory_order_acquire))
                {
                    return true;
                }
            }

            // Keep the dispatch gate locked through signal dispatch. RequestClose
            // either closes the state gate before this point or waits until the
            // active consumer callback has returned.
            onIPCMessageReceived(message);
        }
        return true;
    }

    bool MeridianCefClient::OnBeforePopup(CefRefPtr<CefBrowser> browser,
                                         CefRefPtr<CefFrame>,
                                         int popup_id,
                                         const CefString& target_url,
                                         const CefString&,
                                         CefLifeSpanHandler::WindowOpenDisposition,
                                         bool,
                                         const CefPopupFeatures&,
                                         CefWindowInfo&,
                                         CefRefPtr<CefClient>&,
                                         CefBrowserSettings&,
                                         CefRefPtr<CefDictionaryValue>&,
                                         bool*)
    {
        const auto openerId = browser == nullptr ? -1 : browser->GetIdentifier();
        spdlog::warn("{}: rejected popup {} from browser {} targeting \"{}\"",
                     NameOf(MeridianCefClient::OnBeforePopup),
                     popup_id,
                     openerId,
                     target_url.ToString());
        return true;
    }

    void MeridianCefClient::OnAfterCreated(CefRefPtr<CefBrowser> browser)
    {
        const auto client = CefRefPtr<CefClient>(this);
        const auto disposition = Meridian::Services::CEFService::OnBrowserCreated(client, browser);
        bool closeImmediately = disposition == Meridian::Services::CEFService::BrowserCreationDisposition::CloseImmediately;
        bool retainedDuringQuiesce = false;
        {
            std::lock_guard dispatchLock(m_callbackDispatchMutex);
            bool dispatchReady = false;
            {
                std::lock_guard stateLock(m_browserStateMutex);
                if (disposition == Meridian::Services::CEFService::BrowserCreationDisposition::KeepOpen)
                {
                    closeImmediately = m_closeRequested;
                    if (!m_closeRequested)
                    {
                        m_cefBrowser = browser;
                        retainedDuringQuiesce = m_quiesceRequested;
                        dispatchReady = !m_quiesceRequested;
                    }
                }
            }

            if (dispatchReady)
            {
                spdlog::info("{}: browser with id {} using \"{}\" render layer",
                             NameOf(MeridianCefClient::OnAfterCreated),
                             browser->GetIdentifier(),
                             m_cefRenderLayer->GetName());
                onAfterBrowserCreated(browser);
                return;
            }
        }

        if (retainedDuringQuiesce)
        {
            spdlog::info("{}: browser with id {} created while quiesced; suppressing readiness callbacks",
                         NameOf(MeridianCefClient::OnAfterCreated),
                         browser->GetIdentifier());
        }
        else
        {
            spdlog::info("{}: browser with id {} created during close; suppressing readiness callbacks",
                         NameOf(MeridianCefClient::OnAfterCreated),
                         browser->GetIdentifier());
        }

        if (closeImmediately && browser->IsValid())
        {
            browser->GetHost()->CloseBrowser(true);
        }
    }

    void MeridianCefClient::OnBeforeClose(CefRefPtr<CefBrowser> browser)
    {
        {
            std::lock_guard dispatchLock(m_callbackDispatchMutex);
            bool dispatchBeforeClose = false;
            {
                std::lock_guard stateLock(m_browserStateMutex);
                dispatchBeforeClose = !m_quiesceRequested;
                m_quiesceRequested = true;
                m_closeRequested = true;
                if (m_cefBrowser != nullptr && browser->IsSame(m_cefBrowser))
                {
                    m_cefBrowser = nullptr;
                }
            }

            if (dispatchBeforeClose)
            {
                onBeforeBrowserClose(browser);
            }
        }

        Meridian::Services::CEFService::OnBrowserBeforeClose(CefRefPtr<CefClient>(this), browser);
    }

    bool MeridianCefClient::OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                                           CefRefPtr<CefFrame> frame,
                                           CefRefPtr<CefRequest> request,
                                           bool user_gesture,
                                           bool is_redirect)
    {
        const auto url = request != nullptr ? request->GetURL().ToString() : std::string{};
        bool allowed = false;
        {
            std::lock_guard lock(m_contentPolicyMutex);
            allowed = m_contentPolicy.AllowNavigation(url);
        }

        if (!allowed)
        {
            spdlog::warn("{}: blocked {}navigation of {} frame to untrusted URL \"{}\"",
                         NameOf(MeridianCefClient::OnBeforeBrowse),
                         is_redirect ? "redirect " : "",
                         frame != nullptr && frame->IsMain() ? "main" : "sub",
                         url);
        }
        return !allowed;
    }

    CefRefPtr<CefResourceRequestHandler> MeridianCefClient::GetResourceRequestHandler(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefRefPtr<CefRequest> request,
        bool is_navigation,
        bool is_download,
        const CefString& request_initiator,
        bool& disable_default_handling)
    {
        return this;
    }

    CefResourceRequestHandler::ReturnValue MeridianCefClient::OnBeforeResourceLoad(
        CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        CefRefPtr<CefRequest> request,
        CefRefPtr<CefCallback> callback)
    {
        const auto url = request != nullptr ? request->GetURL().ToString() : std::string{};
        bool allowed = false;
        {
            std::lock_guard lock(m_contentPolicyMutex);
            allowed = m_contentPolicy.AllowResource(url);
        }

        if (!allowed)
        {
            spdlog::warn("{}: blocked untrusted resource URL \"{}\"", NameOf(MeridianCefClient::OnBeforeResourceLoad), url);
            return RV_CANCEL;
        }
        return RV_CONTINUE;
    }

    void MeridianCefClient::OnLoadStart(CefRefPtr<CefBrowser> browser,
                                       CefRefPtr<CefFrame> frame,
                                       TransitionType transition_type)
    {
        {
            std::lock_guard dispatchLock(m_callbackDispatchMutex);
            bool dispatch = false;
            {
                std::lock_guard stateLock(m_browserStateMutex);
                dispatch = !m_quiesceRequested &&
                           !m_closeRequested &&
                           m_cefBrowser != nullptr &&
                           browser->IsSame(m_cefBrowser) &&
                           frame->IsMain();
            }

            if (dispatch)
            {
                onMainFrameLoadStart();
            }
        }
    }

    void MeridianCefClient::OnLoadEnd(CefRefPtr<CefBrowser> browser,
                                     CefRefPtr<CefFrame> frame,
                                     int httpStatusCode)
    {
        // YES, httpStatusCode can be negative o_O
        {
            std::lock_guard dispatchLock(m_callbackDispatchMutex);
            bool dispatch = false;
            {
                std::lock_guard stateLock(m_browserStateMutex);
                dispatch = !m_quiesceRequested &&
                           !m_closeRequested &&
                           m_cefBrowser != nullptr &&
                           browser->IsSame(m_cefBrowser) &&
                           frame->IsMain() &&
                           httpStatusCode >= 0;
            }

            if (dispatch)
            {
                onMainFrameLoadEnd();
            }
        }
    }

    void MeridianCefClient::OnLoadError(CefRefPtr<CefBrowser> browser,
                                       CefRefPtr<CefFrame> frame,
                                       ErrorCode errorCode,
                                       const CefString& errorText,
                                       const CefString& failedUrl)
    {
        spdlog::error("MeridianCefClient::OnLoadError, url {}, text {}", failedUrl.ToString().data(), errorText.ToString().data());
    }

    bool MeridianCefClient::OnJSDialog(CefRefPtr<CefBrowser> browser,
                                      const CefString& origin_url,
                                      JSDialogType dialog_type,
                                      const CefString& message_text,
                                      const CefString& default_prompt_text,
                                      CefRefPtr<CefJSDialogCallback> callback,
                                      bool& suppress_message)
    {
        suppress_message = false;
        return false;
    }

    void MeridianCefClient::OnRenderProcessTerminated(CefRefPtr<CefBrowser> browser,
                                                      TerminationStatus status,
                                                      int error_code,
                                                      const CefString& error_string)
    {
        const auto browserId = browser == nullptr ? -1 : browser->GetIdentifier();
        spdlog::error("{}: browser {} renderer terminated (status={}, error_code={:#010x}, error=\"{}\")",
                      NameOf(MeridianCefClient::OnRenderProcessTerminated),
                      browserId,
                      static_cast<int>(status),
                      static_cast<std::uint32_t>(error_code),
                      error_string.ToString());
    }

    bool MeridianCefClient::OnCursorChange(CefRefPtr<CefBrowser> browser,
                                           CefCursorHandle cursor,
                                           cef_cursor_type_t type,
                                           const CefCursorInfo& custom_cursor_info)
    {
        if (!m_cursorUpdatesEnabled.load(std::memory_order_acquire))
        {
            return true;
        }
        Meridian::Render::CursorRenderer::GetSingleton().NotifyCursor(reinterpret_cast<HCURSOR>(cursor));
        return true;  // suppress CEF's own cursor handling (OSR: none anyway)
    }

}
