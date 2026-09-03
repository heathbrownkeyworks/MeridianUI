#pragma once

#include "PCH.h"
#include "Converters/CEFValueConverter.h"
#include "PromiseRegistry.h"

namespace Meridian::CEF
{
    class MeridianSubprocessCefApp;
}

namespace Meridian::JS
{
    /// <summary>
    /// A pending JS promise awaiting IPC_JS_PROMISE_RESULT. The context rides
    /// alongside the promise value itself so the RESULT handler can re-enter
    /// the correct V8 context to resolve/reject it. TContextKey for the
    /// registry is a std::string (the owning frame's identifier) rather than
    /// CefRefPtr<CefV8Context> -- CefRefPtr pointer-equality is not stable
    /// V8-context identity (different wrapper objects can reference the same
    /// context; IsSame() is the real equality), and a frame identifier is
    /// stable and cheap to compare/hash.
    /// </summary>
    struct PendingPromise
    {
        CefRefPtr<CefV8Value> promise;
        CefRefPtr<CefV8Context> context;
    };

    class CEFPromiseFunctionHandler : public CefV8Handler
    {
        IMPLEMENT_REFCOUNTING(CEFPromiseFunctionHandler);

    protected:
        CefRefPtr<CefBrowser> m_browser = nullptr;
        CefString m_objectName = "";
        Meridian::CEF::MeridianSubprocessCefApp* m_app = nullptr;

    public:
        CEFPromiseFunctionHandler(CefRefPtr<CefBrowser> a_browser, CefString a_objectName, Meridian::CEF::MeridianSubprocessCefApp* a_app);

        // CefV8Handler
        bool Execute(const CefString& name,
                     CefRefPtr<CefV8Value> object,
                     const CefV8ValueList& arguments,
                     CefRefPtr<CefV8Value>& retval,
                     CefString& exception) override;
    };
}
