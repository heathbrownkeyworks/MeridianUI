#include "PCH.h"
#include "Hooks/ShutdownHook.hpp"
#include "Hooks/PresentHook.h"
#include "Menus/CursorMenuHooks.h"
#include "Controllers/PublicAPIController.h"
#include "Controllers/NifViewAPIController.h"
#include "Controllers/NifSceneAPIController.h"
#include "Controllers/RenderLayerAPIController.h"
#include "Controllers/ViewAPIController.h"
#include "Config/IniConfig.h"
#include "RuntimeCompatibility.h"

inline void ShowMessageBox(const char* a_msg)
{
    MessageBoxA(0, a_msg, "ERROR", MB_ICONERROR);
}

void InitDefaultLog()
{
#ifdef _DEBUG
    const auto level = spdlog::level::trace;
    auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
    const auto level = spdlog::level::info;
    auto path = logger::log_directory();
    if (!path)
    {
        SKSE::stl::report_and_fail("Failed to find standard logging directory"sv);
    }

    *path /= fmt::format("{}.log"sv, Meridian::UI::LibVersion::PROJECT_NAME);
    auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif

    auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));
    log->set_level(level);
    log->flush_on(level);

    spdlog::set_default_logger(std::move(log));
    spdlog::set_pattern("[%T.%e] [%^%l%$] : %v"s);
}

void InitCefSubprocessLog()
{
#ifdef _DEBUG
    const auto level = spdlog::level::trace;
    auto sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
#else
    const auto level = spdlog::level::info;
    auto path = logger::log_directory();
    if (!path)
    {
        SKSE::stl::report_and_fail("Failed to find standard logging directory"sv);
    }

    *path /= fmt::format("{}.log"sv, NL_UI_SUBPROC_NAME);
    auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif

    auto log = std::make_shared<spdlog::logger>(NL_UI_SUBPROC_NAME, std::move(sink));
    log->set_level(level);
    log->flush_on(level);
    log->set_pattern("[%T.%e] [%^%l%$] : %v"s);

    spdlog::register_logger(std::move(log));
}

extern "C"
{
    DLLEXPORT constinit auto SKSEPlugin_Version =
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

    DLLEXPORT bool SKSEAPI Entry(const SKSE::LoadInterface* a_skse)
    {
        if (a_skse->IsEditor())
        {
            return false;
        }

        try
        {
            // SKSE (a_log = false, we set up our own loggers below)
            SKSE::Init(a_skse, false);
            SKSE::AllocTrampoline(1024);
            InitDefaultLog();

            const auto& ini = Meridian::Config::LoadIniOverrides();
            if (ini.logLevel)
            {
                const auto logLevel = static_cast<spdlog::level::level_enum>(*ini.logLevel);
                spdlog::default_logger()->set_level(logLevel);
                spdlog::default_logger()->flush_on(logLevel);
            }

            InitCefSubprocessLog();

            // Hooks
            Meridian::Hooks::WinProcHook::Install();
            Meridian::Hooks::PresentHook::Install();  // failures are logged and gated in UIPlatformService::Init
            Meridian::Menus::CursorMenuEx::Install();
            Meridian::Hooks::ShutdownHook::Install();

            // API controller
            Meridian::Controllers::PublicAPIController::GetSingleton().Init();
        }
        catch (const std::exception& e)
        {
            ShowMessageBox(e.what());
            return false;
        }

        return true;
    }

    DLLEXPORT Meridian::UI::ResponseVersionMessage GetUIPlatformAPIVersion()
    {
        return *Meridian::Controllers::PublicAPIController::GetSingleton().GetVersionMessage();
    }

    DLLEXPORT bool CreateOrGetUIPlatformAPI(Meridian::UI::IUIPlatformAPI** a_outApi, Meridian::UI::Settings* a_settings)
    {
        auto& controller = Meridian::Controllers::PublicAPIController::GetSingleton();
        if (!controller.InitIfNotPlatformService(a_settings))
        {
            return false;
        }

        if (a_outApi == nullptr)
        {
            return false;
        }

        *a_outApi = controller.GetAPIMessage()->API;
        return true;
    }

    DLLEXPORT bool CreateOrGetUIPlatformAPIWithVersionCheck(Meridian::UI::IUIPlatformAPI** a_outApi,
                                                            Meridian::UI::Settings* a_settings,
                                                            std::uint32_t a_requestApiVersion,
                                                            const char* a_requestLibName)
    {
        const auto thisLibVer = GetUIPlatformAPIVersion();
        spdlog::info("MeridianUI version: {}.{}", Meridian::UI::LibVersion::GetMajorVersion(thisLibVer.libVersion), Meridian::UI::LibVersion::GetMinorVersion(thisLibVer.libVersion));

        if (!Meridian::UI::APIVersion::IsCompatible(a_requestApiVersion))
        {
            spdlog::error("Can't return API for \"{}\", our ver is {}.{} and their ver is {}.{}",
                          a_requestLibName == nullptr ? "null" : a_requestLibName,
                          Meridian::UI::APIVersion::MAJOR,
                          Meridian::UI::APIVersion::MINOR,
                          Meridian::UI::APIVersion::GetMajorVersion(a_requestApiVersion),
                          Meridian::UI::APIVersion::GetMinorVersion(a_requestApiVersion));
            return false;
        }

        spdlog::info("API requested by \"{}\", our ver is {}.{} and their ver is {}.{}",
                     a_requestLibName == nullptr ? "null" : a_requestLibName,
                     Meridian::UI::APIVersion::MAJOR,
                     Meridian::UI::APIVersion::MINOR,
                     Meridian::UI::APIVersion::GetMajorVersion(a_requestApiVersion),
                     Meridian::UI::APIVersion::GetMinorVersion(a_requestApiVersion));
        return ::CreateOrGetUIPlatformAPI(a_outApi, a_settings);
    }

    DLLEXPORT bool __cdecl QueryMeridianExtension(const char* a_name,
                                                  std::uint32_t a_version,
                                                  void** a_outInterface,
                                                  Meridian::UI::Settings* a_settings,
                                                  const char* a_consumerName)
    {
        if (a_outInterface == nullptr)
        {
            return false;
        }
        *a_outInterface = nullptr;

        const bool isViewRequest = Meridian::UI::View::IsSupported(a_name, a_version);
        const bool isRenderLayerRequest = Meridian::UI::RenderLayer::IsSupported(a_name, a_version);
        const bool isNifViewRequest = Meridian::UI::NifView::IsSupported(a_name, a_version);
        const bool isNifSceneRequest = Meridian::UI::NifScene::IsSupported(a_name, a_version);
        if (!isViewRequest && !isRenderLayerRequest && !isNifViewRequest &&
            !isNifSceneRequest)
        {
            spdlog::warn("Unsupported Meridian extension request '{}' version {} from '{}'",
                         a_name == nullptr ? "null" : a_name,
                         a_version,
                         a_consumerName == nullptr ? "unknown" : a_consumerName);
            return false;
        }

        auto& publicController = Meridian::Controllers::PublicAPIController::GetSingleton();
        if (!publicController.InitIfNotPlatformService(a_settings))
        {
            return false;
        }

        if (isViewRequest)
        {
            auto& viewController = Meridian::Controllers::ViewAPIController::GetSingleton();
            if (viewController.IsShuttingDown())
            {
                return false;
            }

            *a_outInterface = static_cast<Meridian::UI::View::IViewAPI*>(&viewController);
            spdlog::info("Meridian.View/1 requested by '{}'",
                         a_consumerName == nullptr ? "unknown" : a_consumerName);
            return true;
        }

        if (isNifViewRequest)
        {
            auto& nifViewController = Meridian::Controllers::NifViewAPIController::GetSingleton();
            if (nifViewController.IsShuttingDown())
            {
                return false;
            }

            *a_outInterface = static_cast<Meridian::UI::NifView::INifViewAPI*>(&nifViewController);
            spdlog::info("Meridian.NifView/1 requested by '{}'",
                         a_consumerName == nullptr ? "unknown" : a_consumerName);
            return true;
        }

        if (isNifSceneRequest)
        {
            auto& nifSceneController = Meridian::Controllers::NifSceneAPIController::GetSingleton();
            if (nifSceneController.IsShuttingDown())
            {
                return false;
            }

            if (a_version == Meridian::UI::NifScene::ACTOR_APPEARANCE_INTERFACE_VERSION)
            {
                *a_outInterface = static_cast<Meridian::UI::NifScene::INifSceneAPI4*>(
                    &nifSceneController);
                spdlog::info("Meridian.NifScene/4 requested by '{}'",
                             a_consumerName == nullptr ? "unknown" : a_consumerName);
            }
            else if (a_version == Meridian::UI::NifScene::ARMOR_INTERFACE_VERSION)
            {
                *a_outInterface = static_cast<Meridian::UI::NifScene::INifSceneAPI3*>(
                    &nifSceneController);
                spdlog::info("Meridian.NifScene/3 requested by '{}'",
                             a_consumerName == nullptr ? "unknown" : a_consumerName);
            }
            else if (a_version == Meridian::UI::NifScene::WEIGHTED_INTERFACE_VERSION)
            {
                *a_outInterface = static_cast<Meridian::UI::NifScene::INifSceneAPI2*>(
                    &nifSceneController);
                spdlog::info("Meridian.NifScene/2 requested by '{}'",
                             a_consumerName == nullptr ? "unknown" : a_consumerName);
            }
            else
            {
                *a_outInterface = static_cast<Meridian::UI::NifScene::INifSceneAPI*>(
                    &nifSceneController);
                spdlog::info("Meridian.NifScene/1 requested by '{}'",
                             a_consumerName == nullptr ? "unknown" : a_consumerName);
            }
            return true;
        }

        auto& renderLayerController = Meridian::Controllers::RenderLayerAPIController::GetSingleton();
        if (renderLayerController.IsShuttingDown())
        {
            return false;
        }

        *a_outInterface = static_cast<Meridian::UI::RenderLayer::IRenderLayerAPI*>(&renderLayerController);
        spdlog::info("Meridian.RenderLayer/1 requested by '{}'",
                     a_consumerName == nullptr ? "unknown" : a_consumerName);
        return true;
    }
}
