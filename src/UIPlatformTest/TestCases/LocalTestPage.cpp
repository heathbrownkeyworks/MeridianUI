#include "LocalTestPage.h"

namespace Meridian::UI::TestCase
{
    namespace
    {
        // Concatenates already-JSON-encoded per-argument strings (see
        // CefValueToJSONConverter::ConvertToJSONStringArgs -- each a_args[i]
        // is already valid JSON, e.g. a string arg arrives quoted) into a
        // single JSON array literal, so it can be spliced straight into a
        // hand-built JSON payload without re-parsing.
        std::string ArgsToJsonArray(const char** a_args, int a_argsCount)
        {
            std::string result = "[";
            for (auto i = 0; i < a_argsCount; ++i)
            {
                if (i > 0)
                {
                    result += ",";
                }
                result += a_args[i];
            }
            result += "]";
            return result;
        }
    }

    LocalTestPage::LocalTestPage()
    {
        std::lock_guard locker(s_thisVectorMutex);
        s_thisVector.push_back(this);
    }

    LocalTestPage::~LocalTestPage()
    {
        std::lock_guard locker(s_thisVectorMutex);
        for (auto it = s_thisVector.begin(); it != s_thisVector.end(); ++it)
        {
            if (*it == this)
            {
                s_thisVector.erase(it);
                break;
            }
        }
        Shutdown();
    }

    // Three promise-returning native functions exercise
    // IBrowser::AddPromiseFunctionCallback end-to-end from the first trusted
    // mod:// browser. They are registered directly
    // on the browser rather than via AddOrGetBrowser's a_funcInfoArr --
    // that overload only accepts fire-and-forget JSFuncInfo bindings.
    // Promise bindings live per IBrowser instance (like toggle keys), so
    // the warm recreate path must call this again on the replacement.
    void LocalTestPage::RegisterPromiseNatives()
    {
        JS::JSPromiseFuncInfo echoFast;
        echoFast.objectName = "meridianTest";
        echoFast.funcName = "echoFast";
        echoFast.executeInGameThread = false;
        echoFast.callback = [](const char** a_args, int a_argsCount, JS::IJSPromiseResolver* a_resolver) {
            const auto argsJson = ArgsToJsonArray(a_args, a_argsCount);
            spdlog::info("meridianTest.echoFast callback invoked. args: {}", argsJson);
            a_resolver->Resolve(fmt::format(R"({{"echo":{},"path":"fast"}})", argsJson).c_str());
        };
        m_browser->AddPromiseFunctionCallback(echoFast);

        JS::JSPromiseFuncInfo echoSlow;
        echoSlow.objectName = "meridianTest";
        echoSlow.funcName = "echoSlow";
        echoSlow.executeInGameThread = true;
        echoSlow.callback = [](const char** a_args, int a_argsCount, JS::IJSPromiseResolver* a_resolver) {
            const auto argsJson = ArgsToJsonArray(a_args, a_argsCount);
            spdlog::info("meridianTest.echoSlow callback invoked (game thread). args: {}", argsJson);

            // Exercises the resolver's any-thread/any-time contract with a
            // genuinely independent thread rather than the SKSE task/timer
            // idiom used elsewhere in this fixture -- the brief sanctions
            // this as the simplest legitimate exercise of that contract.
            // Capture ONLY the resolver pointer (and a local copy of the
            // already-computed JSON, not fixture state) -- never `this` --
            // because the platform retains every resolver for the process
            // lifetime, so this thread must stay safe to run long after
            // LocalTestPage could have been destroyed.
            std::thread([a_resolver, argsJson]() {
                std::this_thread::sleep_for(1s);
                a_resolver->Resolve(fmt::format(R"({{"echo":{},"path":"slow"}})", argsJson).c_str());
            }).detach();
        };
        m_browser->AddPromiseFunctionCallback(echoSlow);

        JS::JSPromiseFuncInfo alwaysReject;
        alwaysReject.objectName = "meridianTest";
        alwaysReject.funcName = "alwaysReject";
        alwaysReject.executeInGameThread = false;
        alwaysReject.callback = [](const char** a_args, int a_argsCount, JS::IJSPromiseResolver* a_resolver) {
            spdlog::info("meridianTest.alwaysReject callback invoked. argsCount: {}", a_argsCount);
            a_resolver->Reject("intentional test rejection");
        };
        m_browser->AddPromiseFunctionCallback(alwaysReject);

        spdlog::info("{}: registered promise natives meridianTest.{{echoFast, echoSlow, alwaysReject}}",
                     NameOf(LocalTestPage));
    }

    void LocalTestPage::Start(Meridian::UI::IUIPlatformAPI* a_api)
    {
        m_api = a_api;
        a_api->RegisterOnShutdown(&LocalTestPage::StaticShutdown);

        // func1
        auto func1 = new JS::JSFuncInfo();
        func1->objectName = "NL";
        func1->funcName = "func1";
        func1->callbackData.executeInGameThread = false;
        func1->callbackData.callback = [](const char** a_args, int a_argsCount) {
            std::string argsStr = "";
            for (auto i = 0; i < a_argsCount; ++i)
            {
                argsStr += fmt::format("{}{}", (i > 0 ? ", " : ""), a_args[i]);
            }

            spdlog::info("func1 callback. params: {}", argsStr);
        };

        // func2
        auto func2 = new JS::JSFuncInfo();
        func2->objectName = "NL";
        func2->funcName = "func2";
        func2->callbackData.executeInGameThread = true;
        func2->callbackData.callback = [](const char** a_args, int a_argsCount) {
            std::string argsStr = "";
            for (auto i = 0; i < a_argsCount; ++i)
            {
                argsStr += fmt::format("{}{}", (i > 0 ? ", " : ""), a_args[i]);
            }

            spdlog::info("func2 callback. params: {}", argsStr);
        };

        // Event func
        auto func3 = new JS::JSFuncInfo();
        func3->objectName = "NL";
        func3->funcName = "addEventListener";
        func3->callbackData.isEventFunction = true;

        m_funcInfoVector.push_back(func1);
        m_funcInfoVector.push_back(func2);
        m_funcInfoVector.push_back(func3);

        constexpr const char* testPageURL = "mod://MeridianUITest/index.html";
        spdlog::info("{}: loading offline fixture {}", NameOf(LocalTestPage), testPageURL);

        m_browserHandle = a_api->AddOrGetBrowser("LOCAL_TEST_PAGE",
                                                 m_funcInfoVector.data(),
                                                 static_cast<std::uint32_t>(m_funcInfoVector.size()),
                                                 testPageURL,
                                                 m_browser);

        if (m_browserHandle == Meridian::UI::IUIPlatformAPI::InvalidBrowserRefHandle)
        {
            spdlog::error("{}: browser handle is invalid", NameOf(LocalTestPage));
            return;
        }

        if (m_browser == nullptr)
        {
            spdlog::error("{}: browser is nullptr", NameOf(LocalTestPage));
            return;
        }

        // S1 verification aid: F6 toggles browser focus so the cursor-over-browser
        // half of the spike can be tested from the title menu.
        m_browser->ToggleBrowserFocusByKeys(RE::BSKeyboardDevice::Keys::kF6, 0);

        RegisterPromiseNatives();

        // Acceptance fixture: a second, quarter-size browser overlapping the
        // first, above it in z, at half resolution scale. F7 toggles its focus
        // to prove per-rect input and coordinate transforms. It loads via
        // mod://, using a second browser to exercise independent host pinning
        // and overlapping compositor geometry.
        constexpr const char* kModTestPageURL = "mod://MeridianUITest/index.html";
        spdlog::info("{}: second browser loading via mod:// scheme: {}", NameOf(LocalTestPage), kModTestPageURL);

        m_browserHandle2 = a_api->AddOrGetBrowser("LOCAL_TEST_PAGE_2",
                                                  nullptr,
                                                  0,
                                                  kModTestPageURL,
                                                  m_browser2);
        if (m_browserHandle2 != Meridian::UI::IUIPlatformAPI::InvalidBrowserRefHandle && m_browser2 != nullptr)
        {
            m_browser2->SetBrowserRect(80, 80, 640, 360);
            m_browser2->SetBrowserResolutionScale(0.5f);
            m_browser2->SetBrowserZOrder(10);
            m_browser2->ToggleBrowserFocusByKeys(RE::BSKeyboardDevice::Keys::kF7, 0);
            spdlog::info("{}: second browser at (80,80 640x360) scale 0.5 z 10", NameOf(LocalTestPage));
        }
        else
        {
            spdlog::error("{}: second browser creation failed", NameOf(LocalTestPage));
        }

        m_printThread = std::jthread([this](std::stop_token a_stopToken) {
            // func1
            {
                JS::JSFuncInfoString strFunInfo("NL", "func1");
                strFunInfo.callbackData.executeInGameThread = false;
                strFunInfo.callbackData.callback = [](const char** a_args, int a_argsCount) {
                    std::string argsStr = "";
                    for (auto i = 0; i < a_argsCount; ++i)
                    {
                        argsStr += fmt::format("{}{}", (i > 0 ? ", " : ""), a_args[i]);
                    }

                    spdlog::info("func1__ callback. params: {}", argsStr);
                };
                m_browser->AddFunctionCallback(strFunInfo);
            }

            // func2
            {
                JS::JSFuncInfoString strFunInfo("NL", "func2");
                strFunInfo.callbackData.executeInGameThread = false;
                strFunInfo.callbackData.callback = [](const char** a_args, int a_argsCount) {
                    std::string argsStr = "";
                    for (auto i = 0; i < a_argsCount; ++i)
                    {
                        argsStr += fmt::format("{}{}", (i > 0 ? ", " : ""), a_args[i]);
                    }

                    spdlog::info("func2__ callback. params: {}", argsStr);
                };
                m_browser->AddFunctionCallback(strFunInfo);
            }

            while (!a_stopToken.stop_requested() && !m_browser->IsPageLoaded())
            {
                std::this_thread::sleep_for(100ms);
            }

            if (a_stopToken.stop_requested())
            {
                return;
            }

            // m_browser->RemoveFunctionCallback(strFunInfo.objectName, strFunInfo.funcName);

            int i = 0;
            while (i < 10 && !a_stopToken.stop_requested())
            {
                std::this_thread::sleep_for(1s);
                if (a_stopToken.stop_requested())
                {
                    break;
                }
                m_browser->ExecEventFunction("on:message", "EVENT_FUNC WORKS!");
                m_browser->ExecuteJavaScript(fmt::format("NL.func1({})", std::to_string(++i).c_str()).c_str());
            }

            if (a_stopToken.stop_requested())
            {
                return;
            }

            // Run one warm last-handle release/recreate on Skyrim's task thread.
            // The replacement remains live so game exit must exercise the
            // platform-wide close/drain barrier as a separate path.
            auto recreatePromise = std::make_shared<std::promise<bool>>();
            auto recreateFuture = recreatePromise->get_future();
            SKSE::GetTaskInterface()->AddTask([this, recreatePromise]() {
                if (m_shutdownRequested.load(std::memory_order_acquire))
                {
                    recreatePromise->set_value(false);
                    return;
                }

                spdlog::info("{}: starting warm browser release/recreate", NameOf(LocalTestPage));
                if (m_browserHandle != Meridian::UI::IUIPlatformAPI::InvalidBrowserRefHandle)
                {
                    m_api->ReleaseBrowserHandle(m_browserHandle);
                    m_browserHandle = Meridian::UI::IUIPlatformAPI::InvalidBrowserRefHandle;
                    m_browser = nullptr;
                }

                m_browserHandle = m_api->AddOrGetBrowser("LOCAL_TEST_PAGE",
                                                         m_funcInfoVector.data(),
                                                         static_cast<std::uint32_t>(m_funcInfoVector.size()),
                                                         "mod://MeridianUITest/index.html",
                                                         m_browser);
                if (m_browserHandle == Meridian::UI::IUIPlatformAPI::InvalidBrowserRefHandle || m_browser == nullptr)
                {
                    spdlog::error("{}: warm browser recreate failed", NameOf(LocalTestPage));
                    recreatePromise->set_value(false);
                    return;
                }

                // Re-arm F6 on the replacement — toggle keys live per browser instance.
                m_browser->ToggleBrowserFocusByKeys(RE::BSKeyboardDevice::Keys::kF6, 0);

                // Promise bindings live per browser instance too — re-register
                // meridianTest.* so the post-recreate panel clicks hit a live
                // binding instead of an undefined object.
                RegisterPromiseNatives();

                recreatePromise->set_value(true);
            });

            while (!a_stopToken.stop_requested() && recreateFuture.wait_for(100ms) != std::future_status::ready)
            {}

            if (a_stopToken.stop_requested() || !recreateFuture.get())
            {
                return;
            }

            const auto loadDeadline = std::chrono::steady_clock::now() + 30s;
            while (!a_stopToken.stop_requested() && m_browser != nullptr && !m_browser->IsPageLoaded() &&
                   std::chrono::steady_clock::now() < loadDeadline)
            {
                std::this_thread::sleep_for(100ms);
            }

            if (a_stopToken.stop_requested())
            {
                return;
            }

            if (m_browser == nullptr || !m_browser->IsPageLoaded())
            {
                spdlog::error("{}: replacement page did not load within 30 seconds", NameOf(LocalTestPage));
                return;
            }

            spdlog::info("{}: warm browser release/recreate complete; replacement loaded and retained for exit drain",
                         NameOf(LocalTestPage));

            // Remove-binding acceptance check: func1 must become uncallable while func2 stays bound.
            m_browser->RemoveFunctionCallback("NL", "func1");

            std::this_thread::sleep_for(500ms);
            if (a_stopToken.stop_requested())
            {
                return;
            }

            m_browser->ExecuteJavaScript("try { NL.func1(1); NL.func2('remove-check: func1 STILL CALLABLE'); } catch (e) { NL.func2('remove-check: ' + e.name); }");
            spdlog::info("{}: remove-binding check dispatched; expect func2 callback logging 'remove-check: TypeError'", NameOf(LocalTestPage));
        });
    }

    void LocalTestPage::Shutdown()
    {
        if (m_shutdownRequested.exchange(true, std::memory_order_acq_rel))
        {
            return;
        }

        spdlog::info("LocalTestPage::Shutdown");

        if (m_printThread.joinable())
        {
            m_printThread.request_stop();
            m_printThread.join();
        }

        if (m_api != nullptr && m_browserHandle != Meridian::UI::IUIPlatformAPI::InvalidBrowserRefHandle)
        {
            // This fixture deliberately leaves one live browser owned by the platform so the
            // central shutdown coordinator must close and drain it before CefShutdown.
            m_browserHandle = Meridian::UI::IUIPlatformAPI::InvalidBrowserRefHandle;
            m_browser = nullptr;
            spdlog::info("{}: retained offline browser for platform shutdown drain", NameOf(LocalTestPage));
        }

        if (m_api != nullptr && m_browserHandle2 != Meridian::UI::IUIPlatformAPI::InvalidBrowserRefHandle)
        {
            // Same deliberate retention as the first handle: the platform-wide shutdown
            // coordinator closes and drains this browser before CefShutdown.
            m_browserHandle2 = Meridian::UI::IUIPlatformAPI::InvalidBrowserRefHandle;
            m_browser2 = nullptr;
            spdlog::info("{}: retained second offline browser for platform shutdown drain", NameOf(LocalTestPage));
        }
    }
}
