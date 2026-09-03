#pragma once

#define JS_EXECUTE_SCRIPT_URL "Skyrim"

#include "JSTypes.h"

namespace Meridian::CEF
{
    /// <summary>
    /// A browser instance handed out by IUIPlatformAPI::AddOrGetBrowser.
    ///
    /// Lifetime contract (1.0): every registration on this interface — JS
    /// function bindings (fire-and-forget, event, and promise) and toggle
    /// keys — belongs to THIS browser instance. Bindings survive navigation
    /// and reload within the instance (the platform replays them on load) —
    /// except that LoadBrowserURL clears the fire-and-forget and promise
    /// bindings first unless called with a_clearJSFunctions=false;
    /// event-function bindings are unaffected, being re-installed at every
    /// page load. A browser that is released and re-created is a NEW
    /// instance: the consumer must re-register its bindings and toggle keys
    /// on it.
    /// </summary>
    class IBrowser
    {
    public:
        virtual ~IBrowser() = default;

        /// <summary>
        /// Browser takes time to load, so you can check if browser is ready using some methods.
        /// </summary>
        /// <returns></returns>
        virtual bool __cdecl IsBrowserReady() = 0;
        /// <summary>
        /// Page takes time to load. You can check that the page has loaded using this method.
        /// </summary>
        /// <returns></returns>
        virtual bool __cdecl IsPageLoaded() = 0;

        virtual void __cdecl SetBrowserVisible(bool a_value) = 0;
        virtual bool __cdecl IsBrowserVisible() = 0;
        /// <summary>
        /// Sets browser visibility keys (see RE::BSKeyboardDevice::Keys)
        /// Pass zeros to disable
        /// Toggle chords are evaluated for EVERY browser before any focused
        /// browser can swallow the event (1.0 contract): while a registered
        /// chord is held, any button-down that completes or re-satisfies it
        /// is consumed and never reaches a page; key-ups and lone
        /// chord-halves are not intercepted.
        /// </summary>
        /// <param name="a_keyCode1"></param>
        /// <param name="a_keyCode2"></param>
        /// <returns></returns>
        virtual void __cdecl ToggleBrowserVisibleByKeys(const std::uint32_t a_keyCode1, const std::uint32_t a_keyCode2) = 0;

        virtual void __cdecl SetBrowserFocused(bool a_value) = 0;
        virtual bool __cdecl IsBrowserFocused() = 0;
        /// <summary>
        /// Sets browser autofocus keys (see RE::BSKeyboardDevice::Keys)
        /// Pass zeros to disable
        /// Toggle chords are evaluated for EVERY browser before any focused
        /// browser can swallow the event (1.0 contract): while a registered
        /// chord is held, any button-down that completes or re-satisfies it
        /// is consumed and never reaches a page; key-ups and lone
        /// chord-halves are not intercepted.
        /// </summary>
        /// <param name="a_keyCode1"></param>
        /// <param name="a_keyCode2"></param>
        /// <returns></returns>
        virtual void __cdecl ToggleBrowserFocusByKeys(const std::uint32_t a_keyCode1, const std::uint32_t a_keyCode2) = 0;

        /// <summary>
        /// Loads new url
        /// </summary>
        /// <param name="a_url"></param>
        /// <param name="a_clearJSFunctions">If true then remove all previous js function callbacks</param>
        /// <returns></returns>
        virtual void __cdecl LoadBrowserURL(const char* a_url, bool a_clearJSFunctions = true) = 0;

        virtual void __cdecl ExecuteJavaScript(const char* a_script, const char* a_scriptUrl = JS_EXECUTE_SCRIPT_URL) = 0;
        virtual void __cdecl AddFunctionCallback(const Meridian::JS::JSFuncInfo& a_callbackInfo) = 0;
        virtual void __cdecl RemoveFunctionCallback(const char* a_objectName, const char* a_funcName) = 0;
        virtual void __cdecl RemoveFunctionCallback(const Meridian::JS::JSFuncInfo& a_callbackInfo) = 0;
        virtual void __cdecl ExecEventFunction(const char* a_eventName, const char* a_data) = 0;

        /// <summary>
        /// Sets the on-screen destination rect in pixels. The browser's
        /// logical page size follows the rect (times the resolution scale).
        /// Until called, the browser is fullscreen.
        /// </summary>
        virtual void __cdecl SetBrowserRect(int a_x, int a_y, int a_width, int a_height) = 0;

        /// <summary>Reads back the current on-screen rect.</summary>
        virtual void __cdecl GetBrowserRect(int& a_x, int& a_y, int& a_width, int& a_height) = 0;

        /// <summary>
        /// Renders the page at rect × scale and stretches to the rect.
        /// 0.5 quarters the pixel cost of a widget. Clamped to (0, 2]. Default 1.0.
        /// </summary>
        virtual void __cdecl SetBrowserResolutionScale(float a_scale) = 0;

        /// <summary>Higher z draws on top. Ties break by creation order. Default 0.</summary>
        virtual void __cdecl SetBrowserZOrder(int a_zOrder) = 0;

        /// <summary>Registers a JS function window.<objectName>.<funcName>(...)
        /// that returns a Promise. The native callback receives the arguments
        /// as JSON strings plus a one-shot resolver it may settle from any
        /// thread, immediately or later — including after the browser has
        /// closed (a post-close settle is a safe no-op). Fire-and-forget
        /// bindings registered via AddFunctionCallback are unaffected and
        /// keep returning undefined.
        ///
        /// There is NO remove API for promise bindings in 1.0 (the renderer
        /// keeps a reserved handler for a future one). To retire a binding,
        /// load a page that no longer calls it, or re-create the browser.</summary>
        virtual void __cdecl AddPromiseFunctionCallback(const Meridian::JS::JSPromiseFuncInfo& a_info) = 0;
    };
}
