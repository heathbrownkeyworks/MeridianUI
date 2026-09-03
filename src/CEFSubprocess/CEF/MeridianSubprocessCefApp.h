#pragma once

#define RENDER_PROCESS_TYPE "renderer"

#include "PCH.h"
#include "CEFV8ContextGuard.h"
#include "Log/IPCLogSink.hpp"
#include "JS/CEFFunctionQueue.h"
#include "JS/CEFFunctionHandler.h"
#include "JS/CEFEventFunctionHandler.h"
#include "JS/CEFPromiseFunctionHandler.h"
#include "JS/PromiseRegistry.h"
#include "Scheme/ModSchemeHandler.h"

namespace Meridian::CEF
{
    class MeridianSubprocessCefApp final : public CefApp,
                                          public CefRenderProcessHandler
    {
        IMPLEMENT_REFCOUNTING(MeridianSubprocessCefApp);

    private:
        std::shared_ptr<Meridian::Log::IPCLogSink_mt> m_logSink = nullptr;
        CefString m_processType;
        CefRefPtr<CefDictionaryValue> m_extraInfo = nullptr;
        bool m_browserCreatedMsgSent = false;

        // Renderer-side pending-promise bookkeeping. Touched only from
        // OnProcessMessageReceived / OnContextReleased / the promise V8
        // handler's Execute -- all of which CEF delivers on the renderer
        // thread, so per PromiseRegistry's single-threaded contract this
        // needs no locking. CEF_REQUIRE_RENDERER_THREAD() at the top of each
        // registry-touching method enforces that.
        Meridian::JS::PromiseRegistry<Meridian::JS::PendingPromise, std::string> m_promiseRegistry;

        void InitLog(CefRefPtr<CefBrowser> a_browser);

    public:
        MeridianSubprocessCefApp() = default;

        Meridian::JS::PromiseRegistry<Meridian::JS::PendingPromise, std::string>& Registry() { return m_promiseRegistry; }

        CefRefPtr<CefV8Value> GetOrCreateObject(CefRefPtr<CefV8Value> a_parent, const CefString& a_objectName);
        size_t AddFunctionHandlers(CefRefPtr<CefBrowser> a_browser,
                                   CefRefPtr<CefFrame> a_frame,
                                   CefProcessId a_sourceProcess,
                                   CefRefPtr<CefDictionaryValue> a_funcDict);
        size_t RemoveFunctionHandlers(CefRefPtr<CefBrowser> a_browser,
                                      CefRefPtr<CefFrame> a_frame,
                                      CefProcessId a_sourceProcess,
                                      CefRefPtr<CefDictionaryValue> a_funcDict);
        size_t AddPromiseFunctionHandlers(CefRefPtr<CefBrowser> a_browser,
                                          CefRefPtr<CefFrame> a_frame,
                                          CefProcessId a_sourceProcess,
                                          CefRefPtr<CefDictionaryValue> a_funcDict);
        size_t RemovePromiseFunctionHandlers(CefRefPtr<CefBrowser> a_browser,
                                             CefRefPtr<CefFrame> a_frame,
                                             CefProcessId a_sourceProcess,
                                             CefRefPtr<CefDictionaryValue> a_funcDict);

        // CefApp
        void OnBeforeCommandLineProcessing(CefString const& process_type, CefRefPtr<CefCommandLine> command_line) override;
        void OnRegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar) override;
        CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override;

        // CefRenderProcessHandler
        void OnBrowserCreated(CefRefPtr<CefBrowser> browser,
                              CefRefPtr<CefDictionaryValue> extra_info) override;
        void OnBrowserDestroyed(CefRefPtr<CefBrowser> browser) override;
        void OnContextCreated(CefRefPtr<CefBrowser> browser,
                              CefRefPtr<CefFrame> frame,
                              CefRefPtr<CefV8Context> context) override;
        void OnContextReleased(CefRefPtr<CefBrowser> browser,
                               CefRefPtr<CefFrame> frame,
                               CefRefPtr<CefV8Context> context) override;
        bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                      CefRefPtr<CefFrame> frame,
                                      CefProcessId source_process,
                                      CefRefPtr<CefProcessMessage> message) override;
    };
}
