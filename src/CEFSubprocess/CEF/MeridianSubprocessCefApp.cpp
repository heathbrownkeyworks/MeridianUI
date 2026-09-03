#include "MeridianSubprocessCefApp.h"
#include "IPC/JsBindingMessage.h"
#include <include/cef_parser.h>

namespace Meridian::CEF
{
    void MeridianSubprocessCefApp::InitLog(CefRefPtr<CefBrowser> a_browser)
    {
        auto level = spdlog::level::info;
        m_logSink = std::make_shared<Meridian::Log::IPCLogSink_mt>(a_browser);
        auto logger = std::make_shared<spdlog::logger>("global log"s, m_logSink);

#ifdef _DEBUG
        level = spdlog::level::trace;
        logger->sinks().push_back(std::make_shared<spdlog::sinks::msvc_sink_mt>());
#endif

        logger->set_level(level);
        logger->flush_on(level);
        logger->set_pattern("[%T.%e] [%^%l%$] : %v"s);

        spdlog::set_default_logger(logger);
    }

    CefRefPtr<CefV8Value> MeridianSubprocessCefApp::GetOrCreateObject(CefRefPtr<CefV8Value> a_parent, const CefString& a_objectName)
    {
        auto object = a_parent->GetValue(a_objectName);
        if (object == nullptr || object->IsNull() || object->IsUndefined())
        {
            object = CefV8Value::CreateObject(nullptr, nullptr);
            a_parent->SetValue(a_objectName, object, V8_PROPERTY_ATTRIBUTE_NONE);
        }

        return object;
    }

    size_t MeridianSubprocessCefApp::AddFunctionHandlers(CefRefPtr<CefBrowser> a_browser,
                                                        CefRefPtr<CefFrame> a_frame,
                                                        CefProcessId a_sourceProcess,
                                                        CefRefPtr<CefDictionaryValue> a_funcDict)
    {
        size_t addedFuncCount = 0;

        CEFV8ContextGuard v8ContextGuard(a_frame->GetV8Context());
        if (!v8ContextGuard.IsEntered())
        {
            spdlog::error("{}[{}]: can't enter v8 context", NameOf(MeridianSubprocessCefApp::AddFunctionHandlers), ::GetCurrentProcessId());
            return addedFuncCount;
        }

        std::vector<Meridian::JS::JsBindingMessage> messages;
        if (!Meridian::JS::FromCefDictionary(a_funcDict, messages))
        {
            spdlog::error("{}[{}]: failed to parse function dictionary", NameOf(MeridianSubprocessCefApp::AddFunctionHandlers), ::GetCurrentProcessId());
            return addedFuncCount;
        }

        const auto global = a_frame->GetV8Context()->GetGlobal();
        if (global == nullptr)
        {
            spdlog::error("{}[{}]: global object is nullptr", NameOf(MeridianSubprocessCefApp::AddFunctionHandlers), ::GetCurrentProcessId());
            return addedFuncCount;
        }

        for (const auto& message : messages)
        {
            const auto& objectName = message.DictionaryKey();
            if (objectName.empty())
            {
                spdlog::error("{}[{}]: object name is empty", NameOf(MeridianSubprocessCefApp::AddFunctionHandlers), ::GetCurrentProcessId());
                continue;
            }

            auto currentObjectValue = GetOrCreateObject(global, objectName);
            if (currentObjectValue == nullptr || currentObjectValue->IsNull() || currentObjectValue->IsUndefined() || !currentObjectValue->IsObject())
            {
                spdlog::error("{}[{}]: can't get or create object \"{}\"",
                              NameOf(MeridianSubprocessCefApp::AddFunctionHandlers),
                              ::GetCurrentProcessId(),
                              objectName.c_str());
                continue;
            }

            for (const auto& funcName : message.DictionaryValues())
            {
                if (funcName.empty())
                {
                    spdlog::warn("{}[{}]: function name is empty, skipping", NameOf(MeridianSubprocessCefApp::AddFunctionHandlers), ::GetCurrentProcessId());
                    continue;
                }

                CefRefPtr<Meridian::JS::CEFFunctionHandler> funcHandler = new Meridian::JS::CEFFunctionHandler(a_browser, objectName);
                CefRefPtr<CefV8Value> funcValue = CefV8Value::CreateFunction(funcName, funcHandler);
                if (funcValue == nullptr)
                {
                    spdlog::error("{}[{}]: failed to create function \"{}\"",
                                  NameOf(MeridianSubprocessCefApp::AddFunctionHandlers),
                                  ::GetCurrentProcessId(),
                                  funcName.c_str());
                    continue;
                }
                if (!currentObjectValue->SetValue(funcName, funcValue, V8_PROPERTY_ATTRIBUTE_NONE))
                {
                    spdlog::error("{}[{}]: failed to set function \"{}\" to object \"{}\"",
                                  NameOf(MeridianSubprocessCefApp::AddFunctionHandlers),
                                  ::GetCurrentProcessId(),
                                  funcName.c_str(),
                                  objectName.c_str());
                    continue;
                }
                ++addedFuncCount;
            }
        }

        return addedFuncCount;
    }

    size_t MeridianSubprocessCefApp::RemoveFunctionHandlers(CefRefPtr<CefBrowser> a_browser,
                                                           CefRefPtr<CefFrame> a_frame,
                                                           CefProcessId a_sourceProcess,
                                                           CefRefPtr<CefDictionaryValue> a_funcDict)
    {
        size_t removedFuncCount = 0;

        const auto v8Context = a_frame->GetV8Context();
        if (!v8Context->Enter())
        {
            spdlog::error("{}[{}]: can't enter v8 context", NameOf(MeridianSubprocessCefApp::RemoveFunctionHandlers), ::GetCurrentProcessId());
            return removedFuncCount;
        }

        std::vector<Meridian::JS::JsBindingMessage> messages;
        if (!Meridian::JS::FromCefDictionary(a_funcDict, messages))
        {
            spdlog::error("{}[{}]: failed to parse function dictionary", NameOf(MeridianSubprocessCefApp::RemoveFunctionHandlers), ::GetCurrentProcessId());
            v8Context->Exit();
            return removedFuncCount;
        }

        for (const auto& message : messages)
        {
            const auto& objectName = message.DictionaryKey();
            auto currentObjectValue = v8Context->GetGlobal();

            if (!objectName.empty())
            {
                auto objectValue = currentObjectValue->GetValue(objectName);
                if (objectValue != nullptr && objectValue->IsObject())
                {
                    currentObjectValue = objectValue;
                }
            }

            for (const auto& funcName : message.DictionaryValues())
            {
                if (funcName.empty())
                {
                    continue;
                }

                // Note: the window object does not allow deleting a custom function for some reason. Use different object.
                if (currentObjectValue->DeleteValue(funcName))
                {
                    ++removedFuncCount;
                }
            }
        }

        v8Context->Exit();
        return removedFuncCount;
    }

    size_t MeridianSubprocessCefApp::AddPromiseFunctionHandlers(CefRefPtr<CefBrowser> a_browser,
                                                               CefRefPtr<CefFrame> a_frame,
                                                               CefProcessId a_sourceProcess,
                                                               CefRefPtr<CefDictionaryValue> a_funcDict)
    {
        size_t addedFuncCount = 0;

        CEFV8ContextGuard v8ContextGuard(a_frame->GetV8Context());
        if (!v8ContextGuard.IsEntered())
        {
            spdlog::error("{}[{}]: can't enter v8 context", NameOf(MeridianSubprocessCefApp::AddPromiseFunctionHandlers), ::GetCurrentProcessId());
            return addedFuncCount;
        }

        std::vector<Meridian::JS::JsBindingMessage> messages;
        if (!Meridian::JS::FromCefDictionary(a_funcDict, messages))
        {
            spdlog::error("{}[{}]: failed to parse function dictionary", NameOf(MeridianSubprocessCefApp::AddPromiseFunctionHandlers), ::GetCurrentProcessId());
            return addedFuncCount;
        }

        const auto global = a_frame->GetV8Context()->GetGlobal();
        if (global == nullptr)
        {
            spdlog::error("{}[{}]: global object is nullptr", NameOf(MeridianSubprocessCefApp::AddPromiseFunctionHandlers), ::GetCurrentProcessId());
            return addedFuncCount;
        }

        for (const auto& message : messages)
        {
            const auto& objectName = message.DictionaryKey();
            if (objectName.empty())
            {
                spdlog::error("{}[{}]: object name is empty", NameOf(MeridianSubprocessCefApp::AddPromiseFunctionHandlers), ::GetCurrentProcessId());
                continue;
            }

            auto currentObjectValue = GetOrCreateObject(global, objectName);
            if (currentObjectValue == nullptr || currentObjectValue->IsNull() || currentObjectValue->IsUndefined() || !currentObjectValue->IsObject())
            {
                spdlog::error("{}[{}]: can't get or create object \"{}\"",
                              NameOf(MeridianSubprocessCefApp::AddPromiseFunctionHandlers),
                              ::GetCurrentProcessId(),
                              objectName.c_str());
                continue;
            }

            for (const auto& funcName : message.DictionaryValues())
            {
                if (funcName.empty())
                {
                    spdlog::warn("{}[{}]: function name is empty, skipping", NameOf(MeridianSubprocessCefApp::AddPromiseFunctionHandlers), ::GetCurrentProcessId());
                    continue;
                }

                CefRefPtr<Meridian::JS::CEFPromiseFunctionHandler> funcHandler = new Meridian::JS::CEFPromiseFunctionHandler(a_browser, objectName, this);
                CefRefPtr<CefV8Value> funcValue = CefV8Value::CreateFunction(funcName, funcHandler);
                if (funcValue == nullptr)
                {
                    spdlog::error("{}[{}]: failed to create function \"{}\"",
                                  NameOf(MeridianSubprocessCefApp::AddPromiseFunctionHandlers),
                                  ::GetCurrentProcessId(),
                                  funcName.c_str());
                    continue;
                }
                if (!currentObjectValue->SetValue(funcName, funcValue, V8_PROPERTY_ATTRIBUTE_NONE))
                {
                    spdlog::error("{}[{}]: failed to set function \"{}\" to object \"{}\"",
                                  NameOf(MeridianSubprocessCefApp::AddPromiseFunctionHandlers),
                                  ::GetCurrentProcessId(),
                                  funcName.c_str(),
                                  objectName.c_str());
                    continue;
                }
                ++addedFuncCount;
            }
        }

        return addedFuncCount;
    }

    size_t MeridianSubprocessCefApp::RemovePromiseFunctionHandlers(CefRefPtr<CefBrowser> a_browser,
                                                                  CefRefPtr<CefFrame> a_frame,
                                                                  CefProcessId a_sourceProcess,
                                                                  CefRefPtr<CefDictionaryValue> a_funcDict)
    {
        size_t removedFuncCount = 0;

        const auto v8Context = a_frame->GetV8Context();
        if (!v8Context->Enter())
        {
            spdlog::error("{}[{}]: can't enter v8 context", NameOf(MeridianSubprocessCefApp::RemovePromiseFunctionHandlers), ::GetCurrentProcessId());
            return removedFuncCount;
        }

        std::vector<Meridian::JS::JsBindingMessage> messages;
        if (!Meridian::JS::FromCefDictionary(a_funcDict, messages))
        {
            spdlog::error("{}[{}]: failed to parse function dictionary", NameOf(MeridianSubprocessCefApp::RemovePromiseFunctionHandlers), ::GetCurrentProcessId());
            v8Context->Exit();
            return removedFuncCount;
        }

        for (const auto& message : messages)
        {
            const auto& objectName = message.DictionaryKey();
            auto currentObjectValue = v8Context->GetGlobal();

            if (!objectName.empty())
            {
                auto objectValue = currentObjectValue->GetValue(objectName);
                if (objectValue != nullptr && objectValue->IsObject())
                {
                    currentObjectValue = objectValue;
                }
            }

            for (const auto& funcName : message.DictionaryValues())
            {
                if (funcName.empty())
                {
                    continue;
                }

                // Note: the window object does not allow deleting a custom function for some reason. Use different object.
                if (currentObjectValue->DeleteValue(funcName))
                {
                    ++removedFuncCount;
                }
            }
        }

        v8Context->Exit();
        return removedFuncCount;
    }

    void MeridianSubprocessCefApp::OnBeforeCommandLineProcessing(CefString const& process_type,
                                                                CefRefPtr<CefCommandLine> command_line)
    {
        m_processType = process_type;
        InitLog(nullptr);
    }

    void MeridianSubprocessCefApp::OnRegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar)
    {
        Meridian::Scheme::RegisterModScheme(registrar);
    }

    CefRefPtr<CefRenderProcessHandler> MeridianSubprocessCefApp::GetRenderProcessHandler()
    {
        return this;
    }

    void MeridianSubprocessCefApp::OnBrowserCreated(CefRefPtr<CefBrowser> browser,
                                                   CefRefPtr<CefDictionaryValue> extra_info)
    {
        m_logSink->SetBrowser(browser);
        m_extraInfo = extra_info;

        if (!m_browserCreatedMsgSent)
        {
            spdlog::info("{}[{}]: browser created with id {}, using CEF {}", NameOf(MeridianSubprocessCefApp), ::GetCurrentProcessId(), browser->GetIdentifier(), CEF_VERSION);
            m_browserCreatedMsgSent = true;
        }
    }

    void MeridianSubprocessCefApp::OnBrowserDestroyed(CefRefPtr<CefBrowser> browser)
    {
        m_logSink->SetBrowser(nullptr);
        m_extraInfo = nullptr;
    }

    void MeridianSubprocessCefApp::OnContextCreated(CefRefPtr<CefBrowser> browser,
                                                   CefRefPtr<CefFrame> frame,
                                                   CefRefPtr<CefV8Context> context)
    {
        spdlog::default_logger()->flush();

        if (frame->IsMain())
        {
            spdlog::info("{}[{}]: main context with id {} created in browser id {}", NameOf(MeridianSubprocessCefApp), ::GetCurrentProcessId(), frame->GetIdentifier().ToString().data(), browser->GetIdentifier());

            auto message = CefProcessMessage::Create(IPC_JS_CONTEXT_CREATED);
            frame->SendProcessMessage(PID_BROWSER, message);

            // Event name func
            const auto evenFuncInfo = m_extraInfo->GetList(IPC_JS_EVENT_FUNCTION_ADD_NAME);
            if (evenFuncInfo != nullptr && evenFuncInfo->GetSize() > 1)
            {
                const auto objectName = evenFuncInfo->GetString(0);
                const auto funcName = evenFuncInfo->GetString(1);
                if (!funcName.empty())
                {
                    CEFV8ContextGuard v8ContextGuard(frame->GetV8Context());
                    if (!v8ContextGuard.IsEntered())
                    {
                        spdlog::error("{}[{}]: can't enter v8 context", NameOf(MeridianSubprocessCefApp::OnContextCreated), ::GetCurrentProcessId());
                        return;
                    }

                    auto currentObjectValue = frame->GetV8Context()->GetGlobal();
                    if (!objectName.empty())
                    {
                        currentObjectValue = GetOrCreateObject(currentObjectValue, objectName);
                    }

                    CefRefPtr<Meridian::JS::CEFEventFunctionHandler> funcHandler = new Meridian::JS::CEFEventFunctionHandler();
                    CefRefPtr<CefV8Value> funcValue = CefV8Value::CreateFunction(funcName, funcHandler);
                    currentObjectValue->SetValue(funcName, funcValue, V8_PROPERTY_ATTRIBUTE_NONE);
                }
            }
        }
    }

    void MeridianSubprocessCefApp::OnContextReleased(CefRefPtr<CefBrowser> browser,
                                                    CefRefPtr<CefFrame> frame,
                                                    CefRefPtr<CefV8Context> context)
    {
        CEF_REQUIRE_RENDERER_THREAD();

        // Drain and reject every promise still pending for this frame's
        // context before it goes away -- otherwise a promise whose
        // IPC_JS_PROMISE_RESULT never arrives (browser-side handler gone,
        // frame navigated away mid-call, etc.) would leak and the calling JS
        // would hang forever. The context is still valid here (OnContextReleased
        // fires just before teardown), so it can still be entered to settle it.
        //
        // This drain assumes CEF releases a frame's old V8 context before
        // creating its next one (true through CEF 141, the version this
        // pins to). If a future CEF bump reorders that — creating the new
        // context before releasing the old — this drain would run after the
        // new context's registrations already exist and would incorrectly
        // reject them too.
        const auto frameId = frame->GetIdentifier().ToString();
        for (auto& entry : m_promiseRegistry.DrainContext(frameId))
        {
            CEFV8ContextGuard v8ContextGuard(entry.promise.context);
            if (!v8ContextGuard.IsEntered())
            {
                // Context is already unusable -- V8 is going away with it,
                // so there's nothing left to settle. Just drop the entry.
                continue;
            }

            entry.promise.promise->RejectPromise("MeridianUI: context destroyed");
        }

        Meridian::JS::CEFEventFunctionHandler::RemoveEventFunc(context);
    }

    bool MeridianSubprocessCefApp::OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
                                                           CefRefPtr<CefFrame> frame,
                                                           CefProcessId source_process,
                                                           CefRefPtr<CefProcessMessage> message)
    {
        // CEF delivers renderer-process IPC on the renderer thread; the
        // IPC_JS_PROMISE_* branches below touch m_promiseRegistry, which is
        // single-threaded by contract (see the member's declaration comment).
        CEF_REQUIRE_RENDERER_THREAD();

        auto isMessageHandled = false;

        if (message->GetName() == IPC_JS_FUNCTION_ADD_EVENT)
        {
            const auto funcDict = message->GetArgumentList()->GetDictionary(0);
            if (funcDict == nullptr)
            {
                return true;
            }

            const auto addedFuncCount = AddFunctionHandlers(browser, frame, source_process, funcDict);
            spdlog::info("{}[{}]: registered {} functions for the browser with id {}", NameOf(MeridianSubprocessCefApp::OnProcessMessageReceived), ::GetCurrentProcessId(), addedFuncCount, browser->GetIdentifier());
            isMessageHandled = true;
        }
        else if (message->GetName() == IPC_JS_FUNCTION_REMOVE_EVENT)
        {
            const auto funcDict = message->GetArgumentList()->GetDictionary(0);
            if (funcDict == nullptr)
            {
                return true;
            }

            const auto addedFuncCount = RemoveFunctionHandlers(browser, frame, source_process, funcDict);
            spdlog::info("{}[{}]: removed {} functions for the browser with id {}", NameOf(MeridianSubprocessCefApp::OnProcessMessageReceived), ::GetCurrentProcessId(), addedFuncCount, browser->GetIdentifier());
            isMessageHandled = true;
        }
        else if (message->GetName() == IPC_JS_EVENT_FUNCTION_CALL_EVENT)
        {
            if (message->GetArgumentList()->GetSize() < 2)
            {
                return true;
            }

            Meridian::JS::CEFEventFunctionHandler::CallEventFunc(message->GetArgumentList()->GetString(0), browser, message->GetArgumentList()->GetString(1));
            isMessageHandled = true;
        }
        else if (message->GetName() == IPC_JS_PROMISE_FUNCTION_ADD_EVENT)
        {
            const auto funcDict = message->GetArgumentList()->GetDictionary(0);
            if (funcDict == nullptr)
            {
                return true;
            }

            const auto addedFuncCount = AddPromiseFunctionHandlers(browser, frame, source_process, funcDict);
            spdlog::info("{}[{}]: registered {} promise functions for the browser with id {}", NameOf(MeridianSubprocessCefApp::OnProcessMessageReceived), ::GetCurrentProcessId(), addedFuncCount, browser->GetIdentifier());
            isMessageHandled = true;
        }
        else if (message->GetName() == IPC_JS_PROMISE_FUNCTION_REMOVE_EVENT)
        {
            // reserved: no 1.0 producer — IBrowser has no RemovePromiseFunctionCallback
            const auto funcDict = message->GetArgumentList()->GetDictionary(0);
            if (funcDict == nullptr)
            {
                return true;
            }

            const auto removedFuncCount = RemovePromiseFunctionHandlers(browser, frame, source_process, funcDict);
            spdlog::info("{}[{}]: removed {} promise functions for the browser with id {}", NameOf(MeridianSubprocessCefApp::OnProcessMessageReceived), ::GetCurrentProcessId(), removedFuncCount, browser->GetIdentifier());
            isMessageHandled = true;
        }
        else if (message->GetName() == IPC_JS_PROMISE_RESULT)
        {
            const auto ipcArgs = message->GetArgumentList();
            if (ipcArgs->GetSize() < 3)
            {
                return true;
            }

            const auto callId = ipcArgs->GetInt(0);
            const auto ok = ipcArgs->GetBool(1);
            const auto payload = ipcArgs->GetString(2);

            Meridian::JS::PromiseRegistry<Meridian::JS::PendingPromise, std::string>::Entry entry;
            if (!m_promiseRegistry.Take(callId, entry))
            {
                spdlog::debug("{}[{}]: no pending promise for call id {} (already settled or drained)", NameOf(MeridianSubprocessCefApp::OnProcessMessageReceived), ::GetCurrentProcessId(), callId);
                return true;
            }

            CEFV8ContextGuard v8ContextGuard(entry.promise.context);
            if (!v8ContextGuard.IsEntered())
            {
                spdlog::error("{}[{}]: can't enter v8 context to settle promise for call id {}", NameOf(MeridianSubprocessCefApp::OnProcessMessageReceived), ::GetCurrentProcessId(), callId);
                return true;
            }

            if (ok)
            {
                const auto parsed = CefParseJSON(payload, JSON_PARSER_ALLOW_TRAILING_COMMAS);
                if (parsed == nullptr)
                {
                    entry.promise.promise->RejectPromise("MeridianUI: unparseable resolve payload");
                }
                else
                {
                    entry.promise.promise->ResolvePromise(Meridian::Converters::CEFValueConverter::ConvertToV8(parsed));
                }
            }
            else
            {
                entry.promise.promise->RejectPromise(payload);
            }

            isMessageHandled = true;
        }

        return isMessageHandled;
    }
}
