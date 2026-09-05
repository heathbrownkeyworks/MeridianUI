#include "CEFService.h"

#include "Scheme/ModSchemeHandler.h"

#include <include/cef_task.h>

namespace Meridian::Services
{
    class CEFService::DrainBarrierTask final : public CefTask
    {
    public:
        DrainBarrierTask() = default;

        void Execute() override
        {
            CEFService::CompleteDrainBarrier();
        }

    private:
        IMPLEMENT_REFCOUNTING(DrainBarrierTask);
        DISALLOW_COPY_AND_ASSIGN(DrainBarrierTask);
    };

    bool CEFService::HasOutstandingBrowsersLocked()
    {
        for (const auto& clientEntry : s_clients)
        {
            const auto& state = clientEntry.second;
            if (state.creationPending || !state.browsers.empty())
            {
                return true;
            }
        }

        return false;
    }

    bool CEFService::ArmDrainBarrierIfNeededLocked()
    {
        if (s_lifecycleState != LifecycleState::ClosingBrowsers ||
            s_drainBarrierPosted ||
            HasOutstandingBrowsersLocked())
        {
            return false;
        }

        s_drainBarrierPosted = true;
        return true;
    }

    void CEFService::PostDrainBarrier()
    {
        if (!CefPostTask(TID_UI, CefRefPtr<CefTask>(new DrainBarrierTask())))
        {
            FailDrainBarrierPost();
        }
    }

    void CEFService::CompleteDrainBarrier()
    {
        bool notify = false;
        {
            std::lock_guard lock(s_lifecycleMutex);
            s_drainBarrierPosted = false;

            if (s_lifecycleState == LifecycleState::ClosingBrowsers && !HasOutstandingBrowsersLocked())
            {
                s_lifecycleState = LifecycleState::BrowsersClosed;
                notify = true;
            }
        }

        if (notify)
        {
            spdlog::info("CEFService: all browser close callbacks have drained");
            s_lifecycleCondition.notify_all();
        }
    }

    void CEFService::FailDrainBarrierPost()
    {
        {
            std::lock_guard lock(s_lifecycleMutex);
            s_drainBarrierPosted = false;
            s_lifecycleState = LifecycleState::Failed;
        }

        spdlog::error("CEFService: failed to post browser-close drain barrier to CEF UI thread");
        s_lifecycleCondition.notify_all();
    }

    void CEFService::CEFInitialize(CefRefPtr<CefApp> a_cefApp, const CefSettings& a_cefSettings)
    {
        std::lock_guard locker(s_cefInitMutex);

        {
            std::lock_guard lifecycleLock(s_lifecycleMutex);
            if (s_lifecycleState == LifecycleState::InitializationFailed)
            {
                throw std::runtime_error(fmt::format(
                    "{}: CEF initialization previously failed, code {}; restart Skyrim before trying again",
                    NameOf(CEFService), s_initializationExitCode));
            }

            if (s_cefApp != nullptr || s_lifecycleState != LifecycleState::Uninitialized)
            {
                throw std::runtime_error(fmt::format("{}: CEF already inited", NameOf(CEFService)));
            }

            s_lifecycleState = LifecycleState::Initializing;
        }

        CefMainArgs args(GetModuleHandleA(nullptr));
        if (!CefInitialize(args, a_cefSettings, a_cefApp, nullptr))
        {
            std::lock_guard lifecycleLock(s_lifecycleMutex);
            // After a false return, CEF only permits CefGetExitCode. Never
            // retry initialization or call CEF shutdown in this process.
            s_lifecycleState = LifecycleState::InitializationFailed;
            s_initializationExitCode = CefGetExitCode();
            throw std::runtime_error(fmt::format(
                "{}: failed to initialize CEF, code {}; restart Skyrim before trying again",
                NameOf(CEFService), s_initializationExitCode));
        }

        {
            std::lock_guard lifecycleLock(s_lifecycleMutex);
            s_cefApp = a_cefApp;
            s_initializeThreadId = std::this_thread::get_id();
            s_clients.clear();
            s_drainBarrierPosted = false;
            s_lifecycleState = LifecycleState::Running;
        }

        if (CefRegisterSchemeHandlerFactory("mod", "", new Meridian::Scheme::ModSchemeHandlerFactory()))
        {
            spdlog::info("CEFService::CEFInitialize registered the mod:// scheme handler factory");
        }
        else
        {
            spdlog::error("CEFService::CEFInitialize failed to register the mod:// scheme handler factory");
        }

        spdlog::info("CEFService::CEFInitialize successfully on application thread {}",
                     std::hash<std::thread::id>{}(s_initializeThreadId));
    }

    bool CEFService::CloseAllBrowsersAndWait(std::chrono::milliseconds a_timeout)
    {
        {
            std::lock_guard lock(s_lifecycleMutex);
            if (s_lifecycleState == LifecycleState::Uninitialized || s_lifecycleState == LifecycleState::Stopped ||
                s_lifecycleState == LifecycleState::InitializationFailed)
            {
                return true;
            }

            if (std::this_thread::get_id() != s_initializeThreadId)
            {
                spdlog::error("CEFService: browser shutdown requested from a thread other than the CEF initialization thread");
                return false;
            }
        }

        if (CefCurrentlyOn(TID_UI))
        {
            spdlog::error("CEFService: refusing to wait for browser closure on CEF UI thread");
            return false;
        }

        std::vector<CefRefPtr<CefBrowser>> browsersToClose;
        bool postDrainBarrier = false;
        std::size_t activeBrowserCount = 0;
        std::size_t pendingBrowserCount = 0;
        {
            std::lock_guard lock(s_lifecycleMutex);

            if (s_lifecycleState == LifecycleState::Failed || s_lifecycleState == LifecycleState::ShuttingDown)
            {
                return false;
            }

            if (s_lifecycleState == LifecycleState::BrowsersClosed)
            {
                return true;
            }

            s_lifecycleState = LifecycleState::ClosingBrowsers;
            for (auto& clientEntry : s_clients)
            {
                auto& state = clientEntry.second;
                state.closeRequested = true;
                pendingBrowserCount += state.creationPending ? 1 : 0;
                activeBrowserCount += state.browsers.size();
                for (const auto& browserEntry : state.browsers)
                {
                    const auto& browser = browserEntry.second;
                    if (browser != nullptr)
                    {
                        browsersToClose.push_back(browser);
                    }
                }
            }

            postDrainBarrier = ArmDrainBarrierIfNeededLocked();
        }

        spdlog::info("CEFService: shutdown requested on application thread {} ({} active, {} pending)",
                     std::hash<std::thread::id>{}(std::this_thread::get_id()),
                     activeBrowserCount,
                     pendingBrowserCount);

        for (const auto& browser : browsersToClose)
        {
            if (browser != nullptr && browser->IsValid())
            {
                browser->GetHost()->CloseBrowser(true);
            }
        }
        browsersToClose.clear();

        if (postDrainBarrier)
        {
            PostDrainBarrier();
        }

        std::unique_lock lock(s_lifecycleMutex);
        const auto drained = s_lifecycleCondition.wait_for(lock, a_timeout, []() {
            return s_lifecycleState == LifecycleState::BrowsersClosed ||
                   s_lifecycleState == LifecycleState::Stopped ||
                   s_lifecycleState == LifecycleState::Failed;
        });

        if (!drained)
        {
            std::size_t browserCount = 0;
            std::size_t pendingCount = 0;
            for (const auto& clientEntry : s_clients)
            {
                const auto& state = clientEntry.second;
                browserCount += state.browsers.size();
                pendingCount += state.creationPending ? 1 : 0;
            }

            spdlog::error("CEFService: timed out waiting for browser shutdown ({} live, {} pending)", browserCount, pendingCount);
            return false;
        }

        return s_lifecycleState == LifecycleState::BrowsersClosed || s_lifecycleState == LifecycleState::Stopped;
    }

    void CEFService::CEFShutdown()
    {
        std::lock_guard locker(s_cefInitMutex);

        {
            std::lock_guard lifecycleLock(s_lifecycleMutex);
            if (s_cefApp == nullptr || s_lifecycleState == LifecycleState::Stopped || s_lifecycleState == LifecycleState::Uninitialized)
            {
                return;
            }

            if (std::this_thread::get_id() != s_initializeThreadId)
            {
                throw std::runtime_error("CEFService: CEFShutdown must run on the CEF initialization thread");
            }

            if (CefCurrentlyOn(TID_UI))
            {
                throw std::runtime_error("CEFService: CEFShutdown cannot run on CEF UI thread");
            }

            if (s_lifecycleState != LifecycleState::BrowsersClosed)
            {
                throw std::runtime_error("CEFService: CEFShutdown called before all browsers closed");
            }

            s_lifecycleState = LifecycleState::ShuttingDown;
        }

        spdlog::info("CEFService: entering CefShutdown on application thread {}",
                     std::hash<std::thread::id>{}(std::this_thread::get_id()));
        ::CefShutdown();

        {
            std::lock_guard lifecycleLock(s_lifecycleMutex);
            s_clients.clear();
            s_cefApp = nullptr;
            s_initializeThreadId = {};
            s_drainBarrierPosted = false;
            s_lifecycleState = LifecycleState::Stopped;
        }
        s_lifecycleCondition.notify_all();
        spdlog::info("CEFService::CEFShutdown successfully");
    }

    bool CEFService::CreateBrowser(const CefRefPtr<CefClient> a_client,
                                   const CefRefPtr<CefDictionaryValue> a_jsFuncInfo,
                                   const CefString a_url,
                                   const CefWindowInfo& a_cefWindowInfo,
                                   const CefBrowserSettings& a_cefBrowserSettings)
    {
        if (a_client == nullptr)
        {
            spdlog::error("CEFService::CreateBrowser: client is nullptr");
            return false;
        }

        const auto clientKey = a_client.get();
        {
            std::lock_guard lock(s_lifecycleMutex);
            if (s_lifecycleState != LifecycleState::Running)
            {
                spdlog::warn("CEFService::CreateBrowser: rejected because CEF is not running");
                return false;
            }

            auto& state = s_clients[clientKey];
            if (state.creationPending || state.closeRequested || !state.browsers.empty())
            {
                spdlog::error("CEFService::CreateBrowser: client already owns or is creating a browser");
                return false;
            }

            state.client = a_client;
            state.creationPending = true;
        }

        const auto createResult = CefBrowserHost::CreateBrowser(
            a_cefWindowInfo,
            a_client,
            a_url,
            a_cefBrowserSettings,
            a_jsFuncInfo,
            nullptr);

        if (!createResult)
        {
            bool postDrainBarrier = false;
            {
                std::lock_guard lock(s_lifecycleMutex);
                const auto stateIt = s_clients.find(clientKey);
                if (stateIt != s_clients.end())
                {
                    stateIt->second.creationPending = false;
                    if (stateIt->second.browsers.empty())
                    {
                        s_clients.erase(stateIt);
                    }
                }
                postDrainBarrier = ArmDrainBarrierIfNeededLocked();
            }

            if (postDrainBarrier)
            {
                PostDrainBarrier();
            }
        }

        return createResult;
    }

    void CEFService::RequestClientClose(CefRefPtr<CefClient> a_client)
    {
        if (a_client == nullptr)
        {
            return;
        }

        std::vector<CefRefPtr<CefBrowser>> browsersToClose;
        {
            std::lock_guard lock(s_lifecycleMutex);
            const auto stateIt = s_clients.find(a_client.get());
            if (stateIt == s_clients.end())
            {
                return;
            }

            stateIt->second.closeRequested = true;
            for (const auto& browserEntry : stateIt->second.browsers)
            {
                const auto& browser = browserEntry.second;
                if (browser != nullptr)
                {
                    browsersToClose.push_back(browser);
                }
            }
        }

        for (const auto& browser : browsersToClose)
        {
            if (browser != nullptr && browser->IsValid())
            {
                browser->GetHost()->CloseBrowser(true);
            }
        }
    }

    CEFService::BrowserCreationDisposition CEFService::OnBrowserCreated(CefRefPtr<CefClient> a_client,
                                                                        CefRefPtr<CefBrowser> a_browser)
    {
        if (a_client == nullptr || a_browser == nullptr)
        {
            return BrowserCreationDisposition::CloseImmediately;
        }

        BrowserCreationDisposition disposition = BrowserCreationDisposition::KeepOpen;
        {
            std::lock_guard lock(s_lifecycleMutex);
            auto& state = s_clients[a_client.get()];
            state.client = a_client;
            state.creationPending = false;
            state.browsers.insert_or_assign(a_browser->GetIdentifier(), a_browser);
            if (state.closeRequested || s_lifecycleState != LifecycleState::Running)
            {
                disposition = BrowserCreationDisposition::CloseImmediately;
            }
        }

        spdlog::info("CEFService: browser {} registered", a_browser->GetIdentifier());
        return disposition;
    }

    void CEFService::OnBrowserBeforeClose(CefRefPtr<CefClient> a_client, CefRefPtr<CefBrowser> a_browser)
    {
        if (a_client == nullptr || a_browser == nullptr)
        {
            return;
        }

        const auto browserId = a_browser->GetIdentifier();
        bool postDrainBarrier = false;
        {
            std::lock_guard lock(s_lifecycleMutex);
            const auto clientIt = s_clients.find(a_client.get());
            if (clientIt != s_clients.end())
            {
                clientIt->second.browsers.erase(browserId);
                if (!clientIt->second.creationPending && clientIt->second.browsers.empty())
                {
                    s_clients.erase(clientIt);
                }
            }

            postDrainBarrier = ArmDrainBarrierIfNeededLocked();
        }

        spdlog::info("CEFService: browser {} reached OnBeforeClose", browserId);
        if (postDrainBarrier)
        {
            // This task cannot run until the current OnBeforeClose callback has
            // returned, preventing CefShutdown from racing its callback stack.
            PostDrainBarrier();
        }
    }

    CEFService::LifecycleState CEFService::GetLifecycleState()
    {
        std::lock_guard lock(s_lifecycleMutex);
        return s_lifecycleState;
    }

    bool CEFService::IsAcceptingBrowsers()
    {
        return GetLifecycleState() == LifecycleState::Running;
    }
}
