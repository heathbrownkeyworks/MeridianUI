#define DLLEXPORT __declspec(dllexport)

/* disable headers in Windows.h */
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOMINMAX

// std
#include <string>
#include <filesystem>

using namespace std::literals;
using namespace std::string_literals;

// Fmt
#include "fmt/format.h"

// CommonLibSSE
#include <RE/Skyrim.h>
#include <REL/Relocation.h>
#include <SKSE/Impl/Stubs.h>
#include <SKSE/SKSE.h>

// Win
#include <Windows.h>

// spdlog
#include <spdlog/spdlog.h>
#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/sinks/basic_file_sink.h>

// this
#include "Version.h"
#include "Common/RuntimePaths.h"

using EntryFunc = bool (*)(const SKSE::LoadInterface* a_skse);
using PreloadFunc = void (*)();

inline void ShowMessageBox(const char* a_msg)
{
    MessageBoxA(0, a_msg, "ERROR", MB_ICONERROR);
}

void InitDefaultLog()
{
    static bool s_loggerInited = false;

    if (s_loggerInited)
    {
        return;
    }

    Meridian::Log::InitOptions options{};
    options.name = "global log"s;
    options.logFileStem = "MeridianUIPlugin.log";
    options.useDebugMsvcSink = false;
    options.alwaysAddFileSink = true;
    options.debugLevel = Meridian::Log::LogLevel::Info;
    if (Meridian::Log::Init(options) == nullptr)
    {
        ShowMessageBox("Failed to initialize MeridianUIPlugin logger");
    }
    s_loggerInited = true;
}

void LogError(const char* a_error)
{
    InitDefaultLog();
    LOG_ERROR("{}", a_error);
}

void LogError(std::string&& a_error)
{
    LogError(a_error.data());
}

std::string GetLastErrorAsString()
{
    // Get the error message ID, if any.
    DWORD errorMessageID = ::GetLastError();
    if (errorMessageID == 0)
    {
        return std::string(); // No error message has been recorded
    }

    LPSTR messageBuffer = nullptr;

    // Ask Win32 to give us the string version of that message ID.
    // The parameters we pass in, tell Win32 to create the buffer that holds the message for us (because we don't yet know how long the message string will be).
    size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                 NULL,
                                 errorMessageID,
                                 MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                                 (LPSTR)&messageBuffer,
                                 0,
                                 NULL);

    // Copy the error message into a std::string.
    std::string message(messageBuffer, size);

    // Free the Win32's string's buffer.
    LocalFree(messageBuffer);

    return message;
}

static inline std::filesystem::path GetUIRelPath()
{
    return Meridian::Paths::MeridianRoot(Meridian::Paths::GameRoot());
}

HMODULE g_meridianUILib = nullptr;
static inline void LoadMeridianUILib()
{
    if (g_meridianUILib == nullptr)
    {
        const auto uiRoot = GetUIRelPath();
        auto libraryName = std::filesystem::path(NL_UI_LIB_NAME);
        libraryName.replace_extension(L".dll");
        const auto libraryPath = uiRoot / libraryName;

        const auto dllDirectory = ::AddDllDirectory(uiRoot.c_str());
        if (dllDirectory == nullptr)
        {
            throw std::runtime_error(std::format("AddDllDirectory failed for \"{}\": {}", uiRoot.string(), GetLastErrorAsString()));
        }

        const auto meridianUILib = ::LoadLibraryExW(
            libraryPath.c_str(),
            nullptr,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_USER_DIRS | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        ::RemoveDllDirectory(dllDirectory);
        if (!meridianUILib)
        {
            std::string errMsg;
            const auto errCode = GetLastError();
            switch (errCode)
            {
            // Not found
            case 126:
                errMsg = std::format("{} not found", libraryPath.string());
                break;
            default:
                errMsg = std::format("Failed to LoadLibraryExW(\"{}\"), error code: {}, desc: \"{}\"", libraryPath.string(), errCode, GetLastErrorAsString());
                break;
            }

            throw std::runtime_error(errMsg);
        }
        else
        {
            g_meridianUILib = meridianUILib;
        }
    }
}

template<class TFunc>
static inline TFunc ExecLibFunc(const char* a_funcName)
{
    LoadMeridianUILib();

    auto func = reinterpret_cast<TFunc>(GetProcAddress(g_meridianUILib, a_funcName));
    if (!func)
    {
        auto errMsg = std::format("{} \"{}\" function not found", NL_UI_LIB_NAME, a_funcName);
        throw std::runtime_error(errMsg);
    }

    return func;
}

extern "C" void DLLEXPORT APIENTRY Initialize()
{
    try
    {
        auto preload = ExecLibFunc<PreloadFunc>("Initialize");
        if (!preload)
        {
            LogError(std::format("Failed to ExecLibFunc<PreloadFunc>(), {}", GetLastErrorAsString().data()));
            return;
        }

        preload();
    }
    catch (const std::exception& e)
    {
        LogError(std::format("Exception while Initialize(), {}, desc {}", e.what(), GetLastErrorAsString().data()));
        ShowMessageBox(e.what());
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
    if (a_skse->IsEditor())
    {
        return false;
    }

    try
    {
        auto entry = ExecLibFunc<EntryFunc>("Entry");
        if (!entry)
        {
            LogError(std::format("Failed to ExecLibFunc<EntryFunc>(), {}", GetLastErrorAsString().data()));
            return false;
        }

        return entry(a_skse);
    }
    catch (const std::exception& e)
    {
        LogError(std::format("Exception while SKSEPlugin_Load(), {}, desc", e.what(), GetLastErrorAsString().data()));
        ShowMessageBox(e.what());
        return false;
    }
}
