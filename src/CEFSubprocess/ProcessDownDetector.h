#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <functional>
#include <optional>
#include <stop_token>
#include <string_view>
#include <thread>

namespace Meridian
{
    enum class ProcessWaitResult
    {
        Stopped,
        Signaled,
        Failed
    };

    struct ProcessWaitOutcome
    {
        ProcessWaitResult result{ProcessWaitResult::Failed};
        DWORD waitResult{WAIT_FAILED};
        DWORD win32Error{ERROR_SUCCESS};
    };

    enum class ProcessWatchFailure
    {
        OpenProcess,
        WaitForSingleObject
    };

    struct ProcessWatchError
    {
        ProcessWatchFailure failure{ProcessWatchFailure::OpenProcess};
        DWORD waitResult{WAIT_FAILED};
        DWORD win32Error{ERROR_SUCCESS};
    };

    namespace Detail
    {
        using OnWaitingCallback = std::function<void()>;

        [[nodiscard]] std::optional<DWORD> ParseProcessId(std::string_view a_value) noexcept;
        [[nodiscard]] ProcessWaitOutcome WaitForHandle(HANDLE a_handle,
                                                       std::stop_token a_stopToken,
                                                       OnWaitingCallback a_onWaiting = {}) noexcept;
    }

    class ProcessDownDetector final
    {
    public:
        using OnProcessDownCallback = std::function<void()>;
        using OnFailureCallback = std::function<void(const ProcessWatchError&)>;
        using OnWaitingCallback = Detail::OnWaitingCallback;
        using OnBeforeOpenCallback = std::function<void(std::stop_token)>;

        ProcessDownDetector(DWORD a_processId,
                            OnProcessDownCallback a_callback,
                            OnFailureCallback a_failureCallback = {},
                            OnWaitingCallback a_onWaiting = {},
                            OnBeforeOpenCallback a_onBeforeOpen = {});
        ~ProcessDownDetector() = default;

        ProcessDownDetector(const ProcessDownDetector&) = delete;
        ProcessDownDetector(ProcessDownDetector&&) = delete;
        ProcessDownDetector& operator=(const ProcessDownDetector&) = delete;
        ProcessDownDetector& operator=(ProcessDownDetector&&) = delete;

    private:
        std::jthread m_threadDetector;
    };
}
