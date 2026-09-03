#include "ProcessDownDetector.h"

#include <charconv>
#include <limits>
#include <memory>
#include <utility>

namespace Meridian
{
    namespace
    {
        constexpr DWORD PROCESS_WAIT_INTERVAL_MS = 100;

        struct HandleCloser
        {
            void operator()(void* a_handle) const noexcept
            {
                if (a_handle != nullptr && a_handle != INVALID_HANDLE_VALUE)
                {
                    ::CloseHandle(a_handle);
                }
            }
        };

        using UniqueHandle = std::unique_ptr<void, HandleCloser>;

        void ReportFailure(const ProcessDownDetector::OnFailureCallback& a_callback, const ProcessWatchError& a_error) noexcept
        {
            if (!a_callback)
            {
                return;
            }

            try
            {
                a_callback(a_error);
            }
            catch (...)
            {
                ::OutputDebugStringA("[MeridianCEFSubprocess] process-watch failure callback threw an exception\n");
            }
        }

        void ReportProcessDown(const ProcessDownDetector::OnProcessDownCallback& a_callback) noexcept
        {
            if (!a_callback)
            {
                return;
            }

            try
            {
                a_callback();
            }
            catch (...)
            {
                ::OutputDebugStringA("[MeridianCEFSubprocess] process-down callback threw an exception\n");
            }
        }
    }

    std::optional<DWORD> Detail::ParseProcessId(std::string_view a_value) noexcept
    {
        if (a_value.empty())
        {
            return std::nullopt;
        }

        unsigned long long value = 0;
        const auto* const begin = a_value.data();
        const auto* const end = begin + a_value.size();
        const auto [parseEnd, error] = std::from_chars(begin, end, value);
        if (error != std::errc{} || parseEnd != end || value == 0 || value > std::numeric_limits<DWORD>::max())
        {
            return std::nullopt;
        }

        return static_cast<DWORD>(value);
    }

    ProcessWaitOutcome Detail::WaitForHandle(HANDLE a_handle,
                                             std::stop_token a_stopToken,
                                             OnWaitingCallback a_onWaiting) noexcept
    {
        if (a_handle == nullptr || a_handle == INVALID_HANDLE_VALUE)
        {
            return {ProcessWaitResult::Failed, WAIT_FAILED, ERROR_INVALID_HANDLE};
        }

        bool reportedWaiting = false;
        while (!a_stopToken.stop_requested())
        {
            ::SetLastError(ERROR_SUCCESS);
            const auto waitResult = ::WaitForSingleObject(a_handle, PROCESS_WAIT_INTERVAL_MS);
            switch (waitResult)
            {
            case WAIT_OBJECT_0:
                if (a_stopToken.stop_requested())
                {
                    return {ProcessWaitResult::Stopped, waitResult, ERROR_SUCCESS};
                }
                return {ProcessWaitResult::Signaled, waitResult, ERROR_SUCCESS};
            case WAIT_TIMEOUT:
                if (!reportedWaiting && a_onWaiting)
                {
                    reportedWaiting = true;
                    try
                    {
                        a_onWaiting();
                    }
                    catch (...)
                    {
                        ::OutputDebugStringA("[MeridianCEFSubprocess] process-watch waiting callback threw an exception\n");
                    }
                }
                break;
            case WAIT_FAILED:
                return {ProcessWaitResult::Failed, waitResult, ::GetLastError()};
            default:
                return {ProcessWaitResult::Failed, waitResult, ERROR_INVALID_FUNCTION};
            }
        }

        return {ProcessWaitResult::Stopped, WAIT_TIMEOUT, ERROR_SUCCESS};
    }

    ProcessDownDetector::ProcessDownDetector(DWORD a_processId,
                                             OnProcessDownCallback a_callback,
                                             OnFailureCallback a_failureCallback,
                                             OnWaitingCallback a_onWaiting,
                                             OnBeforeOpenCallback a_onBeforeOpen) :
        m_threadDetector([a_processId,
                          callback = std::move(a_callback),
                          failureCallback = std::move(a_failureCallback),
                          onWaiting = std::move(a_onWaiting),
                          onBeforeOpen = std::move(a_onBeforeOpen)](std::stop_token a_stopToken) mutable {
            if (onBeforeOpen)
            {
                try
                {
                    onBeforeOpen(a_stopToken);
                }
                catch (...)
                {
                    ::OutputDebugStringA("[MeridianCEFSubprocess] process-watch before-open callback threw an exception\n");
                }
            }

            if (a_stopToken.stop_requested())
            {
                return;
            }

            const auto rawProcessHandle = ::OpenProcess(SYNCHRONIZE, FALSE, a_processId);
            if (rawProcessHandle == nullptr)
            {
                const auto error = ::GetLastError();
                if (a_processId != 0 && error == ERROR_INVALID_PARAMETER)
                {
                    // A valid positive PID that no longer identifies a process means the
                    // parent disappeared before this helper could acquire its wait handle.
                    if (!a_stopToken.stop_requested())
                    {
                        ReportProcessDown(callback);
                    }
                    return;
                }

                ReportFailure(failureCallback, {ProcessWatchFailure::OpenProcess, WAIT_FAILED, error});
                return;
            }

            UniqueHandle processHandle(rawProcessHandle);
            const auto outcome = Detail::WaitForHandle(processHandle.get(), a_stopToken, std::move(onWaiting));
            processHandle.reset();

            if (outcome.result == ProcessWaitResult::Failed)
            {
                ReportFailure(failureCallback,
                              {ProcessWatchFailure::WaitForSingleObject, outcome.waitResult, outcome.win32Error});
                return;
            }

            if (outcome.result == ProcessWaitResult::Signaled && !a_stopToken.stop_requested())
            {
                ReportProcessDown(callback);
            }
        })
    {}
}
