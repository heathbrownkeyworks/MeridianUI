#include "PCH.h"

#include "UIPlatform/RuntimeCompatibility.h"

#include "MeridianUIAPI/API.h"
#include "MeridianUIAPI/DllLoader.h"
#include "MeridianUIAPI/NifViewDllLoader.h"
#include "MeridianUIAPI/RenderLayerDllLoader.h"
#include "MeridianUIAPI/SKSELoader.h"
#include "TestCases/TestCases.hpp"

#include <fstream>

// AE (1.6+) plugin declaration. Version-independent via Address Library.
extern "C" DLLEXPORT constinit auto SKSEPlugin_Version =
    Meridian::RuntimeCompatibility::MakePluginVersionData(1, PLUGIN_NAME);

static_assert(Meridian::RuntimeCompatibility::HasAddressLibraryV5(
    Meridian::RuntimeCompatibility::MakePluginVersionData(1, PLUGIN_NAME)));
static_assert(Meridian::RuntimeCompatibility::HasUpdatedStructs(
    Meridian::RuntimeCompatibility::MakePluginVersionData(1, PLUGIN_NAME)));

// SE (1.5.x) plugin declaration
extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface* a_skse, SKSE::PluginInfo* a_info)
{
    a_info->infoVersion = SKSE::PluginInfo::kVersion;
    a_info->name = PLUGIN_NAME;
    a_info->version = 1;

    if (a_skse->IsEditor() || a_skse->RuntimeVersion() < SKSE::RUNTIME_SSE_1_5_39)
    {
        return false;
    }

    return true;
}

void InitLog()
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

    *path /= fmt::format("{}.log"sv, PLUGIN_NAME);
    auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
#endif

    auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));
    log->set_level(level);
    log->flush_on(level);

    spdlog::set_default_logger(std::move(log));
    spdlog::set_pattern("[%T.%e] [%^%l%$] : %v"s);
}

// The marker selects the legacy renderer so one fixture build can compare
// SyncCopy with RingBuffer. This is the single settings construction path for
// all loader methods, and both outcomes are logged.
static Meridian::UI::Settings BuildFixtureSettings()
{
    Meridian::UI::Settings settings{};
    if (std::filesystem::exists(Meridian::Paths::MeridianRoot(Meridian::Paths::GameRoot()) / "tests" / "use_synccopy.txt"))
    {
        settings.rendererType = Meridian::UI::RendererType::SyncCopy;
        spdlog::info("fixture: use_synccopy.txt present — requesting SyncCopy renderer");
    }
    else
    {
        spdlog::info("fixture: no marker — requesting RingBuffer renderer");
    }
    return settings;
}

// The fixture remains dormant unless this marker exists, so a stock install
// never opens test browsers. Only the marker's presence matters, and release
// staging never creates it.
static bool FixtureEnabled()
{
    return std::filesystem::exists(Meridian::Paths::MeridianRoot(Meridian::Paths::GameRoot()) / "tests" / "enable_fixture.txt");
}

static void StartRenderLayerFixture(Meridian::UI::Settings* a_settings)
{
    const auto marker = Meridian::Paths::MeridianRoot(Meridian::Paths::GameRoot()) /
                        "tests" / "enable_render_layer.txt";
    if (!std::filesystem::exists(marker))
    {
        return;
    }

    static Meridian::UI::RenderLayer::IRenderLayerAPI* s_renderLayers = nullptr;
    static Meridian::UI::RenderLayer::SurfaceHandle s_checkerboard =
        Meridian::UI::RenderLayer::INVALID_SURFACE_HANDLE;
    if (s_checkerboard != Meridian::UI::RenderLayer::INVALID_SURFACE_HANDLE)
    {
        return;
    }

    s_renderLayers = Meridian::UI::RenderLayer::Query(a_settings, PLUGIN_NAME);
    if (s_renderLayers == nullptr)
    {
        spdlog::error("render-layer fixture: Meridian.RenderLayer/1 query failed");
        return;
    }

    Meridian::UI::RenderLayer::SurfaceCreateInfo info{};
    info.ownerName = PLUGIN_NAME;
    info.surfaceName = "checkerboard";
    info.x = 200;
    info.y = 200;
    info.width = 640;
    info.height = 480;
    info.zOrder = 1000;
    info.initiallyVisible = true;
    s_checkerboard = s_renderLayers->CreateSurface(&info);
    if (s_checkerboard == Meridian::UI::RenderLayer::INVALID_SURFACE_HANDLE)
    {
        spdlog::error("render-layer fixture: checkerboard creation failed");
        return;
    }

    spdlog::info("render-layer fixture: visible checkerboard handle {}", s_checkerboard);

    const auto testsRoot = Meridian::Paths::MeridianRoot(Meridian::Paths::GameRoot()) / "tests";
    if (!std::filesystem::exists(testsRoot / "enable_nif_view.txt"))
    {
        return;
    }

    std::ifstream pathFile(testsRoot / "nif_path.txt");
    std::string modelPath;
    std::getline(pathFile, modelPath);
    while (!modelPath.empty() && (modelPath.back() == '\r' || modelPath.back() == ' ' || modelPath.back() == '\t'))
    {
        modelPath.pop_back();
    }
    const auto first = modelPath.find_first_not_of(" \t");
    if (first == std::string::npos)
    {
        spdlog::error("NIF fixture: Data\\MeridianUI\\tests\\nif_path.txt is missing or empty");
        return;
    }
    modelPath.erase(0, first);

    static Meridian::UI::NifView::INifViewAPI* s_nifView = nullptr;
    s_nifView = Meridian::UI::NifView::Query(a_settings, PLUGIN_NAME);
    if (s_nifView == nullptr)
    {
        spdlog::error("NIF fixture: Meridian.NifView/1 query failed");
        return;
    }

    Meridian::UI::NifView::NifLoadInfo loadInfo{};
    loadInfo.surface = s_checkerboard;
    loadInfo.modelPath = modelPath.c_str();
    loadInfo.frameOnLoad = true;
    if (!s_nifView->LoadModel(&loadInfo))
    {
        spdlog::error("NIF fixture: LoadModel rejected '{}'", modelPath);
        return;
    }
    spdlog::info("NIF fixture: queued '{}' on surface {}", modelPath, s_checkerboard);
}

void Init1stMethodToGetAPI()
{
    static bool s_canUseAPI = false;
    SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* a_msg) {
        switch (a_msg->type)
        {
        case SKSE::MessagingInterface::kPostPostLoad:
            // All plugins are loaded. Request lib version.
            SKSE::GetMessagingInterface()->Dispatch(Meridian::UI::APIMessageType::RequestVersion, nullptr, 0, Meridian::UI::LibVersion::PROJECT_NAME);
            break;
        case SKSE::MessagingInterface::kInputLoaded:
            if (s_canUseAPI)
            {
                Meridian::UI::Settings defaultSettings = BuildFixtureSettings();
                // API version is ok. Request interface.
                SKSE::GetMessagingInterface()->Dispatch(Meridian::UI::APIMessageType::RequestAPI, &defaultSettings, sizeof(defaultSettings), Meridian::UI::LibVersion::PROJECT_NAME);
            }
            break;
        default:
            break;
        }
    });
    SKSE::GetMessagingInterface()->RegisterListener(Meridian::UI::LibVersion::PROJECT_NAME, [](SKSE::MessagingInterface::Message* a_msg) {
        spdlog::info("Received message({}) from \"{}\"", a_msg->type, a_msg->sender ? a_msg->sender : "nullptr");
        switch (a_msg->type)
        {
        case Meridian::UI::APIMessageType::ResponseVersion: {
            const auto versionInfo = reinterpret_cast<Meridian::UI::ResponseVersionMessage*>(a_msg->data);
            spdlog::info("MeridianUI version: {}.{}", Meridian::UI::LibVersion::GetMajorVersion(versionInfo->libVersion), Meridian::UI::LibVersion::GetMinorVersion(versionInfo->libVersion));

            const auto majorAPIVersion = Meridian::UI::APIVersion::GetMajorVersion(versionInfo->apiVersion);
            // If the major version is different from ours, then using the API may cause problems
            if (majorAPIVersion != Meridian::UI::APIVersion::MAJOR)
            {
                s_canUseAPI = false;
                spdlog::error("Can't using this API version of MeridianUI. We have {}.{} and installed is {}.{}",
                              Meridian::UI::APIVersion::MAJOR,
                              Meridian::UI::APIVersion::MINOR,
                              Meridian::UI::APIVersion::GetMajorVersion(versionInfo->apiVersion),
                              Meridian::UI::APIVersion::GetMinorVersion(versionInfo->apiVersion));
            }
            else
            {
                s_canUseAPI = true;
                spdlog::info("API version is ok. We have {}.{} and installed is {}.{}",
                             Meridian::UI::APIVersion::MAJOR,
                             Meridian::UI::APIVersion::MINOR,
                             Meridian::UI::APIVersion::GetMajorVersion(versionInfo->apiVersion),
                             Meridian::UI::APIVersion::GetMinorVersion(versionInfo->apiVersion));
            }
            break;
        }
        case Meridian::UI::APIMessageType::ResponseAPI: {
            auto api = reinterpret_cast<Meridian::UI::ResponseAPIMessage*>(a_msg->data)->API;
            if (api == nullptr)
            {
                spdlog::error("API is nullptr");
                break;
            }
            Meridian::UI::TestCase::StartTests(api);
            break;
        }
        default:
            break;
        }
    });
}

void Init2ndMethodToGetAPI()
{
    SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* a_msg) {
        switch (a_msg->type)
        {
        case SKSE::MessagingInterface::kInputLoaded:
            // All plugins are loaded
            try
            {
                Meridian::UI::IUIPlatformAPI* api = nullptr;
                Meridian::UI::Settings defaultSettings = BuildFixtureSettings();

                if (Meridian::UI::DllLoader::CreateOrGetUIPlatformAPIWithVersionCheck(&api, &defaultSettings, Meridian::UI::APIVersion::AS_INT, PLUGIN_NAME))
                {
                    Meridian::UI::TestCase::StartTests(api);
                }
                else
                {
                    spdlog::error("Failed to load MeridianUI API :(");
                }
            }
            catch (const std::exception& err)
            {
                spdlog::error("Failed to load MeridianUI API, {}", err.what());
            }
            break;
        default:
            break;
        }
    });
}

void Init3rdMethodToGetAPI()
{
    SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* a_msg) {
        // Active loader path (see SKSEPluginLoad below). SKSELoader::ProcessSKSEMessage
        // only reads `settings` for the kInputLoaded/RequestAPI dispatch, but a static
        // keeps the pointer valid for that call regardless of message type.
        static Meridian::UI::Settings s_fixtureSettings{};
        if (a_msg->type == SKSE::MessagingInterface::kInputLoaded)
        {
            s_fixtureSettings = BuildFixtureSettings();
        }
        Meridian::UI::SKSELoader::ProcessSKSEMessage(a_msg, &s_fixtureSettings);
        if (a_msg->type == SKSE::MessagingInterface::kInputLoaded)
        {
            StartRenderLayerFixture(&s_fixtureSettings);
        }
    });
    Meridian::UI::SKSELoader::GetUIPlatformAPIWithVersionCheck([](Meridian::UI::IUIPlatformAPI* a_api) {
        Meridian::UI::TestCase::StartTests(a_api);
    });
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
    if (a_skse->IsEditor())
    {
        return false;
    }

    // SKSE
    SKSE::Init(a_skse);
    SKSE::AllocTrampoline(1024);
    InitLog();
    if (!FixtureEnabled())
    {
        spdlog::info("fixture dormant — no enable marker (Data\\MeridianUI\\tests\\enable_fixture.txt)");
        return true;
    }
    // First method may not work correctly with some plugins
    // Init1stMethodToGetAPI();
    // Init2ndMethodToGetAPI();
    Init3rdMethodToGetAPI();

    const auto iniCollection = RE::INISettingCollection::GetSingleton();
    // [General]
    // Don't stop game when window is collapsed
    iniCollection->GetSetting("bAlwaysActive:General")->data.b = true;

    return true;
}
