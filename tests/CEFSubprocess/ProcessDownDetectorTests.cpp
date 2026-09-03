#include "ProcessDownDetector.h"

#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <limits>
#include <memory>
#include <stop_token>
#include <string>
#include <vector>

namespace
{
    using namespace std::chrono_literals;

    int g_failureCount = 0;

    void Expect(bool a_condition, const char* a_message)
    {
        if (!a_condition)
        {
            ++g_failureCount;
            std::cerr << "FAILED: " << a_message << '\n';
        }
    }

    void TestParseProcessId()
    {
        Expect(!Meridian::Detail::ParseProcessId("").has_value(), "empty PID must be rejected");
        Expect(!Meridian::Detail::ParseProcessId("0").has_value(), "zero PID must be rejected");
        Expect(!Meridian::Detail::ParseProcessId("-1").has_value(), "negative PID must be rejected");
        Expect(!Meridian::Detail::ParseProcessId("+1").has_value(), "signed PID must be rejected");
        Expect(!Meridian::Detail::ParseProcessId(" 1").has_value(), "PID with whitespace must be rejected");
        Expect(!Meridian::Detail::ParseProcessId("123abc").has_value(), "PID with trailing text must be rejected");
        Expect(!Meridian::Detail::ParseProcessId("4294967296").has_value(), "overflowing PID must be rejected");

        const auto parsed = Meridian::Detail::ParseProcessId("1234");
        Expect(parsed.has_value() && *parsed == 1234, "valid PID must be parsed");

        const auto maxPid = Meridian::Detail::ParseProcessId(std::to_string(std::numeric_limits<DWORD>::max()));
        Expect(maxPid.has_value() && *maxPid == std::numeric_limits<DWORD>::max(), "maximum DWORD PID must be parsed");
    }

    void TestWaitForSignaledHandle()
    {
        const auto eventHandle = ::CreateEventW(nullptr, TRUE, TRUE, nullptr);
        Expect(eventHandle != nullptr, "signaled test event must be created");
        if (eventHandle == nullptr)
        {
            return;
        }

        std::stop_source stopSource;
        const auto outcome = Meridian::Detail::WaitForHandle(eventHandle, stopSource.get_token());
        Expect(outcome.result == Meridian::ProcessWaitResult::Signaled, "signaled handle must report Signaled");
        Expect(outcome.waitResult == WAIT_OBJECT_0, "signaled handle must preserve WAIT_OBJECT_0");
        Expect(outcome.win32Error == ERROR_SUCCESS, "signaled handle must not report an error");
        ::CloseHandle(eventHandle);
    }

    void TestWaitStopsWithoutSignal()
    {
        const auto eventHandle = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
        Expect(eventHandle != nullptr, "unsignaled test event must be created");
        if (eventHandle == nullptr)
        {
            return;
        }

        std::stop_source stopSource;
        stopSource.request_stop();
        const auto outcome = Meridian::Detail::WaitForHandle(eventHandle, stopSource.get_token());
        Expect(outcome.result == Meridian::ProcessWaitResult::Stopped, "stop request must report Stopped");
        ::CloseHandle(eventHandle);
    }

    void TestWaitFailureDoesNotLookSignaled()
    {
        std::stop_source stopSource;
        const auto outcome = Meridian::Detail::WaitForHandle(INVALID_HANDLE_VALUE, stopSource.get_token());
        Expect(outcome.result == Meridian::ProcessWaitResult::Failed, "invalid handle must report Failed");
        Expect(outcome.waitResult == WAIT_FAILED, "invalid handle must preserve WAIT_FAILED");
        Expect(outcome.win32Error == ERROR_INVALID_HANDLE, "invalid handle must report ERROR_INVALID_HANDLE");
    }

    void TestOpenFailureDoesNotInvokeProcessDown()
    {
        std::atomic_int processDownCount = 0;
        std::promise<Meridian::ProcessWatchError> failurePromise;
        auto failureFuture = failurePromise.get_future();

        Meridian::ProcessDownDetector detector(
            0,
            [&processDownCount]() {
                ++processDownCount;
            },
            [&failurePromise](const Meridian::ProcessWatchError& a_error) {
                failurePromise.set_value(a_error);
            });

        Expect(failureFuture.wait_for(1s) == std::future_status::ready, "OpenProcess failure must be reported");
        if (failureFuture.wait_for(0s) == std::future_status::ready)
        {
            const auto error = failureFuture.get();
            Expect(error.failure == Meridian::ProcessWatchFailure::OpenProcess, "failure stage must be OpenProcess");
            Expect(error.win32Error == ERROR_INVALID_PARAMETER, "PID zero must report ERROR_INVALID_PARAMETER");
        }
        Expect(processDownCount.load() == 0, "OpenProcess failure must not invoke process-down callback");
    }

    void TestMissingPositiveParentInvokesProcessDown()
    {
        std::atomic_int processDownCount = 0;
        std::atomic_int failureCount = 0;
        std::promise<void> processDownPromise;
        auto processDownFuture = processDownPromise.get_future();

        Meridian::ProcessDownDetector detector(
            std::numeric_limits<DWORD>::max(),
            [&processDownCount, &processDownPromise]() {
                if (++processDownCount == 1)
                {
                    processDownPromise.set_value();
                }
            },
            [&failureCount](const Meridian::ProcessWatchError&) {
                ++failureCount;
            });

        Expect(processDownFuture.wait_for(1s) == std::future_status::ready,
               "a missing positive parent PID must be treated as already exited");
        Expect(processDownCount.load() == 1, "a missing positive parent must invoke process-down exactly once");
        Expect(failureCount.load() == 0, "a missing positive parent must not be reported as a monitor failure");
    }

    void TestCancellationBeforeOpenSuppressesMissingParentCallback()
    {
        std::atomic_int processDownCount = 0;
        std::atomic_int failureCount = 0;
        std::promise<void> beforeOpenPromise;
        auto beforeOpenFuture = beforeOpenPromise.get_future();

        auto detector = std::make_unique<Meridian::ProcessDownDetector>(
            std::numeric_limits<DWORD>::max(),
            [&processDownCount]() {
                ++processDownCount;
            },
            [&failureCount](const Meridian::ProcessWatchError&) {
                ++failureCount;
            },
            Meridian::ProcessDownDetector::OnWaitingCallback{},
            [&beforeOpenPromise](std::stop_token a_stopToken) {
                beforeOpenPromise.set_value();
                while (!a_stopToken.stop_requested())
                {
                    std::this_thread::yield();
                }
            });

        Expect(beforeOpenFuture.wait_for(1s) == std::future_status::ready,
               "cancel-before-open test must reach its synchronization seam");
        detector.reset();

        Expect(processDownCount.load() == 0, "cancellation before OpenProcess must suppress parent-down callback");
        Expect(failureCount.load() == 0, "cancellation before OpenProcess must not report a monitor failure");
    }

    void TestDetectorDestructionDoesNotInvokeProcessDown()
    {
        std::atomic_int processDownCount = 0;
        std::promise<void> waitingPromise;
        auto waitingFuture = waitingPromise.get_future();
        auto detector = std::make_unique<Meridian::ProcessDownDetector>(
            ::GetCurrentProcessId(),
            [&processDownCount]() {
                ++processDownCount;
            },
            Meridian::ProcessDownDetector::OnFailureCallback{},
            [&waitingPromise]() {
                waitingPromise.set_value();
            });

        Expect(waitingFuture.wait_for(1s) == std::future_status::ready,
               "detector must enter the wait loop before cancellation");

        const auto startedAt = std::chrono::steady_clock::now();
        detector.reset();
        const auto elapsed = std::chrono::steady_clock::now() - startedAt;

        Expect(processDownCount.load() == 0, "cancelling a waiting detector must not invoke process-down callback");
        Expect(elapsed < 1s, "cancelling a waiting detector must stop and join promptly");
    }

    void TestControlledChildExitInvokesProcessDownOnce()
    {
        std::vector<wchar_t> executablePath(32768, L'\0');
        const auto pathLength = ::GetModuleFileNameW(nullptr,
                                                     executablePath.data(),
                                                     static_cast<DWORD>(executablePath.size()));
        Expect(pathLength > 0 && pathLength < executablePath.size(), "test executable path must be available");
        if (pathLength == 0 || pathLength >= executablePath.size())
        {
            return;
        }

        std::wstring commandLine = L"\"" + std::wstring(executablePath.data(), pathLength) + L"\" --child";
        std::vector<wchar_t> mutableCommandLine(commandLine.begin(), commandLine.end());
        mutableCommandLine.push_back(L'\0');

        STARTUPINFOW startupInfo{};
        startupInfo.cb = sizeof(startupInfo);
        PROCESS_INFORMATION processInfo{};
        const auto created = ::CreateProcessW(nullptr,
                                              mutableCommandLine.data(),
                                              nullptr,
                                              nullptr,
                                              FALSE,
                                              CREATE_NO_WINDOW,
                                              nullptr,
                                              nullptr,
                                              &startupInfo,
                                              &processInfo);
        Expect(created != FALSE, "controlled child process must start");
        if (!created)
        {
            return;
        }

        std::atomic_int processDownCount = 0;
        std::atomic_int failureCount = 0;
        std::promise<void> processDownPromise;
        auto processDownFuture = processDownPromise.get_future();
        {
            Meridian::ProcessDownDetector detector(
                processInfo.dwProcessId,
                [&processDownCount, &processDownPromise]() {
                    if (++processDownCount == 1)
                    {
                        processDownPromise.set_value();
                    }
                },
                [&failureCount](const Meridian::ProcessWatchError&) {
                    ++failureCount;
                });

            Expect(processDownFuture.wait_for(2s) == std::future_status::ready,
                   "controlled child exit must invoke process-down callback");
        }

        ::WaitForSingleObject(processInfo.hProcess, 2'000);
        ::CloseHandle(processInfo.hThread);
        ::CloseHandle(processInfo.hProcess);

        Expect(processDownCount.load() == 1, "controlled child exit must invoke callback exactly once");
        Expect(failureCount.load() == 0, "controlled child exit must not report monitor failure");
    }

    void TestRepeatedCancellationDoesNotLeakHandles()
    {
        DWORD handlesBefore = 0;
        DWORD handlesAfter = 0;
        Expect(::GetProcessHandleCount(::GetCurrentProcess(), &handlesBefore) != FALSE,
               "initial process handle count must be available");

        std::atomic_int processDownCount = 0;
        for (int iteration = 0; iteration < 100; ++iteration)
        {
            Meridian::ProcessDownDetector detector(::GetCurrentProcessId(), [&processDownCount]() {
                ++processDownCount;
            });
        }

        Expect(::GetProcessHandleCount(::GetCurrentProcess(), &handlesAfter) != FALSE,
               "final process handle count must be available");
        Expect(processDownCount.load() == 0, "repeated cancellation must never report parent death");
        Expect(handlesAfter <= handlesBefore + 2, "repeated cancellation must not leak process handles");
    }
}

int main(int a_argc, char** a_argv)
{
    if (a_argc == 2 && std::string_view(a_argv[1]) == "--child")
    {
        ::Sleep(50);
        return 0;
    }

    TestParseProcessId();
    TestWaitForSignaledHandle();
    TestWaitStopsWithoutSignal();
    TestWaitFailureDoesNotLookSignaled();
    TestOpenFailureDoesNotInvokeProcessDown();
    TestMissingPositiveParentInvokesProcessDown();
    TestCancellationBeforeOpenSuppressesMissingParentCallback();
    TestDetectorDestructionDoesNotInvokeProcessDown();
    TestControlledChildExitInvokesProcessDownOnce();
    TestRepeatedCancellationDoesNotLeakHandles();

    if (g_failureCount != 0)
    {
        std::cerr << g_failureCount << " ProcessDownDetector test(s) failed\n";
        return 1;
    }

    std::cout << "All ProcessDownDetector tests passed\n";
    return 0;
}
