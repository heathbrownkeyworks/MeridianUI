#pragma once

#include "PCH.h"
#include "CEF/MeridianCefClient.h"
#include "Services/CEFService.h"
#include "Hooks/WinProcHook.h"
#include "JS/JSFunctionStorage.h"
#include "JS/JSEventFuncInfo.h"
#include "JS/PromiseRouter.h"
#include <map>
#include "Converters/CefValueToJSONConverter.h"
#include "Converters/KeyInputConverter.h"
#include "Menus/LayerGeometryHolder.h"
#include "MeridianUIAPI/ViewAPI.h"

namespace Meridian::CEF
{
    class MeridianCefClient;

    class DefaultBrowser : public IBrowser,
                           public std::enable_shared_from_this<DefaultBrowser>
    {
    protected:
        std::shared_ptr<spdlog::logger> m_logger = nullptr;
        CefRefPtr<MeridianCefClient> m_cefClient = nullptr;
        std::shared_ptr<Meridian::JS::JSFunctionStorage> m_jsFuncStorage = nullptr;
        std::shared_ptr<Meridian::Menus::LayerGeometryHolder> m_geometryHolder = nullptr;

        // Url
        std::recursive_mutex m_urlMutex;
        bool m_isUrlCached = false;
        bool m_isPageLoaded = false;
        bool m_clearJSFunctions = false;
        std::string m_urlCache = "";

        // Focus
        bool m_isFocusedCached = false;
        bool m_pendingFocusValue = false;
        std::atomic_bool m_textInputActive{false};

        // JS execution
        std::vector<std::tuple<std::string, std::string>> m_jsExecCache;
        std::map<std::string, std::tuple<std::string, std::string>> m_persistentJS;

        // JS function callback
        std::list<Meridian::JS::JSFuncInfoString> m_jsFuncCallbackInfoCache;
        std::list<std::tuple<std::string, std::string>> m_jsFuncRemoveCache;

        // JS promise function callback. m_jsPromiseFuncCallbackMap is the
        // already-sent persistent registry — the void path's analog is
        // m_jsFuncStorage's internal map (cleared wholesale when
        // m_clearJSFunctions, bulk-resent otherwise). Guarded by m_urlMutex,
        // mirroring the void-binding discipline.
        struct JSPromiseFuncCallbackData
        {
            Meridian::JS::JSPromiseCallback callback = nullptr;
            bool executeInGameThread = true;
        };
        std::map<std::pair<std::string, std::string>, JSPromiseFuncCallbackData> m_jsPromiseFuncCallbackMap;

        // Not-yet-sent registrations (owned strings, since JSPromiseFuncInfo
        // holds raw pointers with no lifetime guarantee). This is a SEPARATE
        // container from m_jsPromiseFuncCallbackMap — the direct analog of
        // m_jsFuncCallbackInfoCache — and, critically, is NOT touched by the
        // m_clearJSFunctions branch: the clear wipes only the already-sent
        // registry, never registrations still in flight. Replayed
        // unconditionally into the map (and sent) at both load-start (after
        // the clear/bulk) and load-end, exactly like the
        // m_jsFuncCallbackInfoCache replay in onMainFrameLoadStart/onMainFrameLoadEnd.
        struct JSPromiseFuncInfoString
        {
            std::string objectNameString;
            std::string funcNameString;
            Meridian::JS::JSPromiseCallback callback = nullptr;
            bool executeInGameThread = true;

            JSPromiseFuncInfoString() = default;
            JSPromiseFuncInfoString(const Meridian::JS::JSPromiseFuncInfo& a_info)
                : objectNameString(a_info.objectName),
                  funcNameString(a_info.funcName),
                  callback(a_info.callback),
                  executeInGameThread(a_info.executeInGameThread)
            {
            }

            operator Meridian::JS::JSPromiseFuncInfo() const
            {
                Meridian::JS::JSPromiseFuncInfo info;
                info.objectName = objectNameString.c_str();
                info.funcName = funcNameString.c_str();
                info.callback = callback;
                info.executeInGameThread = executeInGameThread;
                return info;
            }
        };
        std::list<JSPromiseFuncInfoString> m_jsPromiseFuncCallbackInfoCache;

        CefMouseEvent m_lastCefMouseEvent;
        bool m_lastMouseInside = false;
        Meridian::Converters::KeyInputConverter m_keyInputConverter;

        std::uint32_t m_toggleFocusKeyCode1 = 0;
        std::uint32_t m_toggleFocusKeyCode2 = 0;

        std::uint32_t m_toggleVisibleKeyCode1 = 0;
        std::uint32_t m_toggleVisibleKeyCode2 = 0;

        std::atomic_bool m_closeRequested{false};
        // Set on any teardown path; gates the input entry points. Distinct from m_closeRequested, whose exchange-transition RequestClose consumes.
        std::atomic_bool m_shutdownStarted{false};

        sigslot::scoped_connection m_onWndInactive_Connection;
        sigslot::scoped_connection m_onIPCMessageReceived_Connection;
        sigslot::scoped_connection m_onAfterBrowserCreated_Connection;
        sigslot::scoped_connection m_onBeforeBrowserClose_Connection;
        sigslot::scoped_connection m_onMainFrameLoadStart_Connection;
        sigslot::scoped_connection m_onMainFrameLoadEnd_Connection;

    public:
        DefaultBrowser(std::shared_ptr<spdlog::logger> a_logger,
                       CefRefPtr<MeridianCefClient> a_cefClient,
                       std::shared_ptr<Meridian::JS::JSFunctionStorage> a_jsFuncStorage,
                       std::shared_ptr<Meridian::Menus::LayerGeometryHolder> a_geometryHolder);
        ~DefaultBrowser() override;

        CefRefPtr<MeridianCefClient> GetCefClient();
        bool IsReadyAndLog();
        void BeginShutdown();
        void RequestClose();

        void AddFunctionCallbackAndSendMessage(const Meridian::JS::JSFuncInfo& a_callbackInfo);
        void RemoveFunctionCallbackAndSendMessage(const char* a_objectName, const char* a_funcName);
        void AddPromiseFunctionCallbackAndSendMessage(const Meridian::JS::JSPromiseFuncInfo& a_info);
        void AddPersistentJavaScript(const char* a_key,
                                     const char* a_script,
                                     const char* a_scriptUrl = JS_EXECUTE_SCRIPT_URL);

        void OnFocusGranted();
        void OnFocusRevoked();
        void SetTextInputActive(bool a_active);
        [[nodiscard]] bool IsTextInputActive() const;
        Meridian::UI::View::FocusResult TryViewFocus(Meridian::UI::View::FocusMode a_mode);

        // IBrowser
        bool __cdecl IsBrowserReady() override;
        bool __cdecl IsPageLoaded() override;

        void __cdecl SetBrowserVisible(bool a_value) override;
        bool __cdecl IsBrowserVisible() override;
        void __cdecl ToggleBrowserVisibleByKeys(const std::uint32_t a_keyCode1, const std::uint32_t a_keyCode2) override;

        void __cdecl SetBrowserFocused(bool a_value) override;
        bool __cdecl IsBrowserFocused() override;
        void __cdecl ToggleBrowserFocusByKeys(const std::uint32_t a_keyCode1, const std::uint32_t a_keyCode2) override;

        void __cdecl LoadBrowserURL(const char* a_url, bool a_clearJSFunctions = true) override;
        void __cdecl ExecuteJavaScript(const char* a_script, const char* a_scriptUrl = JS_EXECUTE_SCRIPT_URL) override;
        void __cdecl AddFunctionCallback(const Meridian::JS::JSFuncInfo& a_callbackInfo) override;
        void __cdecl RemoveFunctionCallback(const char* a_objectName, const char* a_funcName) override;
        void __cdecl RemoveFunctionCallback(const Meridian::JS::JSFuncInfo& a_callbackInfo) override;
        void __cdecl ExecEventFunction(const char* a_eventName, const char* a_data) override;

        void __cdecl SetBrowserRect(int a_x, int a_y, int a_width, int a_height) override;
        void __cdecl GetBrowserRect(int& a_x, int& a_y, int& a_width, int& a_height) override;
        void __cdecl SetBrowserResolutionScale(float a_scale) override;
        void __cdecl SetBrowserZOrder(int a_zOrder) override;
        void __cdecl AddPromiseFunctionCallback(const Meridian::JS::JSPromiseFuncInfo& a_info) override;

        // Meridian input routing. These deliberately do not inherit the
        // runtime-dependent RE::MenuEventHandler vtable.
        bool CanProcess(RE::InputEvent* a_event);
        bool ProcessMouseMove(RE::MouseMoveEvent* a_event);
        bool ProcessButton(RE::ButtonEvent* a_event);

        // Checks the registered focus/visibility toggle chords against this
        // event and fires the corresponding toggle. Returns true if either
        // fired (event consumed). Called ahead of the ProcessButton walk —
        // see InputRouter's toggle pass.
        bool ProcessToggleKeys(RE::ButtonEvent* a_event);

    private:
        void QueueFocusRequest(bool a_value);
        void ApplyBrowserFocused(bool a_value);
        void NotifyGeometryChanged();
        void SendKeyEventIfAlive(const CefKeyEvent& a_keyEvent);
        void OnCefBrowserBeforeClose(CefRefPtr<CefBrowser> a_browser);
    };
}
