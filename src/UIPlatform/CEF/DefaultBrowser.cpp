#include "DefaultBrowser.h"
#include "IPC/JsBindingMessage.h"
#include "Render/CEFRingRenderLayer.h"
#include "Menus/FocusArbiter.h"
#include "Common/InputChord.h"
#include <unordered_map>

namespace Meridian::CEF
{
    DefaultBrowser::DefaultBrowser(std::shared_ptr<spdlog::logger> a_logger,
                                   CefRefPtr<MeridianCefClient> a_cefClient,
                                   std::shared_ptr<Meridian::JS::JSFunctionStorage> a_jsFuncStorage,
                                   std::shared_ptr<Meridian::Menus::LayerGeometryHolder> a_geometryHolder)
    {
        ThrowIfNullptr(DefaultBrowser, a_logger);
        m_logger = a_logger;

        ThrowIfNullptr(DefaultBrowser, a_cefClient);
        m_cefClient = a_cefClient;

        ThrowIfNullptr(DefaultBrowser, a_jsFuncStorage);
        m_jsFuncStorage = a_jsFuncStorage;

        ThrowIfNullptr(DefaultBrowser, a_geometryHolder);
        m_geometryHolder = a_geometryHolder;

        ZeroMemory(&m_lastCefMouseEvent, sizeof(CefMouseEvent));

        m_keyInputConverter.OnKeyDown.connect([this](CefKeyEvent& a_keyEvent) {
            SendKeyEventIfAlive(a_keyEvent);
        });

        m_keyInputConverter.OnKeyUp.connect([this](CefKeyEvent& a_keyEvent) {
            SendKeyEventIfAlive(a_keyEvent);
        });

        m_keyInputConverter.OnChar.connect([this](CefKeyEvent& a_keyEvent) {
            SendKeyEventIfAlive(a_keyEvent);
        });

        m_onWndInactive_Connection = Meridian::Hooks::WinProcHook::OnWindowInactive.connect([&](bool a_gameClosing) {
            if (!a_gameClosing)
            {
                m_keyInputConverter.ProcessAltTab();
            }
        });

        m_onIPCMessageReceived_Connection = m_cefClient->onIPCMessageReceived.connect([&, a_jsFuncStorage](CefRefPtr<CefProcessMessage> a_message) {
            if (a_message->GetName() == IPC_JS_FUNCTION_CALL_EVENT)
            {
                if (!m_cefClient->CanExposeNativeBindings())
                {
                    m_logger->warn("{}: ignored native function call from an untrusted document", NameOf(DefaultBrowser));
                    return;
                }
                const auto ipcArgs = a_message->GetArgumentList();
                const auto objName = ipcArgs->GetString(0).ToString();
                const auto funcName = ipcArgs->GetString(1).ToString();
                const auto argList = ipcArgs->GetList(2);

                const auto params = Meridian::Converters::CefValueToJSONConverter::ConvertToJSONStringArgs(argList);
                m_jsFuncStorage->ExecuteFunctionCallback(objName, funcName, params, a_jsFuncStorage);
            }
            else if (a_message->GetName() == IPC_JS_PROMISE_CALL)
            {
                if (!m_cefClient->CanExposeNativeBindings())
                {
                    m_logger->warn("{}: ignored native promise call from an untrusted document", NameOf(DefaultBrowser));
                    return;
                }
                const auto ipcArgs = a_message->GetArgumentList();
                const auto callId = ipcArgs->GetInt(0);
                const auto objName = ipcArgs->GetString(1).ToString();
                const auto funcName = ipcArgs->GetString(2).ToString();
                const auto argList = ipcArgs->GetList(3);

                const auto params = Meridian::Converters::CefValueToJSONConverter::ConvertToJSONStringArgs(argList);

                JSPromiseFuncCallbackData callbackData;
                bool found = false;
                {
                    std::lock_guard locker(m_urlMutex);
                    const auto it = m_jsPromiseFuncCallbackMap.find({objName, funcName});
                    if (it != m_jsPromiseFuncCallbackMap.end())
                    {
                        callbackData = it->second;
                        found = true;
                    }
                }

                const auto browser = m_cefClient->GetBrowser();

                if (!found || callbackData.callback == nullptr)
                {
                    spdlog::debug("{}: no promise binding for {}.{}", NameOf(DefaultBrowser), objName.c_str(), funcName.c_str());
                    if (browser != nullptr)
                    {
                        const auto frame = browser->GetMainFrame();
                        if (frame != nullptr)
                        {
                            auto resultMessage = CefProcessMessage::Create(IPC_JS_PROMISE_RESULT);
                            auto resultArgs = resultMessage->GetArgumentList();
                            resultArgs->SetInt(0, callId);
                            resultArgs->SetBool(1, false);
                            resultArgs->SetString(2, "no such binding");
                            frame->SendProcessMessage(CefProcessId::PID_RENDERER, resultMessage);
                        }
                    }
                    return;
                }

                // browser can be null here independent of `found` (the promise
                // callback map lookup above) — guard the GetIdentifier() call
                // rather than crash; AddRoute additionally drops a null browser
                // itself (warn + no-op), since nothing could ever settle to it.
                const int browserId = browser != nullptr ? browser->GetIdentifier() : 0;
                Meridian::JS::PromiseRouter::GetSingleton().AddRoute(browserId, callId, browser);
                auto* resolver = Meridian::JS::CreatePromiseResolver(browserId, callId);

                const auto callback = callbackData.callback;
                if (callbackData.executeInGameThread)
                {
                    SKSE::GetTaskInterface()->AddTask([callback, params, resolver]() {
                        auto argsCharArray = Meridian::Converters::CefValueToJSONConverter::ConvertToCharArray(params);
                        callback(argsCharArray.data(), static_cast<int>(argsCharArray.size()), resolver);
                    });
                }
                else
                {
                    auto argsCharArray = Meridian::Converters::CefValueToJSONConverter::ConvertToCharArray(params);
                    callback(argsCharArray.data(), static_cast<int>(argsCharArray.size()), resolver);
                }
            }
            else if (a_message->GetName() == IPC_LOG_EVENT)
            {
                const auto logger = spdlog::get(NL_UI_SUBPROC_NAME);
                if (logger != nullptr)
                {
                    auto argList = a_message->GetArgumentList();
                    logger->log(static_cast<spdlog::level::level_enum>(argList->GetInt(0)), argList->GetString(1).ToString().c_str());
                }
            }
        });

        m_onAfterBrowserCreated_Connection = m_cefClient->onAfterBrowserCreated.connect([&](CefRefPtr<CefBrowser> a_cefBrowser) {
            std::lock_guard locker(m_urlMutex);
            // load url
            if (m_isUrlCached)
            {
                LoadBrowserURL(m_urlCache.c_str(), m_clearJSFunctions);
            }

            // Async creation can miss a geometry change that arrived while
            // GetBrowser() was still null (NotifyGeometryChanged skips
            // WasResized then); re-sync unconditionally now.
            a_cefBrowser->GetHost()->WasResized();
        });

        m_onBeforeBrowserClose_Connection = m_cefClient->onBeforeBrowserClose.connect(
            [this](CefRefPtr<CefBrowser> a_browser) { OnCefBrowserBeforeClose(a_browser); });

        m_onMainFrameLoadStart_Connection = m_cefClient->onMainFrameLoadStart.connect([&]() {
            // A navigation destroys the old document and its focused element.
            // Do not keep Skyrim in text-entry mode while the replacement page
            // is loading; the new bootstrap will report its own active element.
            SetTextInputActive(false);
            std::lock_guard locker(m_urlMutex);
            m_isPageLoaded = false;

            // Add js func callbacks
            if (m_clearJSFunctions)
            {
                m_jsFuncStorage->ClearFunctionCallback();
            }
            else
            {
                const auto browser = m_cefClient->GetBrowser();
                if (browser != nullptr && m_jsFuncStorage->GetSize() > 0 && m_cefClient->CanExposeNativeBindings())
                {
                    auto cefMessage = CefProcessMessage::Create(IPC_JS_FUNCTION_ADD_EVENT);
                    cefMessage->GetArgumentList()->SetDictionary(0, m_jsFuncStorage->ConvertToCefDictionary());
                    browser->GetMainFrame()->SendProcessMessage(CefProcessId::PID_RENDERER, cefMessage);
                }
            }

            for (auto& funcInfo : m_jsFuncCallbackInfoCache)
            {
                AddFunctionCallbackAndSendMessage(funcInfo);
            }
            m_jsFuncCallbackInfoCache.clear();

            // Remove js func callbacks
            for (auto& funcInfo : m_jsFuncRemoveCache)
            {
                RemoveFunctionCallbackAndSendMessage(std::get<0>(funcInfo).c_str(), std::get<1>(funcInfo).c_str());
            }
            m_jsFuncRemoveCache.clear();

            // Add js promise func callbacks. Mirrors the void path above:
            // m_clearJSFunctions wipes only the already-sent registry
            // (m_jsPromiseFuncCallbackMap), never m_jsPromiseFuncCallbackInfoCache
            // — registrations still in flight (not yet sent) must survive a
            // reload's clear, exactly like m_jsFuncCallbackInfoCache does.
            if (m_clearJSFunctions)
            {
                m_jsPromiseFuncCallbackMap.clear();
            }
            else
            {
                const auto browser = m_cefClient->GetBrowser();
                if (browser != nullptr && !m_jsPromiseFuncCallbackMap.empty() && m_cefClient->CanExposeNativeBindings())
                {
                    std::unordered_map<std::string, Meridian::JS::JsBindingMessage> groupedByObject;
                    for (const auto& [key, data] : m_jsPromiseFuncCallbackMap)
                    {
                        auto& message = groupedByObject[key.first];
                        message.objectName = key.first;
                        message.funcNames.push_back(key.second);
                    }

                    std::vector<Meridian::JS::JsBindingMessage> messages;
                    messages.reserve(groupedByObject.size());
                    for (auto& [objectName, message] : groupedByObject)
                    {
                        messages.push_back(std::move(message));
                    }

                    // Re-adding an already-bound name here is a harmless
                    // no-op on the renderer side: binding add is a SetValue
                    // overwrite, not an accumulating registration.
                    auto cefMessage = CefProcessMessage::Create(IPC_JS_PROMISE_FUNCTION_ADD_EVENT);
                    cefMessage->GetArgumentList()->SetDictionary(0, Meridian::JS::ToCefDictionary(messages));
                    browser->GetMainFrame()->SendProcessMessage(CefProcessId::PID_RENDERER, cefMessage);
                }
            }

            // Replay not-yet-sent promise registrations unconditionally —
            // regardless of m_clearJSFunctions — exactly parallel to the
            // m_jsFuncCallbackInfoCache replay in onMainFrameLoadStart.
            for (auto& funcInfo : m_jsPromiseFuncCallbackInfoCache)
            {
                AddPromiseFunctionCallbackAndSendMessage(funcInfo);
            }
            m_jsPromiseFuncCallbackInfoCache.clear();
        });

        m_onMainFrameLoadEnd_Connection = m_cefClient->onMainFrameLoadEnd.connect([&]() {
            bool applyPendingFocus = false;
            bool pendingFocusValue = false;
            {
                std::lock_guard locker(m_urlMutex);
                m_isPageLoaded = true;

                // Flush function changes accumulated after load start.
                for (auto& funcInfo : m_jsFuncCallbackInfoCache)
                {
                    AddFunctionCallbackAndSendMessage(funcInfo);
                }
                m_jsFuncCallbackInfoCache.clear();

                for (auto& funcInfo : m_jsFuncRemoveCache)
                {
                    RemoveFunctionCallbackAndSendMessage(std::get<0>(funcInfo).c_str(), std::get<1>(funcInfo).c_str());
                }
                m_jsFuncRemoveCache.clear();

                // Flush promise registrations accumulated after load start (mid-
                // navigation AddPromiseFunctionCallback calls land in
                // m_jsPromiseFuncCallbackInfoCache since the page wasn't loaded
                // yet). Mirrors the void-binding flush above.
                for (auto& funcInfo : m_jsPromiseFuncCallbackInfoCache)
                {
                    AddPromiseFunctionCallbackAndSendMessage(funcInfo);
                }
                m_jsPromiseFuncCallbackInfoCache.clear();

                // JS exec scripts
                if (!m_jsExecCache.empty())
                {
                    ExecuteJavaScript(nullptr, nullptr);
                }

                // Meridian.View compatibility bindings must survive a page
                // reload. Ordered keys keep the bootstrap ahead of listener
                // shims on every main-frame load.
                for (const auto& [key, script] : m_persistentJS)
                {
                    ExecuteJavaScript(std::get<0>(script).c_str(), std::get<1>(script).c_str());
                }

                // Focus
                applyPendingFocus = m_isFocusedCached;
                pendingFocusValue = m_pendingFocusValue;
                m_isFocusedCached = false;
            }

            if (applyPendingFocus)
            {
                QueueFocusRequest(pendingFocusValue);
            }
        });
    }

    DefaultBrowser::~DefaultBrowser()
    {
        RequestClose();
        m_jsFuncStorage->ClearFunctionCallback();
    }

    void DefaultBrowser::SendKeyEventIfAlive(const CefKeyEvent& a_keyEvent)
    {
        if (m_shutdownStarted.load(std::memory_order_acquire))
        {
            return;
        }
        const auto browser = m_cefClient->GetBrowser();
        const auto host = browser != nullptr ? browser->GetHost() : nullptr;
        if (host != nullptr)
        {
            host->SendKeyEvent(a_keyEvent);
        }
    }

    void DefaultBrowser::OnCefBrowserBeforeClose(CefRefPtr<CefBrowser> a_browser)
    {
        m_shutdownStarted.store(true, std::memory_order_release);
        m_closeRequested.store(true, std::memory_order_release);
        m_cefClient->SetCursorUpdatesEnabled(false);
        Meridian::Menus::FocusArbiter::GetSingleton().Release(this);
        m_keyInputConverter.Clear();
        m_lastMouseInside = false;
        if (a_browser != nullptr)
        {
            Meridian::JS::PromiseRouter::GetSingleton().DropBrowserRoutes(a_browser->GetIdentifier());
        }
        std::lock_guard locker(m_urlMutex);
        m_isPageLoaded = false;
    }

    bool DefaultBrowser::ProcessToggleKeys(RE::ButtonEvent* a_event)
    {
        if (m_shutdownStarted.load(std::memory_order_acquire)) { return false; }

        if (!a_event->IsDown())
        {
            return false;
        }

        const auto keyboard = RE::BSInputDeviceManager::GetSingleton()->GetKeyboard();
        const std::uint8_t* keyboardState = keyboard == nullptr ? nullptr : keyboard->curState;
        const auto stateSize = keyboard == nullptr ? 0 : sizeof(keyboard->curState);

        bool fired = false;
        if (Meridian::Common::ChordSatisfied(m_toggleFocusKeyCode1, m_toggleFocusKeyCode2, keyboardState, stateSize))
        {
            SetBrowserFocused(!IsBrowserFocused());
            fired = true;
        }
        if (Meridian::Common::ChordSatisfied(m_toggleVisibleKeyCode1, m_toggleVisibleKeyCode2, keyboardState, stateSize))
        {
            SetBrowserVisible(!IsBrowserVisible());
            fired = true;
        }

        return fired;
    }

    CefRefPtr<MeridianCefClient> DefaultBrowser::GetCefClient()
    {
        return m_cefClient;
    }

    bool DefaultBrowser::IsReadyAndLog()
    {
        const auto result = IsBrowserReady();
        if (!result)
        {
            m_logger->info("{}: browser is still loading, try later", NameOf(DefaultBrowser));
        }
        return result;
    }

    void DefaultBrowser::BeginShutdown()
    {
        m_shutdownStarted.store(true, std::memory_order_release);

        Meridian::Menus::FocusArbiter::GetSingleton().Release(this);

        const auto browser = m_cefClient->GetBrowser();
        if (browser != nullptr)
        {
            Meridian::JS::PromiseRouter::GetSingleton().DropBrowserRoutes(browser->GetIdentifier());
        }

        m_cefClient->BeginShutdown();
        m_jsFuncStorage->DrainAndClearFunctionCallbacks();

        std::lock_guard locker(m_urlMutex);
        m_isPageLoaded = false;
    }

    void DefaultBrowser::RequestClose()
    {
        m_shutdownStarted.store(true, std::memory_order_release);

        const auto requestServiceClose = !m_closeRequested.exchange(true, std::memory_order_acq_rel);

        BeginShutdown();
        if (requestServiceClose)
        {
            m_cefClient->RequestClose();
        }
    }

    void DefaultBrowser::AddFunctionCallbackAndSendMessage(const Meridian::JS::JSFuncInfo& a_callbackInfo)
    {
        m_cefClient->EnableNativeBindings();
        m_jsFuncStorage->AddFunctionCallback(a_callbackInfo);
        const auto browser = m_cefClient->GetBrowser();
        if (browser != nullptr && m_cefClient->CanExposeNativeBindings())
        {
            auto cefMessage = CefProcessMessage::Create(IPC_JS_FUNCTION_ADD_EVENT);
            cefMessage->GetArgumentList()->SetDictionary(
                0,
                Meridian::JS::ToCefDictionary(Meridian::JS::MakeSingleBinding(a_callbackInfo.objectName, a_callbackInfo.funcName)));
            browser->GetMainFrame()->SendProcessMessage(CefProcessId::PID_RENDERER, cefMessage);
        }
    }

    void DefaultBrowser::RemoveFunctionCallbackAndSendMessage(const char* a_objectName, const char* a_funcName)
    {
        m_jsFuncStorage->RemoveFunctionCallback(a_objectName, a_funcName);
        const auto browser = m_cefClient->GetBrowser();
        if (browser != nullptr)
        {
            auto cefMessage = CefProcessMessage::Create(IPC_JS_FUNCTION_REMOVE_EVENT);
            cefMessage->GetArgumentList()->SetDictionary(
                0,
                Meridian::JS::ToCefDictionary(Meridian::JS::MakeSingleBinding(a_objectName, a_funcName)));
            browser->GetMainFrame()->SendProcessMessage(CefProcessId::PID_RENDERER, cefMessage);
        }
    }

    void DefaultBrowser::AddPromiseFunctionCallbackAndSendMessage(const Meridian::JS::JSPromiseFuncInfo& a_info)
    {
        m_cefClient->EnableNativeBindings();
        m_jsPromiseFuncCallbackMap[{a_info.objectName, a_info.funcName}] =
            JSPromiseFuncCallbackData{a_info.callback, a_info.executeInGameThread};
        const auto browser = m_cefClient->GetBrowser();
        if (browser != nullptr && m_cefClient->CanExposeNativeBindings())
        {
            auto cefMessage = CefProcessMessage::Create(IPC_JS_PROMISE_FUNCTION_ADD_EVENT);
            cefMessage->GetArgumentList()->SetDictionary(
                0,
                Meridian::JS::ToCefDictionary(Meridian::JS::MakeSingleBinding(a_info.objectName, a_info.funcName)));
            browser->GetMainFrame()->SendProcessMessage(CefProcessId::PID_RENDERER, cefMessage);
        }
    }

#pragma region IBrowser

    bool __cdecl DefaultBrowser::IsBrowserReady()
    {
        return m_cefClient != nullptr && m_cefClient->IsBrowserReady();
    }

    bool __cdecl DefaultBrowser::IsPageLoaded()
    {
        std::lock_guard locker(m_urlMutex);
        return m_isPageLoaded;
    }

    void __cdecl DefaultBrowser::SetBrowserVisible(bool a_value)
    {
        m_cefClient->GetRenderLayer()->SetVisible(a_value);
        if (!a_value)
        {
            SetBrowserFocused(false);
        }
    }

    bool __cdecl DefaultBrowser::IsBrowserVisible()
    {
        return m_cefClient->GetRenderLayer()->GetVisible();
    }

    void __cdecl DefaultBrowser::ToggleBrowserVisibleByKeys(const std::uint32_t a_keyCode1, const std::uint32_t a_keyCode2)
    {
        m_toggleVisibleKeyCode1 = a_keyCode1 < sizeof(RE::BSInputDeviceManager::GetSingleton()->GetKeyboard()->curState) ? a_keyCode1 : 0;
        m_toggleVisibleKeyCode2 = a_keyCode2 < sizeof(RE::BSInputDeviceManager::GetSingleton()->GetKeyboard()->curState) ? a_keyCode2 : 0;
    }

    void DefaultBrowser::OnFocusGranted()
    {
        m_cefClient->SetCursorUpdatesEnabled(true);
        const auto browser = m_cefClient->GetBrowser();
        if (browser != nullptr)
        {
            browser->GetHost()->SetFocus(true);
        }

        if (const auto controls = RE::PlayerControls::GetSingleton())
        {
            auto& cdata = controls->data;
            cdata.lookInputVec.x = 0;
            cdata.lookInputVec.y = 0;
            cdata.prevLookVec.x = 0;
            cdata.prevLookVec.y = 0;
            if (!cdata.autoMove)
            {
                cdata.moveInputVec.x = 0;
                cdata.moveInputVec.y = 0;
            }
        }
    }

    void DefaultBrowser::OnFocusRevoked()
    {
        m_cefClient->SetCursorUpdatesEnabled(false);
        const auto browser = m_cefClient->GetBrowser();
        const auto host = browser != nullptr ? browser->GetHost() : nullptr;
        if (host != nullptr)
        {
            // Chromium must see balancing key-ups while it still owns focus.
            // SetFocus(false) alone does not repair Meridian's converter state
            // if another menu or an API transition interrupted a held key.
            m_keyInputConverter.ReleasePressedKeys();
            if (m_lastMouseInside)
            {
                host->SendMouseMoveEvent(m_lastCefMouseEvent, true);
            }
            host->SetFocus(false);
        }
        else
        {
            m_keyInputConverter.Clear();
        }
        m_lastMouseInside = false;
    }

    void DefaultBrowser::SetTextInputActive(bool a_active)
    {
        if (m_shutdownStarted.load(std::memory_order_acquire))
        {
            a_active = false;
        }

        if (m_textInputActive.exchange(a_active, std::memory_order_acq_rel) == a_active)
        {
            return;
        }
        Meridian::Menus::FocusArbiter::GetSingleton().OnTextInputChanged(this, a_active);
    }

    bool DefaultBrowser::IsTextInputActive() const
    {
        return m_textInputActive.load(std::memory_order_acquire);
    }

    void DefaultBrowser::AddPersistentJavaScript(const char* a_key,
                                                  const char* a_script,
                                                  const char* a_scriptUrl)
    {
        if (a_key == nullptr || a_script == nullptr)
        {
            return;
        }

        std::lock_guard locker(m_urlMutex);
        m_persistentJS[std::string(a_key)] = {
            std::string(a_script),
            a_scriptUrl == nullptr ? std::string(JS_EXECUTE_SCRIPT_URL) : std::string(a_scriptUrl)};
        if (m_isPageLoaded)
        {
            ExecuteJavaScript(a_script, a_scriptUrl);
        }
    }

    Meridian::UI::View::FocusResult DefaultBrowser::TryViewFocus(Meridian::UI::View::FocusMode a_mode)
    {
        if (m_shutdownStarted.load(std::memory_order_acquire))
        {
            return Meridian::UI::View::FocusResult::ShuttingDown;
        }
        if (!IsPageLoaded())
        {
            return Meridian::UI::View::FocusResult::NotReady;
        }

        auto& arbiter = Meridian::Menus::FocusArbiter::GetSingleton();
        const auto result = arbiter.TryClaim(shared_from_this(), a_mode);
        if (result == Meridian::UI::View::FocusResult::Granted)
        {
            const auto weakSelf = weak_from_this();
            SKSE::GetTaskInterface()->AddUITask([weakSelf]() {
                if (const auto self = weakSelf.lock();
                    self != nullptr && Meridian::Menus::FocusArbiter::GetSingleton().IsOwner(self.get()))
                {
                    self->OnFocusGranted();
                }
            });
        }
        return result;
    }

    void __cdecl DefaultBrowser::SetBrowserFocused(bool a_value)
    {
        {
            std::lock_guard locker(m_urlMutex);
            if (!m_isPageLoaded)
            {
                m_isFocusedCached = true;
                m_pendingFocusValue = a_value;
                return;
            }
            m_isFocusedCached = false;
        }

        QueueFocusRequest(a_value);
    }

    void DefaultBrowser::QueueFocusRequest(bool a_value)
    {
        const auto weakSelf = weak_from_this();
        SKSE::GetTaskInterface()->AddUITask([weakSelf, a_value]() {
            if (const auto self = weakSelf.lock())
            {
                self->ApplyBrowserFocused(a_value);
            }
        });
    }

    void DefaultBrowser::ApplyBrowserFocused(bool a_value)
    {
        if (m_shutdownStarted.load(std::memory_order_acquire))
        {
            return;
        }
        if (a_value)
        {
            Meridian::Menus::FocusArbiter::GetSingleton().Claim(shared_from_this());
        }
        else
        {
            Meridian::Menus::FocusArbiter::GetSingleton().Release(this);
        }
    }

    bool __cdecl DefaultBrowser::IsBrowserFocused()
    {
        return Meridian::Menus::FocusArbiter::GetSingleton().IsOwner(this);
    }

    void __cdecl DefaultBrowser::ToggleBrowserFocusByKeys(const std::uint32_t a_keyCode1, const std::uint32_t a_keyCode2)
    {
        m_toggleFocusKeyCode1 = a_keyCode1 < sizeof(RE::BSInputDeviceManager::GetSingleton()->GetKeyboard()->curState) ? a_keyCode1 : 0;
        m_toggleFocusKeyCode2 = a_keyCode2 < sizeof(RE::BSInputDeviceManager::GetSingleton()->GetKeyboard()->curState) ? a_keyCode2 : 0;
    }

    void __cdecl DefaultBrowser::LoadBrowserURL(const char* a_url, bool a_clearJSFunctions)
    {
        std::lock_guard locker(m_urlMutex);
        if (a_url == nullptr || *a_url == '\0' || !m_cefClient->AllowInitialNavigation(a_url))
        {
            m_logger->error("{}: blocked untrusted navigation request", NameOf(DefaultBrowser));
            return;
        }
        m_clearJSFunctions = a_clearJSFunctions;
        if (!IsPageLoaded())
        {
            m_isUrlCached = true;
            m_urlCache = a_url;
            return;
        }

        const auto browser = m_cefClient->GetBrowser();
        const auto frame = browser != nullptr ? browser->GetMainFrame() : nullptr;
        if (frame)
        {
            m_isPageLoaded = false;
            frame->LoadURL(CefString(a_url));
        }
        else
        {
            m_logger->error("{}: can't get main frame to load url \"{}\"", NameOf(DefaultBrowser), a_url);
        }
        m_isUrlCached = false;
    }

    void __cdecl DefaultBrowser::ExecuteJavaScript(const char* a_script, const char* a_scriptUrl)
    {
        std::lock_guard locker(m_urlMutex);
        if (!IsPageLoaded())
        {
            if (a_script != nullptr)
            {
                m_jsExecCache.push_back({a_script, a_scriptUrl == nullptr ? JS_EXECUTE_SCRIPT_URL : a_scriptUrl});
            }
            return;
        }

        const auto browser = m_cefClient->GetBrowser();
        const auto frame = browser != nullptr ? browser->GetMainFrame() : nullptr;
        if (frame == nullptr)
        {
            return;
        }

        if (!m_jsExecCache.empty())
        {
            for (auto& jsScript : m_jsExecCache)
            {
                frame->ExecuteJavaScript(std::get<0>(jsScript).c_str(), std::get<1>(jsScript).c_str(), 0);
            }
            m_jsExecCache.clear();
            m_jsExecCache.shrink_to_fit();
        }

        if (a_script != nullptr)
        {
            frame->ExecuteJavaScript(a_script, a_scriptUrl == nullptr ? JS_EXECUTE_SCRIPT_URL : a_scriptUrl, 0);
        }
    }

    void __cdecl DefaultBrowser::AddFunctionCallback(const Meridian::JS::JSFuncInfo& a_callbackInfo)
    {
        if (a_callbackInfo.objectName == nullptr || *a_callbackInfo.objectName == '\0' ||
            a_callbackInfo.funcName == nullptr || *a_callbackInfo.funcName == '\0')
        {
            m_logger->error("{}: rejected function binding with a null or empty name", NameOf(DefaultBrowser));
            return;
        }
        m_cefClient->EnableNativeBindings();
        std::lock_guard locker(m_urlMutex);
        if (!IsPageLoaded())
        {
            for (auto it = m_jsFuncRemoveCache.begin(); it != m_jsFuncRemoveCache.end();)
            {
                if (std::get<0>(*it) == a_callbackInfo.objectName && std::get<1>(*it) == a_callbackInfo.funcName)
                {
                    it = m_jsFuncRemoveCache.erase(it);
                }
                else
                {
                    ++it;
                }
            }

            m_jsFuncCallbackInfoCache.push_back(a_callbackInfo);
            return;
        }

        AddFunctionCallbackAndSendMessage(a_callbackInfo);
    }

    void __cdecl DefaultBrowser::RemoveFunctionCallback(const char* a_objectName, const char* a_funcName)
    {
        if (a_objectName == nullptr || *a_objectName == '\0' || a_funcName == nullptr || *a_funcName == '\0')
        {
            return;
        }
        std::lock_guard locker(m_urlMutex);
        if (!IsPageLoaded())
        {
            for (auto it = m_jsFuncCallbackInfoCache.begin(); it != m_jsFuncCallbackInfoCache.end();)
            {
                if (it->objectNameString == a_objectName && it->funcNameString == a_funcName)
                {
                    it = m_jsFuncCallbackInfoCache.erase(it);
                }
                else
                {
                    ++it;
                }
            }

            if (!m_jsFuncStorage->RemoveFunctionCallback(a_objectName, a_funcName))
            {
                m_jsFuncRemoveCache.push_back({a_objectName, a_funcName});
            }
            return;
        }

        RemoveFunctionCallbackAndSendMessage(a_objectName, a_funcName);
    }

    void __cdecl DefaultBrowser::RemoveFunctionCallback(const Meridian::JS::JSFuncInfo& a_callbackInfo)
    {
        RemoveFunctionCallback(a_callbackInfo.objectName, a_callbackInfo.funcName);
    }

    void __cdecl DefaultBrowser::ExecEventFunction(const char* a_eventName, const char* a_data)
    {
        if (a_eventName == nullptr || *a_eventName == '\0')
        {
            return;
        }
        const auto browser = m_cefClient->GetBrowser();
        if (IsPageLoaded() && browser != nullptr)
        {
            auto cefMessage = CefProcessMessage::Create(IPC_JS_EVENT_FUNCTION_CALL_EVENT);
            cefMessage->GetArgumentList()->SetString(0, a_eventName);
            cefMessage->GetArgumentList()->SetString(1, a_data == nullptr ? "" : a_data);

            browser->GetMainFrame()->SendProcessMessage(CefProcessId::PID_RENDERER, cefMessage);
        }
    }

#pragma endregion

#pragma region RE::MenuEventHandler

    bool DefaultBrowser::CanProcess(RE::InputEvent* a_event)
    {
        if (m_shutdownStarted.load(std::memory_order_acquire)) { return false; }

        return IsBrowserFocused() && m_cefClient->GetBrowser() != nullptr;
    }

    bool DefaultBrowser::ProcessMouseMove(RE::MouseMoveEvent* a_event)
    {
        if (m_shutdownStarted.load(std::memory_order_acquire)) { return false; }

        if (!IsBrowserFocused())
        {
            return false;
        }

        const auto browser = m_cefClient->GetBrowser();
        const auto browserHost = browser != nullptr ? browser->GetHost() : nullptr;
        if (browserHost == nullptr)
        {
            Meridian::Menus::FocusArbiter::GetSingleton().Release(this);
            return false;
        }

        auto cursor = RE::UI::GetSingleton()->GetMenu<RE::CursorMenu>(RE::CursorMenu::MENU_NAME);
        if (cursor.get())
        {
            cursor->ProcessMouseMove(a_event);
        }

        const auto menuCursor = RE::MenuCursor::GetSingleton();
        if (menuCursor == nullptr)
        {
            return true; // no cursor source this event; swallow while focused
        }

        const auto geometry = m_geometryHolder->Get();
        int localX = 0;
        int localY = 0;
        Meridian::Menus::CompositorMath::ScreenToBrowser(geometry, menuCursor->cursorPosX, menuCursor->cursorPosY, localX, localY);
        m_lastCefMouseEvent.x = localX;
        m_lastCefMouseEvent.y = localY;
        const bool inside = Meridian::Menus::CompositorMath::HitTest(
            geometry,
            static_cast<int>(menuCursor->cursorPosX),
            static_cast<int>(menuCursor->cursorPosY));
        if (inside || m_lastMouseInside)
        {
            browserHost->SendMouseMoveEvent(m_lastCefMouseEvent, !inside);
        }
        m_lastMouseInside = inside;

        return true;
    }

    bool DefaultBrowser::ProcessButton(RE::ButtonEvent* a_event)
    {
        if (m_shutdownStarted.load(std::memory_order_acquire)) { return false; }

        if (!IsBrowserFocused())
        {
            return false;
        }

        const auto browser = m_cefClient->GetBrowser();
        const auto browserHost = browser != nullptr ? browser->GetHost() : nullptr;
        if (browserHost == nullptr)
        {
            Meridian::Menus::FocusArbiter::GetSingleton().Release(this);
            return false;
        }

        const auto scanCode = a_event->GetIDCode();
        switch (a_event->GetDevice())
        {
        case RE::INPUT_DEVICE::kMouse:
            if (!a_event->IsDown() && !a_event->IsUp())
            {
                return true;
            }

            {
                const auto menuCursor = RE::MenuCursor::GetSingleton();
                const auto geometry = m_geometryHolder->Get();
                if (menuCursor == nullptr ||
                    !Meridian::Menus::CompositorMath::HitTest(geometry,
                                                              static_cast<int>(menuCursor->cursorPosX),
                                                              static_cast<int>(menuCursor->cursorPosY)))
                {
                    return false;
                }

                // The click must land where the cursor is NOW: m_lastCefMouseEvent
                // is only refreshed by focused mouse-moves, so a park-then-focus-
                // then-click would otherwise send stale (or zero) coordinates.
                int localX = 0;
                int localY = 0;
                Meridian::Menus::CompositorMath::ScreenToBrowser(geometry, menuCursor->cursorPosX, menuCursor->cursorPosY, localX, localY);
                m_lastCefMouseEvent.x = localX;
                m_lastCefMouseEvent.y = localY;
            }

            switch (scanCode)
            {
            case RE::BSWin32MouseDevice::Keys::kWheelUp:
                m_lastCefMouseEvent.modifiers = m_keyInputConverter.GetCurrentModifiers();
                browserHost->SendMouseWheelEvent(m_lastCefMouseEvent, 0, MOUSE_WHEEL_DELTA);
                break;
            case RE::BSWin32MouseDevice::Keys::kWheelDown:
                m_lastCefMouseEvent.modifiers = m_keyInputConverter.GetCurrentModifiers();
                browserHost->SendMouseWheelEvent(m_lastCefMouseEvent, 0, -MOUSE_WHEEL_DELTA);
                break;
            case RE::BSWin32MouseDevice::Key::kLeftButton:
                m_keyInputConverter.UpdateCefKeyModifiers(EVENTFLAG_LEFT_MOUSE_BUTTON, a_event->IsDown());
                m_lastCefMouseEvent.modifiers = m_keyInputConverter.GetCurrentModifiers();
                browserHost->SendMouseClickEvent(m_lastCefMouseEvent, CefBrowserHost::MouseButtonType::MBT_LEFT, a_event->IsUp(), 1);
                break;
            case RE::BSWin32MouseDevice::Key::kRightButton:
                m_keyInputConverter.UpdateCefKeyModifiers(EVENTFLAG_RIGHT_MOUSE_BUTTON, a_event->IsDown());
                m_lastCefMouseEvent.modifiers = m_keyInputConverter.GetCurrentModifiers();
                browserHost->SendMouseClickEvent(m_lastCefMouseEvent, CefBrowserHost::MouseButtonType::MBT_RIGHT, a_event->IsUp(), 1);
                break;
            case RE::BSWin32MouseDevice::Key::kMiddleButton:
                m_keyInputConverter.UpdateCefKeyModifiers(EVENTFLAG_MIDDLE_MOUSE_BUTTON, a_event->IsDown());
                m_lastCefMouseEvent.modifiers = m_keyInputConverter.GetCurrentModifiers();
                browserHost->SendMouseClickEvent(m_lastCefMouseEvent, CefBrowserHost::MouseButtonType::MBT_MIDDLE, a_event->IsUp(), 1);
                break;
            default:
                break;
            }
            break;
        case RE::INPUT_DEVICE::kFlatVirtualKeyboard:
        case RE::INPUT_DEVICE::kKeyboard: {
            m_keyInputConverter.ProcessButton(a_event);
            break;
        }
        default:
            return false;
        }

        return true;
    }

    void DefaultBrowser::NotifyGeometryChanged()
    {
        const auto geometry = m_geometryHolder->Get();

        const auto renderLayer = m_cefClient->GetRenderLayer();
        if (auto* ringLayer = dynamic_cast<Meridian::Render::CEFRingRenderLayer*>(renderLayer.get()))
        {
            ringLayer->Transport().RequestResize(Meridian::Menus::CompositorMath::LogicalWidth(geometry),
                                                 Meridian::Menus::CompositorMath::LogicalHeight(geometry));
        }

        const auto browser = m_cefClient->GetBrowser();
        if (browser != nullptr)
        {
            browser->GetHost()->WasResized();
        }
    }

    void __cdecl DefaultBrowser::SetBrowserRect(int a_x, int a_y, int a_width, int a_height)
    {
        m_geometryHolder->SetRect(a_x, a_y, a_width, a_height);
        NotifyGeometryChanged();
    }

    void __cdecl DefaultBrowser::GetBrowserRect(int& a_x, int& a_y, int& a_width, int& a_height)
    {
        const auto geometry = m_geometryHolder->Get();
        a_x = geometry.x;
        a_y = geometry.y;
        a_width = geometry.width;
        a_height = geometry.height;
    }

    void __cdecl DefaultBrowser::SetBrowserResolutionScale(float a_scale)
    {
        m_geometryHolder->SetResolutionScale(a_scale);
        NotifyGeometryChanged();
    }

    void __cdecl DefaultBrowser::SetBrowserZOrder(int a_zOrder)
    {
        m_geometryHolder->SetZOrder(a_zOrder);
    }

    void __cdecl DefaultBrowser::AddPromiseFunctionCallback(const Meridian::JS::JSPromiseFuncInfo& a_info)
    {
        if (a_info.objectName == nullptr || *a_info.objectName == '\0' ||
            a_info.funcName == nullptr || *a_info.funcName == '\0' ||
            a_info.callback == nullptr)
        {
            m_logger->error("{}: rejected promise binding with invalid name or null callback", NameOf(DefaultBrowser));
            return;
        }
        m_cefClient->EnableNativeBindings();
        std::lock_guard locker(m_urlMutex);
        if (!IsPageLoaded())
        {
            // Not-yet-sent: queue only, do NOT touch m_jsPromiseFuncCallbackMap
            // here — it must stay purely "already sent" so a subsequent
            // m_clearJSFunctions reload clears only what was actually sent,
            // never this pending registration. Flushed at load-end, or
            // superseded by the next load-start's replay if navigation
            // hasn't reached load-end yet.
            m_jsPromiseFuncCallbackInfoCache.push_back(a_info);
            return;
        }

        AddPromiseFunctionCallbackAndSendMessage(a_info);
    }

#pragma endregion

}
