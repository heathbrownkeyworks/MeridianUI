#include "PCH.h"

#include "MeridianUIAPI/API.h"
#include "MeridianUIAPI/DllLoader.h"
#include "MeridianUIAPI/NifViewDllLoader.h"
#include "MeridianUIAPI/RenderLayerDllLoader.h"
#include "MeridianUIAPI/SKSELoader.h"
#include "TestCases/TestCases.hpp"

#include <fstream>


[[nodiscard]] bool InitLog()
{
    Meridian::Log::InitOptions options{};
    options.name = "global log"s;
    options.logFileStem = fmt::format("{}.log", PLUGIN_NAME);
    options.pattern = "[%T.%e] [%^%l%$] : %v"s;
    return Meridian::Log::Init(options) != nullptr;
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
        LOG_INFO("fixture: use_synccopy.txt present — requesting SyncCopy renderer");
    }
    else
    {
        LOG_INFO("fixture: no marker — requesting RingBuffer renderer");
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
        LOG_ERROR("render-layer fixture: Meridian.RenderLayer/1 query failed");
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
        LOG_ERROR("render-layer fixture: checkerboard creation failed");
        return;
    }

    LOG_INFO("render-layer fixture: visible checkerboard handle {}", s_checkerboard);

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
        LOG_ERROR("NIF fixture: Data\\MeridianUI\\tests\\nif_path.txt is missing or empty");
        return;
    }
    modelPath.erase(0, first);

    static Meridian::UI::NifView::INifViewAPI* s_nifView = nullptr;
    s_nifView = Meridian::UI::NifView::Query(a_settings, PLUGIN_NAME);
    if (s_nifView == nullptr)
    {
        LOG_ERROR("NIF fixture: Meridian.NifView/1 query failed");
        return;
    }

    Meridian::UI::NifView::NifLoadInfo loadInfo{};
    loadInfo.surface = s_checkerboard;
    loadInfo.modelPath = modelPath.c_str();
    loadInfo.frameOnLoad = true;
    if (!s_nifView->LoadModel(&loadInfo))
    {
        LOG_ERROR("NIF fixture: LoadModel rejected '{}'", modelPath);
        return;
    }
    LOG_INFO("NIF fixture: queued '{}' on surface {}", modelPath, s_checkerboard);
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
        LOG_INFO("Received message({}) from \"{}\"", a_msg->type, a_msg->sender ? a_msg->sender : "nullptr");
        switch (a_msg->type)
        {
        case Meridian::UI::APIMessageType::ResponseVersion: {
            const auto versionInfo = reinterpret_cast<Meridian::UI::ResponseVersionMessage*>(a_msg->data);
            LOG_INFO("MeridianUI version: {}.{}", Meridian::UI::LibVersion::GetMajorVersion(versionInfo->libVersion), Meridian::UI::LibVersion::GetMinorVersion(versionInfo->libVersion));

            const auto majorAPIVersion = Meridian::UI::APIVersion::GetMajorVersion(versionInfo->apiVersion);
            // If the major version is different from ours, then using the API may cause problems
            if (majorAPIVersion != Meridian::UI::APIVersion::MAJOR)
            {
                s_canUseAPI = false;
                LOG_ERROR("Can't using this API version of MeridianUI. We have {}.{} and installed is {}.{}",
                              Meridian::UI::APIVersion::MAJOR,
                              Meridian::UI::APIVersion::MINOR,
                              Meridian::UI::APIVersion::GetMajorVersion(versionInfo->apiVersion),
                              Meridian::UI::APIVersion::GetMinorVersion(versionInfo->apiVersion));
            }
            else
            {
                s_canUseAPI = true;
                LOG_INFO("API version is ok. We have {}.{} and installed is {}.{}",
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
                LOG_ERROR("API is nullptr");
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
                    LOG_ERROR("Failed to load MeridianUI API :(");
                }
            }
            catch (const std::exception& err)
            {
                LOG_ERROR("Failed to load MeridianUI API, {}", err.what());
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
    SKSE::Init(a_skse, false);
    SKSE::AllocTrampoline(1024);
    if (!InitLog())
    {
        return false;
    }

    LOG_INFO("{} {} Plugin Loaded", Meridian::UI::LibVersion::PROJECT_NAME,Meridian::UI::LibVersion::AS_STRING);

    if (!FixtureEnabled())
    {
        LOG_INFO("fixture dormant — no enable marker (Data\\MeridianUI\\tests\\enable_fixture.txt)");
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
