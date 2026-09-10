#include "PCH.h"
#include "Hooks/InputDispatchHook.h"
#include "Hooks/ShutdownHook.hpp"
#include "Hooks/PresentHook.h"
#include "Menus/CursorMenuHooks.h"
#include "Controllers/PublicAPIController.h"
#include "Controllers/NifViewAPIController.h"
#include "Controllers/NifSceneAPIController.h"
#include "Controllers/RenderLayerAPIController.h"
#include "Controllers/ViewAPIController.h"
#include "Controllers/InputAPIController.h"
#include "Config/IniConfig.h"

inline void ShowMessageBox(const char* a_msg)
{
    MessageBoxA(0, a_msg, "ERROR", MB_ICONERROR);
}

[[nodiscard]] bool InitDefaultLog()
{
    Meridian::Log::InitOptions options{};
    options.name = "global log"s;
    options.logFileStem = fmt::format("{}.log", Meridian::UI::LibVersion::PROJECT_NAME);
    options.makeDefault = true;
    return Meridian::Log::Init(options) != nullptr;
}

[[nodiscard]] bool InitCefSubprocessLog()
{
    Meridian::Log::InitOptions options{};
    options.name = NL_UI_SUBPROC_NAME;
    options.logFileStem = fmt::format("{}.log", NL_UI_SUBPROC_NAME);
    options.makeDefault = false;
    return Meridian::Log::Init(options) != nullptr;
}

extern "C"
{
    DLLEXPORT bool Entry(const SKSE::LoadInterface* a_skse)
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
            if (!InitDefaultLog())
            {
                ShowMessageBox("Failed to initialize MeridianUI logging");
                return false;
            }

            const auto& ini = Meridian::Config::LoadIniOverrides();
            if (ini.logLevel)
            {
                Meridian::Log::SetLevel(*ini.logLevel);
            }

            if (!InitCefSubprocessLog())
            {
                LOG_WARN("Failed to initialize the CEF subprocess log relay; subprocess log lines will be dropped");
            }

            LOG_INFO("{} {} Plugin Loaded", Meridian::UI::LibVersion::PROJECT_NAME,Meridian::UI::LibVersion::AS_STRING);
            
            // Hooks
            Meridian::Hooks::WinProcHook::Install();
            Meridian::Hooks::InputDispatchHook::Install();  // early layer preserves the original sink contract
            Meridian::Hooks::InputDispatchHook::RegisterOutermostInstall();  // final layer runs ahead of competing plugin hooks
            Meridian::Hooks::PresentHook::Install(
                ini.compositorTiming.value_or(Meridian::Config::CompositorTiming::AfterRendererEnd));  // failures are logged and gated in UIPlatformService::Init
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
        LOG_INFO("MeridianUI version: {}.{}", Meridian::UI::LibVersion::GetMajorVersion(thisLibVer.libVersion), Meridian::UI::LibVersion::GetMinorVersion(thisLibVer.libVersion));

        if (!Meridian::UI::APIVersion::IsCompatible(a_requestApiVersion))
        {
            LOG_ERROR("Can't return API for \"{}\", our ver is {}.{} and their ver is {}.{}",
                          a_requestLibName == nullptr ? "null" : a_requestLibName,
                          Meridian::UI::APIVersion::MAJOR,
                          Meridian::UI::APIVersion::MINOR,
                          Meridian::UI::APIVersion::GetMajorVersion(a_requestApiVersion),
                          Meridian::UI::APIVersion::GetMinorVersion(a_requestApiVersion));
            return false;
        }

        LOG_INFO("API requested by \"{}\", our ver is {}.{} and their ver is {}.{}",
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
        const bool isInputRequest = Meridian::UI::Input::IsSupported(a_name, a_version);
        const bool isRenderLayerRequest = Meridian::UI::RenderLayer::IsSupported(a_name, a_version);
        const bool isNifViewRequest = Meridian::UI::NifView::IsSupported(a_name, a_version);
        const bool isNifSceneRequest = Meridian::UI::NifScene::IsSupported(a_name, a_version);
        if (!isViewRequest && !isRenderLayerRequest && !isNifViewRequest &&
            !isNifSceneRequest && !isInputRequest)
        {
            LOG_WARN("Unsupported Meridian extension request '{}' version {} from '{}'",
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

        if (isInputRequest)
        {
            if (Meridian::Controllers::ViewAPIController::GetSingleton().IsShuttingDown()) return false;
            *a_outInterface = static_cast<Meridian::UI::Input::IInputAPI*>(
                &Meridian::Controllers::InputAPIController::GetSingleton());
            spdlog::info("Meridian.Input/1 requested by '{}'", a_consumerName ? a_consumerName : "unknown");
            return true;
        }
        if (isViewRequest)
        {
            auto& viewController = Meridian::Controllers::ViewAPIController::GetSingleton();
            if (viewController.IsShuttingDown())
            {
                return false;
            }

            *a_outInterface = static_cast<Meridian::UI::View::IViewAPI*>(&viewController);
            LOG_INFO("Meridian.View/1 requested by '{}'",
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
            LOG_INFO("Meridian.NifView/1 requested by '{}'",
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
                LOG_INFO("Meridian.NifScene/4 requested by '{}'",
                             a_consumerName == nullptr ? "unknown" : a_consumerName);
            }
            else if (a_version == Meridian::UI::NifScene::ARMOR_INTERFACE_VERSION)
            {
                *a_outInterface = static_cast<Meridian::UI::NifScene::INifSceneAPI3*>(
                    &nifSceneController);
                LOG_INFO("Meridian.NifScene/3 requested by '{}'",
                             a_consumerName == nullptr ? "unknown" : a_consumerName);
            }
            else if (a_version == Meridian::UI::NifScene::WEIGHTED_INTERFACE_VERSION)
            {
                *a_outInterface = static_cast<Meridian::UI::NifScene::INifSceneAPI2*>(
                    &nifSceneController);
                LOG_INFO("Meridian.NifScene/2 requested by '{}'",
                             a_consumerName == nullptr ? "unknown" : a_consumerName);
            }
            else
            {
                *a_outInterface = static_cast<Meridian::UI::NifScene::INifSceneAPI*>(
                    &nifSceneController);
                LOG_INFO("Meridian.NifScene/1 requested by '{}'",
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
        LOG_INFO("Meridian.RenderLayer/1 requested by '{}'",
                     a_consumerName == nullptr ? "unknown" : a_consumerName);
        return true;
    }
}
