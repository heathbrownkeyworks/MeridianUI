#include "CEFPromiseFunctionHandler.h"
#include "CEF/MeridianSubprocessCefApp.h"

namespace Meridian::JS
{
    CEFPromiseFunctionHandler::CEFPromiseFunctionHandler(CefRefPtr<CefBrowser> a_browser, CefString a_objectName, Meridian::CEF::MeridianSubprocessCefApp* a_app)
    {
        if (a_browser == nullptr)
        {
            spdlog::error("{}: browser is nullptr", NameOf(CEFPromiseFunctionHandler));
        }

        if (a_app == nullptr)
        {
            spdlog::error("{}: app is nullptr", NameOf(CEFPromiseFunctionHandler));
        }

        m_browser = a_browser;
        m_objectName = a_objectName;
        m_app = a_app;
    }

    bool CEFPromiseFunctionHandler::Execute(const CefString& name,
                                            CefRefPtr<CefV8Value> object,
                                            const CefV8ValueList& arguments,
                                            CefRefPtr<CefV8Value>& retval,
                                            CefString& exception)
    {
        // Execute is invoked by V8 on the renderer thread, and Register()
        // below is PromiseRegistry's single-threaded contract -- see
        // MeridianSubprocessCefApp's m_promiseRegistry comment.
        CEF_REQUIRE_RENDERER_THREAD();

        const auto context = CefV8Context::GetCurrentContext();

        auto funcArgs = CefListValue::Create();

        std::vector<CefRefPtr<CefV8Value>> objectRefs;
        std::unordered_map<std::string, std::uint32_t> warnMap;
        CefString firstException;
        for (size_t i = 0; i < arguments.size(); ++i)
        {
            funcArgs->SetValue(static_cast<int32_t>(i), Meridian::Converters::CEFValueConverter::ConvertValue(arguments[i], objectRefs, warnMap, exception));

            if (!exception.empty())
            {
                spdlog::error("{}: {}", NameOf(CEFPromiseFunctionHandler::Execute), exception.ToString());
                if (firstException.empty())
                {
                    firstException = exception;
                }
                exception = "";
            }
        }

        exception = firstException;
        for (const auto& it : warnMap)
        {
            spdlog::warn("{} ({})", it.first.c_str(), it.second);
        }

        retval = CefV8Value::CreatePromise();
        if (retval == nullptr)
        {
            exception = "failed to create promise";
            return true;
        }

        const auto frameId = context->GetFrame()->GetIdentifier().ToString();
        const auto callId = m_app->Registry().Register(PendingPromise{retval, context}, frameId);

        auto message = CefProcessMessage::Create(IPC_JS_PROMISE_CALL);
        auto messageArgs = message->GetArgumentList();
        messageArgs->SetInt(0, callId);
        messageArgs->SetString(1, m_objectName);
        messageArgs->SetString(2, name);
        messageArgs->SetList(3, funcArgs);
        m_browser->GetMainFrame()->SendProcessMessage(PID_BROWSER, message);

        return true;
    }
}
