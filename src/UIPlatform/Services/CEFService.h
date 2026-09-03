#pragma once

#include "PCH.h"

#include <chrono>
#include <condition_variable>
#include <thread>
#include <unordered_map>

namespace Meridian::Services
{
    class CEFService
    {
    public:
        enum class LifecycleState : std::uint8_t
        {
            Uninitialized,
            Initializing,
            Running,
            ClosingBrowsers,
            BrowsersClosed,
            ShuttingDown,
            Stopped,
            Failed,
        };

        enum class BrowserCreationDisposition : std::uint8_t
        {
            KeepOpen,
            CloseImmediately,
        };

    protected:
        struct ClientState
        {
            CefRefPtr<CefClient> client = nullptr;
            bool creationPending = false;
            bool closeRequested = false;
            std::unordered_map<int, CefRefPtr<CefBrowser>> browsers;
        };

        class DrainBarrierTask;

        static inline std::mutex s_cefInitMutex;
        static inline CefRefPtr<CefApp> s_cefApp = nullptr;

        static inline std::mutex s_lifecycleMutex;
        static inline std::condition_variable s_lifecycleCondition;
        static inline LifecycleState s_lifecycleState = LifecycleState::Uninitialized;
        static inline std::thread::id s_initializeThreadId{};
        static inline std::unordered_map<const CefClient*, ClientState> s_clients;
        static inline bool s_drainBarrierPosted = false;

        static bool HasOutstandingBrowsersLocked();
        static bool ArmDrainBarrierIfNeededLocked();
        static void PostDrainBarrier();
        static void CompleteDrainBarrier();
        static void FailDrainBarrierPost();

    public:
        static void CEFInitialize(CefRefPtr<CefApp> a_cefApp, const CefSettings& a_cefSettings);

        /// <summary>
        /// Requests forced closure of every live (or still-creating) browser and
        /// waits for all OnBeforeClose callbacks to unwind on CEF's UI thread.
        /// Must be called from the thread that initialized CEF, never TID_UI.
        /// </summary>
        static bool CloseAllBrowsersAndWait(std::chrono::milliseconds a_timeout);

        /// <summary>
        /// Finalizes CEF after CloseAllBrowsersAndWait has succeeded and all
        /// external client/render-handler owners have been released.
        /// </summary>
        static void CEFShutdown();

        static bool CreateBrowser(const CefRefPtr<CefClient> a_client,
                                  const CefRefPtr<CefDictionaryValue> a_jsFuncInfo,
                                  const CefString a_url,
                                  const CefWindowInfo& a_cefWindowInfo,
                                  const CefBrowserSettings& a_cefBrowserSettings);

        /// <summary>
        /// Closes every browser associated with a client. If browser creation is
        /// still pending, the browser will be closed immediately in OnAfterCreated.
        /// </summary>
        static void RequestClientClose(CefRefPtr<CefClient> a_client);

        // Called by MeridianCefClient from CEF's browser-process UI thread.
        static BrowserCreationDisposition OnBrowserCreated(CefRefPtr<CefClient> a_client,
                                                            CefRefPtr<CefBrowser> a_browser);
        static void OnBrowserBeforeClose(CefRefPtr<CefClient> a_client, CefRefPtr<CefBrowser> a_browser);

        static LifecycleState GetLifecycleState();
        static bool IsAcceptingBrowsers();
    };
}
