#include "CEF/MeridianSubprocessCefApp.h"
#include "ProcessDownDetector.h"

#include <cstdlib>

namespace
{
    void WriteProcessWatchLog(const std::string& a_message)
    {
        const auto message = "[MeridianCEFSubprocess][" + std::to_string(::GetCurrentProcessId()) + "] " + a_message + "\n";
        ::OutputDebugStringA(message.c_str());
    }

    const char* GetFailureName(Meridian::ProcessWatchFailure a_failure)
    {
        switch (a_failure)
        {
        case Meridian::ProcessWatchFailure::OpenProcess:
            return "OpenProcess";
        case Meridian::ProcessWatchFailure::WaitForSingleObject:
            return "WaitForSingleObject";
        default:
            return "Unknown";
        }
    }
}

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nShowCmd)
{
    void* sandbox_info = nullptr;
#if defined(CEF_USE_SANDBOX)
    // Manage the life span of the sandbox information object. This is necessary
    // for sandbox support on Windows. See cef_sandbox_win.h for complete details.
    CefScopedSandboxInfo scoped_sandbox;
    sandbox_info = scoped_sandbox.sandbox_info();
#endif

    auto cmdLine = CefCommandLine::CreateCommandLine();
    cmdLine->InitFromString(GetCommandLineW());

    std::unique_ptr<Meridian::ProcessDownDetector> mainProcessDownDetector;
    if (cmdLine->HasSwitch(IPC_CL_PROCESS_ID_NAME))
    {
        const auto mainProcessId = Meridian::Detail::ParseProcessId(cmdLine->GetSwitchValue(IPC_CL_PROCESS_ID_NAME).ToString());
        if (mainProcessId.has_value())
        {
            auto processType = cmdLine->GetSwitchValue("type").ToString();
            if (processType.empty())
            {
                processType = "unknown";
            }

            WriteProcessWatchLog("process watch armed: type=" + processType + " parentPid=" + std::to_string(*mainProcessId));
            mainProcessDownDetector = std::make_unique<Meridian::ProcessDownDetector>(
                *mainProcessId,
                []() {
                    WriteProcessWatchLog("parent exited; terminating helper");
                    if (!::TerminateProcess(::GetCurrentProcess(), EXIT_SUCCESS))
                    {
                        std::_Exit(EXIT_FAILURE);
                    }
                },
                [parentProcessId = *mainProcessId](const Meridian::ProcessWatchError& a_error) {
                    WriteProcessWatchLog("process watch disabled: operation=" + std::string(GetFailureName(a_error.failure)) +
                                         " parentPid=" + std::to_string(parentProcessId) +
                                         " waitResult=" + std::to_string(a_error.waitResult) +
                                         " win32Error=" + std::to_string(a_error.win32Error));
                });
        }
        else
        {
            WriteProcessWatchLog("process watch disabled: invalid parent PID");
        }
    }
    else
    {
        WriteProcessWatchLog("process watch disabled: parent PID switch missing");
    }

    CefMainArgs main_args(hInstance);
    CefRefPtr<Meridian::CEF::MeridianSubprocessCefApp> app(new Meridian::CEF::MeridianSubprocessCefApp());

    const auto exitCode = CefExecuteProcess(main_args, app, sandbox_info);
    mainProcessDownDetector.reset();
    WriteProcessWatchLog("CefExecuteProcess returned: exitCode=" + std::to_string(exitCode));
    return exitCode;
}
