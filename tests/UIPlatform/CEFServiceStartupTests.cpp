#include "Services/CEFService.h"

#include <array>
#include <iostream>

namespace
{
    using Service = Meridian::Services::CEFService;
    int g_failures = 0;

    void Expect(bool a_condition, const char* a_message)
    {
        if (!a_condition)
        {
            ++g_failures;
            std::cerr << "FAILED: " << a_message << '\n';
        }
    }

    std::string TryInitialize()
    {
        try
        {
            Service::CEFInitialize(std::make_shared<CefApp>(), CefSettings{});
            return {};
        }
        catch (const std::runtime_error& error)
        {
            return error.what();
        }
    }

    void Failure(int a_exitCode)
    {
        CefCalls::resultCode = a_exitCode;
        const auto codeText = std::format("code {}", a_exitCode);
        Expect(TryInitialize().find(codeText) != std::string::npos, "initial failure reports the CEF exit code");
        Expect(!Service::IsAcceptingBrowsers(), "failed CEF does not accept browsers");
        Expect(CefCalls::initialize == 1 && CefCalls::exitCode == 1, "failure attempts initialization and reads its code exactly once");
        Expect(CefCalls::Total() == 2, "failure does not register a scheme or call other CEF APIs");
        const auto callsAfterFailure = CefCalls::Total();

        // Make a forbidden second initialization appear successful. A retry
        // must still be rejected, regardless of what CEF would return next.
        CefCalls::initializeResult = true;
        std::array<std::string, 8> errors;
        std::array<std::thread, 8> callers;
        for (std::size_t i = 0; i < callers.size(); ++i)
        {
            callers[i] = std::thread([i, &errors]() { errors[i] = TryInitialize(); });
        }
        for (auto& caller : callers) caller.join();
        for (const auto& error : errors)
        {
            Expect(error.find(codeText) != std::string::npos, "concurrent retries preserve the original failure code");
            Expect(error.find("restart Skyrim") != std::string::npos, "retry diagnostic requires a new Skyrim session");
        }

        Expect(!Service::CreateBrowser(std::make_shared<CefClient>(), nullptr, "mod://horde/index.html", {}, {}),
               "browser creation is rejected after initialization failure");
        Service::CloseAllBrowsersAndWait(std::chrono::milliseconds(1));
        try { Service::CEFShutdown(); }
        catch (const std::exception&) { Expect(false, "cleanup of failed initialization does not throw"); }
        Expect(TryInitialize().find(codeText) != std::string::npos, "cleanup cannot reset the failure latch");
        Expect(CefCalls::Total() == callsAfterFailure, "retries, browser requests, and cleanup make no further CEF calls");
    }

    void Success()
    {
        CefCalls::initializeResult = true;
        Expect(TryInitialize().empty(), "successful CEF initialization succeeds");
        Expect(Service::IsAcceptingBrowsers(), "successful CEF accepts browsers");
        Expect(CefCalls::initialize == 1 && CefCalls::registerScheme == 1 && CefCalls::exitCode == 0,
               "success initializes and registers the mod scheme without querying a failure code");
        const auto callsAfterSuccess = CefCalls::Total();
        Expect(!TryInitialize().empty(), "duplicate initialization is rejected");
        Expect(CefCalls::Total() == callsAfterSuccess, "duplicate initialization makes no CEF calls");
        Expect(Service::CloseAllBrowsersAndWait(std::chrono::milliseconds(1)), "successful session drains its close barrier");
        Service::CEFShutdown();
        Expect(Service::GetLifecycleState() == Service::LifecycleState::Stopped, "successful shutdown reaches Stopped");
        Expect(CefCalls::postTask == 1 && CefCalls::shutdown == 1, "successful shutdown posts one barrier and calls CEF shutdown once");
        const auto callsAfterShutdown = CefCalls::Total();
        Service::CEFShutdown();
        Expect(!TryInitialize().empty(), "stopped CEF cannot be reinitialized");
        Expect(CefCalls::Total() == callsAfterShutdown, "stopped session makes no more CEF calls");
    }
}

int main(int argc, char** argv)
{
    if (argc != 2) return 2;
    const std::string scenario = argv[1];
    if (scenario == "success") Success();
    else if (scenario == "code38") Failure(38);
    else if (scenario == "code1") Failure(1);
    else return 2;
    if (g_failures != 0) return 1;
    std::cout << "CEF startup scenario passed: " << scenario << '\n';
    return 0;
}
