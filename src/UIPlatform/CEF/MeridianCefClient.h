#pragma once

#include "PCH.h"
#include "Render/CEFSyncCopyRenderLayer.h"
#include "Render/CEFCopyRenderLayer.h"
#include "Providers/ICEFSettingsProvider.h"
#include "Menus/LayerGeometryHolder.h"
#include "Render/CEFRingRenderLayer.h"
#include "CEF/BrowserContentPolicy.h"

namespace Meridian::CEF
{
    class MeridianCefClient : public CefClient,
                             public CefLifeSpanHandler,
                             public CefLoadHandler,
                             public CefJSDialogHandler,
                             public CefRequestHandler,
                             public CefResourceRequestHandler,
                             public CefDisplayHandler
    {
        IMPLEMENT_REFCOUNTING(MeridianCefClient);

    protected:
        CefRefPtr<Meridian::Render::IRenderLayer> m_cefRenderLayer = nullptr;
        CefRefPtr<CefRenderHandler> m_cefRenderHandler = nullptr;
        mutable std::mutex m_browserStateMutex;
        std::recursive_mutex m_callbackDispatchMutex;
        CefRefPtr<CefBrowser> m_cefBrowser = nullptr;
        bool m_quiesceRequested = false;
        bool m_closeRequested = false;
        std::atomic_bool m_cursorUpdatesEnabled{false};
        mutable std::mutex m_contentPolicyMutex;
        BrowserContentPolicy m_contentPolicy;

    public:
        MeridianCefClient(std::shared_ptr<Meridian::Providers::ICEFSettingsProvider> a_settingsProvider,
                          std::shared_ptr<Meridian::Menus::LayerGeometryHolder> a_geometryHolder);
        virtual ~MeridianCefClient() override = default;

        CefRefPtr<Meridian::Render::IRenderLayer> GetRenderLayer();
        CefRefPtr<CefBrowser> GetBrowser();
        bool IsBrowserReady();
        void BeginShutdown();
        void RequestClose();
        void EnableNativeBindings();
        bool AllowInitialNavigation(std::string_view a_url);
        bool CanExposeNativeBindings();
        void SetCursorUpdatesEnabled(bool a_enabled);

        sigslot::signal<CefRefPtr<CefProcessMessage>> onIPCMessageReceived;
        sigslot::signal<CefRefPtr<CefBrowser>> onAfterBrowserCreated;
        sigslot::signal<CefRefPtr<CefBrowser>> onBeforeBrowserClose;
        sigslot::signal<> onMainFrameLoadStart;
        sigslot::signal<> onMainFrameLoadEnd;

        // CefClient
        CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override;
        CefRefPtr<CefLoadHandler> GetLoadHandler() override;
        CefRefPtr<CefRenderHandler> GetRenderHandler() override;
        CefRefPtr<CefJSDialogHandler> GetJSDialogHandler() override;
        CefRefPtr<CefRequestHandler> GetRequestHandler() override;
        CefRefPtr<CefDisplayHandler> GetDisplayHandler() override;
        bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                      CefRefPtr<CefFrame> frame,
                                      CefProcessId source_process,
                                      CefRefPtr<CefProcessMessage> message) override;

        // CefLifeSpanHandler
        bool OnBeforePopup(CefRefPtr<CefBrowser> browser,
                           CefRefPtr<CefFrame> frame,
                           int popup_id,
                           const CefString& target_url,
                           const CefString& target_frame_name,
                           CefLifeSpanHandler::WindowOpenDisposition target_disposition,
                           bool user_gesture,
                           const CefPopupFeatures& popupFeatures,
                           CefWindowInfo& windowInfo,
                           CefRefPtr<CefClient>& client,
                           CefBrowserSettings& settings,
                           CefRefPtr<CefDictionaryValue>& extra_info,
                           bool* no_javascript_access) override;
        void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
        void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

        // CefRequestHandler
        bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
                            CefRefPtr<CefFrame> frame,
                            CefRefPtr<CefRequest> request,
                            bool user_gesture,
                            bool is_redirect) override;
        CefRefPtr<CefResourceRequestHandler> GetResourceRequestHandler(
            CefRefPtr<CefBrowser> browser,
            CefRefPtr<CefFrame> frame,
            CefRefPtr<CefRequest> request,
            bool is_navigation,
            bool is_download,
            const CefString& request_initiator,
            bool& disable_default_handling) override;

        // CefResourceRequestHandler
        CefResourceRequestHandler::ReturnValue OnBeforeResourceLoad(
            CefRefPtr<CefBrowser> browser,
            CefRefPtr<CefFrame> frame,
            CefRefPtr<CefRequest> request,
            CefRefPtr<CefCallback> callback) override;

        // CefLoadHandler
        void OnLoadStart(CefRefPtr<CefBrowser> browser,
                         CefRefPtr<CefFrame> frame,
                         TransitionType transition_type) override;
        void OnLoadEnd(CefRefPtr<CefBrowser> browser,
                       CefRefPtr<CefFrame> frame,
                       int httpStatusCode) override;
        virtual void OnLoadError(CefRefPtr<CefBrowser> browser,
                                 CefRefPtr<CefFrame> frame,
                                 ErrorCode errorCode,
                                 const CefString& errorText,
                                 const CefString& failedUrl) override;

        // CefJSDialogHandler
        virtual bool OnJSDialog(CefRefPtr<CefBrowser> browser,
                                const CefString& origin_url,
                                JSDialogType dialog_type,
                                const CefString& message_text,
                                const CefString& default_prompt_text,
                                CefRefPtr<CefJSDialogCallback> callback,
                                bool& suppress_message) override;

        void OnRenderProcessTerminated(CefRefPtr<CefBrowser> browser,
                                       TerminationStatus status,
                                       int error_code,
                                       const CefString& error_string) override;

        // CefDisplayHandler
        bool OnCursorChange(CefRefPtr<CefBrowser> browser,
                            CefCursorHandle cursor,
                            cef_cursor_type_t type,
                            const CefCursorInfo& custom_cursor_info) override;
    };
}
