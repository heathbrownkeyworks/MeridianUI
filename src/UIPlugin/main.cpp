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

namespace logger = SKSE::log;

// Win
#include <Windows.h>

// spdlog
#include <spdlog/spdlog.h>
#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/sinks/basic_file_sink.h>

// this
#include "Version.h"
#include "UIPlatform/RuntimeCompatibility.h"
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

    const auto level = spdlog::level::info;
    auto path = logger::log_directory();
    if (!path)
    {
        SKSE::stl::report_and_fail("Failed to find standard logging directory"sv);
    }

    *path /= fmt::format("{}.log"sv, "MeridianUIPlugin");
    auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);

    auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));
    log->set_level(level);
    log->flush_on(level);

    spdlog::set_default_logger(std::move(log));
    spdlog::set_pattern("[%T.%e] [%^%l%$] : %v"s);
    s_loggerInited = true;
}

void LogError(const char* a_error)
{
    InitDefaultLog();
    spdlog::error(a_error);
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

// AE (1.6+) plugin declaration. Version-independent via Address Library.
extern "C" DLLEXPORT constinit auto SKSEPlugin_Version =
    Meridian::RuntimeCompatibility::MakePluginVersionData(
        Meridian::UI::LibVersion::AS_INT,
        Meridian::UI::LibVersion::PROJECT_NAME);

static_assert(Meridian::RuntimeCompatibility::HasAddressLibraryV5(
    Meridian::RuntimeCompatibility::MakePluginVersionData(
        Meridian::UI::LibVersion::AS_INT,
        Meridian::UI::LibVersion::PROJECT_NAME)));
static_assert(Meridian::RuntimeCompatibility::HasUpdatedStructs(
    Meridian::RuntimeCompatibility::MakePluginVersionData(
        Meridian::UI::LibVersion::AS_INT,
        Meridian::UI::LibVersion::PROJECT_NAME)));

// SE (1.5.x) plugin declaration
extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
{
    a_info->infoVersion = SKSE::PluginInfo::kVersion;
    a_info->name = Meridian::UI::LibVersion::PROJECT_NAME;
    a_info->version = Meridian::UI::LibVersion::AS_INT;

    if (a_skse->IsEditor() || a_skse->RuntimeVersion() < SKSE::RUNTIME_SSE_1_5_39)
    {
        return false;
    }

    return true;
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
