#include "PCH.h"

#include "ControlPayload.h"
#include "EquippedArmorManifest.h"
#include "NifTestHotkey.h"
#include "MeridianUIAPI/NifSceneDllLoader.h"
#include "MeridianUIAPI/NifViewDllLoader.h"
#include "MeridianUIAPI/RenderLayerDllLoader.h"
#include "MeridianUIAPI/ViewDllLoader.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace
{
    constexpr Meridian::UI::NifScene::ObjectHandle CUIRASS_OBJECT = 101;
    constexpr Meridian::UI::NifScene::ObjectHandle HELMET_OBJECT = 102;
    constexpr Meridian::UI::NifScene::ObjectHandle GAUNTLETS_OBJECT = 103;
    constexpr Meridian::UI::NifScene::ObjectHandle BOOTS_OBJECT = 104;
    constexpr RE::FormID NORD_RACE_FORM_ID = 0x00013746;
    constexpr RE::FormID HIDE_CUIRASS_FORM_ID = 0x00013911;
    constexpr RE::FormID HIDE_HELMET_FORM_ID = 0x00013913;
    constexpr RE::FormID HIDE_GAUNTLETS_FORM_ID = 0x00013912;
    constexpr RE::FormID HIDE_BOOTS_FORM_ID = 0x00013910;
    constexpr RE::FormID BD_HIDE_CUIRASS_LOCAL_FORM_ID = 0x000B28;
    constexpr RE::FormID BD_HIDE_GAUNTLETS_LOCAL_FORM_ID = 0x000AF3;
    constexpr RE::FormID BD_HIDE_BOOTS_LOCAL_FORM_ID = 0x000B51;
    constexpr RE::FormID LAYERED_ARMOR_LOCAL_FORM_ID = 0x000800;
    constexpr RE::FormID TEXTURE_SWAP_ARMOR_LOCAL_FORM_ID = 0x000803;
    constexpr char BD_STANDALONE_PLUGIN[] = "BD Standalone.esp";
    constexpr char NIF_TEST_PLUGIN[] = "MeridianNifTest.esp";
    constexpr Meridian::UI::NifScene::ObjectHandle LAYERED_OBJECT = CUIRASS_OBJECT;
    constexpr Meridian::UI::NifScene::ObjectHandle TEXTURE_SWAP_OBJECT = CUIRASS_OBJECT;
    constexpr char VIEW_OWNER[] = "meridianniftest";
    constexpr char VIEW_URL[] = "mod://meridianniftest/index.html";

    enum class OutfitSource
    {
        NifPaths,
        ArmorRecords,
        ModdedArmorRecords,
        LayeredArmorRecord,
        TextureSwapArmorRecord,
        PlayerEquippedArmor,
        PlayerActorAppearance,
    };

    struct TestState
    {
        Meridian::UI::Settings settings{};
        Meridian::UI::RenderLayer::IRenderLayerAPI* renderLayers = nullptr;
        Meridian::UI::NifView::INifViewAPI* nifView = nullptr;
        Meridian::UI::NifScene::INifSceneAPI4* nifScene = nullptr;
        Meridian::UI::View::IViewAPI* views = nullptr;
        Meridian::UI::RenderLayer::SurfaceHandle surface =
            Meridian::UI::RenderLayer::INVALID_SURFACE_HANDLE;
        Meridian::UI::View::ViewHandle view = Meridian::UI::View::INVALID_VIEW_HANDLE;
        Meridian::UI::NifView::CameraState camera{};
        std::array<bool, 4> outfitVisibility{true, true, true, true};
        float outfitWeight = 100.0f;
        OutfitSource outfitSource = OutfitSource::ArmorRecords;
        bool outfitActive = false;
        bool inputLoaded = false;
        bool dataLoaded = false;
        bool initialModelQueued = false;
        bool APIsQueried = false;
        bool testVisible = false;
        bool layoutReady = false;
    };

    TestState& State()
    {
        static TestState state{};
        return state;
    }

    [[nodiscard]] bool InitializeLog()
    {
        Meridian::Log::InitOptions options{};
        options.name = "MeridianNifTest";
        options.logFileStem = "MeridianNifTest.log";
        options.pattern = "[%T.%e] [%^%l%$] : %v"s;
        options.debugLevel = Meridian::Log::LogLevel::Info;
        return Meridian::Log::Init(options) != nullptr;
    }

    bool QueueModel(std::string_view a_modelPath)
    {
        auto& state = State();
        if (!state.dataLoaded || state.nifView == nullptr ||
            state.surface == Meridian::UI::RenderLayer::INVALID_SURFACE_HANDLE)
        {
            return false;
        }

        Meridian::UI::NifView::NifLoadInfo load{};
        load.surface = state.surface;
        const std::string stablePath(a_modelPath);
        load.modelPath = stablePath.c_str();
        load.frameOnLoad = true;
        if (!state.nifView->LoadModel(&load))
        {
            LOG_ERROR("Meridian.NifView/1 rejected '{}'", stablePath);
            return false;
        }

        state.outfitActive = false;
        LOG_INFO("queued test mesh '{}' on surface {}", stablePath, state.surface);
        return true;
    }

    bool QueueNifHideOutfit()
    {
        auto& state = State();
        if (!state.dataLoaded || state.nifScene == nullptr ||
            state.surface == Meridian::UI::RenderLayer::INVALID_SURFACE_HANDLE)
        {
            return false;
        }

        const std::array objects{
            Meridian::UI::NifScene::WeightedSceneObjectLoadInfo{
                .object = CUIRASS_OBJECT,
                .lowModelPath = "armor\\hide\\f\\cuirasslight_0.nif",
                .highModelPath = "armor\\hide\\f\\cuirasslight_1.nif",
                .visible = state.outfitVisibility[0],
            },
            Meridian::UI::NifScene::WeightedSceneObjectLoadInfo{
                .object = HELMET_OBJECT,
                .lowModelPath = "armor\\hide\\f\\helmetlight.nif",
                .visible = state.outfitVisibility[1],
            },
            Meridian::UI::NifScene::WeightedSceneObjectLoadInfo{
                .object = GAUNTLETS_OBJECT,
                .lowModelPath = "armor\\hide\\f\\gauntletslight_0.nif",
                .highModelPath = "armor\\hide\\f\\gauntletslight_1.nif",
                .visible = state.outfitVisibility[2],
            },
            Meridian::UI::NifScene::WeightedSceneObjectLoadInfo{
                .object = BOOTS_OBJECT,
                .lowModelPath = "armor\\hide\\f\\bootslight_0.nif",
                .highModelPath = "armor\\hide\\f\\bootslight_1.nif",
                .visible = state.outfitVisibility[3],
            },
        };
        Meridian::UI::NifScene::WeightedSceneLoadInfo load{};
        load.surface = state.surface;
        load.objects = objects.data();
        load.objectCount = static_cast<std::uint32_t>(objects.size());
        load.weight = state.outfitWeight;
        load.frameOnLoad = !state.outfitActive;
        if (!state.nifScene->ReplaceWeightedScene(&load))
        {
            LOG_ERROR("Meridian.NifScene/2 rejected the weighted hide outfit");
            return false;
        }
        state.outfitActive = true;
        LOG_INFO("queued four-piece NIF-path hide outfit at weight {:.1f} on surface {}",
                     state.outfitWeight,
                     state.surface);
        return true;
    }

    bool QueueResolvedArmorRecordOutfit(
        RE::TESRace* a_race,
        const std::array<RE::TESObjectARMO*, 4>& a_armors,
        std::string_view a_fixtureName)
    {
        auto& state = State();
        if (!state.dataLoaded || state.nifScene == nullptr ||
            state.surface == Meridian::UI::RenderLayer::INVALID_SURFACE_HANDLE)
        {
            return false;
        }

        if (a_race == nullptr ||
            std::any_of(a_armors.begin(), a_armors.end(), [](const auto* a_armor) {
                return a_armor == nullptr;
            }))
        {
            LOG_ERROR("could not resolve the {} armor record fixture", a_fixtureName);
            return false;
        }

        LOG_INFO(
            "using deterministic {} record fixture: race {:08X}, armors {:08X}, {:08X}, {:08X}, {:08X}",
            a_fixtureName,
            a_race->GetFormID(),
            a_armors[0]->GetFormID(),
            a_armors[1]->GetFormID(),
            a_armors[2]->GetFormID(),
            a_armors[3]->GetFormID());

        const std::array handles{
            CUIRASS_OBJECT, HELMET_OBJECT, GAUNTLETS_OBJECT, BOOTS_OBJECT};
        std::array<Meridian::UI::NifScene::ArmorSceneObjectLoadInfo, 4> objects{};
        for (std::size_t index = 0; index < objects.size(); ++index)
        {
            objects[index].object = handles[index];
            objects[index].armorFormID = a_armors[index]->GetFormID();
            objects[index].visible = state.outfitVisibility[index];
        }

        Meridian::UI::NifScene::ArmorSceneLoadInfo load{};
        load.surface = state.surface;
        load.objects = objects.data();
        load.objectCount = static_cast<std::uint32_t>(objects.size());
        load.raceFormID = a_race->GetFormID();
        load.sex = Meridian::UI::NifScene::Sex::Female;
        load.weight = state.outfitWeight;
        load.frameOnLoad = !state.outfitActive;
        if (!state.nifScene->ReplaceArmorScene(&load))
        {
            LOG_ERROR("Meridian.NifScene/3 rejected the {} armor-record hide outfit",
                          a_fixtureName);
            return false;
        }
        state.outfitActive = true;
        LOG_INFO("queued four-piece {} armor-record hide outfit at weight {:.1f} on surface {}",
                     a_fixtureName,
                     state.outfitWeight,
                     state.surface);
        return true;
    }

    bool QueueArmorRecordOutfit()
    {
        auto* race = RE::TESForm::LookupByID<RE::TESRace>(NORD_RACE_FORM_ID);
        const std::array armors{
            RE::TESForm::LookupByID<RE::TESObjectARMO>(HIDE_CUIRASS_FORM_ID),
            RE::TESForm::LookupByID<RE::TESObjectARMO>(HIDE_HELMET_FORM_ID),
            RE::TESForm::LookupByID<RE::TESObjectARMO>(HIDE_GAUNTLETS_FORM_ID),
            RE::TESForm::LookupByID<RE::TESObjectARMO>(HIDE_BOOTS_FORM_ID),
        };
        return QueueResolvedArmorRecordOutfit(race, armors, "Skyrim.esm");
    }

    bool QueueModdedArmorRecordOutfit()
    {
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        auto* race = RE::TESForm::LookupByID<RE::TESRace>(NORD_RACE_FORM_ID);
        if (dataHandler == nullptr)
        {
            LOG_ERROR("could not access TESDataHandler for the BD Standalone fixture");
            return false;
        }
        const std::array armors{
            dataHandler->LookupForm<RE::TESObjectARMO>(
                BD_HIDE_CUIRASS_LOCAL_FORM_ID, BD_STANDALONE_PLUGIN),
            RE::TESForm::LookupByID<RE::TESObjectARMO>(HIDE_HELMET_FORM_ID),
            dataHandler->LookupForm<RE::TESObjectARMO>(
                BD_HIDE_GAUNTLETS_LOCAL_FORM_ID, BD_STANDALONE_PLUGIN),
            dataHandler->LookupForm<RE::TESObjectARMO>(
                BD_HIDE_BOOTS_LOCAL_FORM_ID, BD_STANDALONE_PLUGIN),
        };
        return QueueResolvedArmorRecordOutfit(race, armors, BD_STANDALONE_PLUGIN);
    }

    bool QueueSingleFixtureArmorRecord(RE::FormID a_localFormID,
                                       Meridian::UI::NifScene::ObjectHandle a_object,
                                       std::string_view a_fixtureName)
    {
        auto& state = State();
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        auto* race = RE::TESForm::LookupByID<RE::TESRace>(NORD_RACE_FORM_ID);
        auto* armor = dataHandler == nullptr ? nullptr :
            dataHandler->LookupForm<RE::TESObjectARMO>(
                a_localFormID, NIF_TEST_PLUGIN);
        if (!state.dataLoaded || state.nifScene == nullptr || dataHandler == nullptr ||
            race == nullptr || armor == nullptr ||
            state.surface == Meridian::UI::RenderLayer::INVALID_SURFACE_HANDLE)
        {
            LOG_ERROR("could not resolve the {} {} fixture",
                          NIF_TEST_PLUGIN,
                          a_fixtureName);
            return false;
        }

        Meridian::UI::NifScene::ArmorSceneObjectLoadInfo object{};
        object.object = a_object;
        object.armorFormID = armor->GetFormID();
        object.visible = state.outfitVisibility[0];

        Meridian::UI::NifScene::ArmorSceneLoadInfo load{};
        load.surface = state.surface;
        load.objects = std::addressof(object);
        load.objectCount = 1;
        load.raceFormID = race->GetFormID();
        load.sex = Meridian::UI::NifScene::Sex::Female;
        load.weight = state.outfitWeight;
        load.frameOnLoad = !state.outfitActive;
        if (!state.nifScene->ReplaceArmorScene(&load))
        {
            LOG_ERROR("Meridian.NifScene/3 rejected the {} fixture", a_fixtureName);
            return false;
        }
        state.outfitActive = true;
        LOG_INFO(
            "queued one public object from {} armor {:08X} at weight {:.1f} on surface {}",
            a_fixtureName,
            armor->GetFormID(),
            state.outfitWeight,
            state.surface);
        return true;
    }

    bool QueueLayeredArmorRecord()
    {
        return QueueSingleFixtureArmorRecord(
            LAYERED_ARMOR_LOCAL_FORM_ID, LAYERED_OBJECT, "layered ARMA");
    }

    bool QueueTextureSwapArmorRecord()
    {
        return QueueSingleFixtureArmorRecord(
            TEXTURE_SWAP_ARMOR_LOCAL_FORM_ID,
            TEXTURE_SWAP_OBJECT,
            "alternate-texture ARMA");
    }

    bool QueuePlayerEquippedOutfit(bool a_useActorWeight)
    {
        auto& state = State();
        const auto* taskInterface = SKSE::GetTaskInterface();
        if (!state.dataLoaded || state.nifScene == nullptr || taskInterface == nullptr ||
            state.surface == Meridian::UI::RenderLayer::INVALID_SURFACE_HANDLE)
        {
            LOG_ERROR("player-equipped outfit could not be queued");
            return false;
        }

        auto* const nifScene = state.nifScene;
        auto* const views = state.views;
        const auto surface = state.surface;
        const auto view = state.view;
        const auto requestedWeight = state.outfitWeight;
        const auto frameOnLoad = !state.outfitActive;
        state.outfitActive = true;
        taskInterface->AddTask([
            nifScene,
            views,
            surface,
            view,
            requestedWeight,
            frameOnLoad,
            useActorWeight = a_useActorWeight]() {
            auto* player = RE::PlayerCharacter::GetSingleton();
            auto* base = player != nullptr ? player->GetActorBase() : nullptr;
            auto* race = player != nullptr ? player->GetRace() : nullptr;
            if (player == nullptr || base == nullptr || race == nullptr)
            {
                LOG_ERROR("could not resolve the live player actor, base, or race");
                return;
            }

            const auto sex = base->GetSex();
            if (sex != RE::SEX::kMale && sex != RE::SEX::kFemale)
            {
                LOG_ERROR("player actor has unsupported sex {}", static_cast<std::uint32_t>(sex));
                return;
            }
            const auto manifest = Meridian::NifTest::BuildEquippedArmorManifest(
                [player](std::uint32_t a_slotMask) {
                    auto* armor = player->GetWornArmor(
                        static_cast<RE::BGSBipedObjectForm::BipedObjectSlot>(a_slotMask));
                    return armor != nullptr ? armor->GetFormID() : 0u;
                },
                Meridian::UI::NifScene::MAX_SCENE_OBJECTS);
            if (manifest.error !=
                    Meridian::NifTest::EquippedArmorManifestError::None ||
                manifest.items.empty())
            {
                LOG_WARN("player-equipped outfit is empty or exceeds the {}-record cap",
                             Meridian::UI::NifScene::MAX_SCENE_OBJECTS);
                return;
            }

            std::vector<Meridian::UI::NifScene::ArmorSceneObjectLoadInfo> objects;
            objects.reserve(manifest.items.size());
            std::ostringstream formIDs;
            formIDs << std::hex << std::uppercase << std::setfill('0');
            for (const auto& item : manifest.items)
            {
                objects.push_back({
                    .object = item.object,
                    .armorFormID = item.armorFormID,
                    .visible = true,
                });
                if (formIDs.tellp() > 0)
                {
                    formIDs << ',';
                }
                formIDs << std::setw(8) << item.armorFormID;
            }

            const auto resolvedWeight = useActorWeight ?
                std::clamp(base->weight, 0.0f, 100.0f) : requestedWeight;
            Meridian::UI::NifScene::ArmorSceneLoadInfo load{};
            load.surface = surface;
            load.objects = objects.data();
            load.objectCount = static_cast<std::uint32_t>(objects.size());
            load.raceFormID = race->GetFormID();
            load.sex = sex == RE::SEX::kFemale ?
                Meridian::UI::NifScene::Sex::Female :
                Meridian::UI::NifScene::Sex::Male;
            load.weight = resolvedWeight;
            load.frameOnLoad = frameOnLoad;
            if (!nifScene->ReplaceArmorScene(&load))
            {
                LOG_ERROR("Meridian.NifScene/3 rejected the player-equipped outfit");
                return;
            }

            LOG_INFO(
                "queued player-equipped actor {:08X}: race {:08X}, sex {}, weight {:.1f}, {} unique armor records [{}]",
                player->GetFormID(),
                race->GetFormID(),
                static_cast<std::uint32_t>(load.sex),
                resolvedWeight,
                objects.size(),
                formIDs.str());
            if (useActorWeight && views != nullptr &&
                view != Meridian::UI::View::INVALID_VIEW_HANDLE)
            {
                const auto script = std::string("window.meridianNifApplyActorState?.(") +
                    std::to_string(resolvedWeight) + ");";
                views->ExecuteJavaScript(view, script.c_str());
            }
        });
        return true;
    }

    bool QueuePlayerActorAppearance()
    {
        auto& state = State();
        if (!state.dataLoaded || state.nifScene == nullptr ||
            state.surface == Meridian::UI::RenderLayer::INVALID_SURFACE_HANDLE)
        {
            LOG_ERROR("player actor appearance could not be queued");
            return false;
        }

        Meridian::UI::NifScene::ActorAppearanceSceneLoadInfo load{};
        load.surface = state.surface;
        load.actorFormID = 0x14;
        load.parts = Meridian::UI::NifScene::ACTOR_APPEARANCE_PARTS_ALL;
        load.frameOnLoad = !state.outfitActive;
        if (!state.nifScene->ReplaceActorAppearanceScene(&load))
        {
            LOG_ERROR("Meridian.NifScene/4 rejected the player actor appearance");
            return false;
        }

        state.outfitActive = true;
        LOG_INFO("queued live player actor appearance {:08X} on surface {}",
                     load.actorFormID,
                     state.surface);
        return true;
    }

    bool QueueActiveOutfit()
    {
        switch (State().outfitSource)
        {
        case OutfitSource::ArmorRecords:
            return QueueArmorRecordOutfit();
        case OutfitSource::ModdedArmorRecords:
            return QueueModdedArmorRecordOutfit();
        case OutfitSource::LayeredArmorRecord:
            return QueueLayeredArmorRecord();
        case OutfitSource::TextureSwapArmorRecord:
            return QueueTextureSwapArmorRecord();
        case OutfitSource::PlayerEquippedArmor:
            return QueuePlayerEquippedOutfit(false);
        case OutfitSource::PlayerActorAppearance:
            return QueuePlayerActorAppearance();
        case OutfitSource::NifPaths:
        default:
            return QueueNifHideOutfit();
        }
    }

    void TryQueueInitialModel()
    {
        auto& state = State();
        if (state.initialModelQueued)
        {
            return;
        }
        if (QueueActiveOutfit())
        {
            state.initialModelQueued = true;
        }
    }

    void __cdecl SetCameraFromPage(const char* a_payload)
    {
        const auto parsed = Meridian::NifTest::ParseCameraPayload(a_payload);
        auto& state = State();
        if (!parsed || state.nifView == nullptr ||
            state.surface == Meridian::UI::RenderLayer::INVALID_SURFACE_HANDLE)
        {
            return;
        }

        state.camera.yawDegrees = parsed->yawDegrees;
        state.camera.pitchDegrees = parsed->pitchDegrees;
        state.camera.distanceScale = parsed->distanceScale;
        state.camera.panX = parsed->panX;
        state.camera.panY = parsed->panY;
        state.camera.panZ = parsed->panZ;
        state.nifView->SetCamera(state.surface, &state.camera);
    }

    void __cdecl SetLayoutFromPage(const char* a_payload)
    {
        const auto parsed = Meridian::NifTest::ParseLayoutPayload(a_payload);
        auto& state = State();
        if (!parsed || state.renderLayers == nullptr ||
            state.surface == Meridian::UI::RenderLayer::INVALID_SURFACE_HANDLE)
        {
            LOG_WARN("rejected preview layout payload");
            return;
        }
        if (!state.renderLayers->SetRect(
                state.surface, parsed->x, parsed->y, parsed->width, parsed->height))
        {
            LOG_WARN("rejected preview layout rectangle {},{},{},{}",
                         parsed->x,
                         parsed->y,
                         parsed->width,
                         parsed->height);
            return;
        }
        state.layoutReady = true;
        state.renderLayers->SetVisible(state.surface, state.testVisible);
        LOG_INFO("applied preview layout {},{},{},{}",
                     parsed->x,
                     parsed->y,
                     parsed->width,
                     parsed->height);
    }

    void __cdecl FrameModelFromPage(const char*)
    {
        auto& state = State();
        if (state.nifView != nullptr &&
            state.surface != Meridian::UI::RenderLayer::INVALID_SURFACE_HANDLE)
        {
            state.nifView->FrameModel(state.surface);
        }
    }

    void __cdecl LoadModelFromPage(const char* a_payload)
    {
        const auto modelPath = Meridian::NifTest::ParseModelPathPayload(a_payload);
        if (!modelPath)
        {
            LOG_WARN("rejected invalid model selector payload");
            return;
        }
        QueueModel(*modelPath);
    }

    void __cdecl SetLightingFromPage(const char* a_payload)
    {
        const auto parsed = Meridian::NifTest::ParseLightingPayload(a_payload);
        auto& state = State();
        if (!parsed || state.nifView == nullptr ||
            state.surface == Meridian::UI::RenderLayer::INVALID_SURFACE_HANDLE)
        {
            return;
        }
        state.camera.lightingPreset = parsed->preset;
        state.camera.exposureStops = parsed->exposureStops;
        if (!state.nifView->SetCamera(state.surface, &state.camera))
        {
            LOG_WARN("Meridian.NifView/1 rejected lighting state");
        }
    }

    void __cdecl LoadNifOutfitFromPage(const char*)
    {
        auto& state = State();
        state.outfitSource = OutfitSource::NifPaths;
        state.outfitVisibility.fill(true);
        QueueActiveOutfit();
    }

    void __cdecl LoadArmorOutfitFromPage(const char*)
    {
        auto& state = State();
        state.outfitSource = OutfitSource::ArmorRecords;
        state.outfitVisibility.fill(true);
        QueueActiveOutfit();
    }

    void __cdecl LoadModdedArmorOutfitFromPage(const char*)
    {
        auto& state = State();
        state.outfitSource = OutfitSource::ModdedArmorRecords;
        state.outfitVisibility.fill(true);
        QueueActiveOutfit();
    }

    void __cdecl LoadLayeredArmorFromPage(const char*)
    {
        auto& state = State();
        state.outfitSource = OutfitSource::LayeredArmorRecord;
        state.outfitVisibility.fill(true);
        QueueActiveOutfit();
    }

    void __cdecl LoadTextureSwapArmorFromPage(const char*)
    {
        auto& state = State();
        state.outfitSource = OutfitSource::TextureSwapArmorRecord;
        state.outfitVisibility.fill(true);
        QueueActiveOutfit();
    }

    void __cdecl LoadPlayerOutfitFromPage(const char*)
    {
        auto& state = State();
        state.outfitSource = OutfitSource::PlayerEquippedArmor;
        state.outfitVisibility.fill(true);
        QueuePlayerEquippedOutfit(true);
    }

    void __cdecl LoadPlayerActorFromPage(const char*)
    {
        auto& state = State();
        state.outfitSource = OutfitSource::PlayerActorAppearance;
        state.outfitVisibility.fill(true);
        QueuePlayerActorAppearance();
    }

    void __cdecl SetWeightFromPage(const char* a_payload)
    {
        const auto weight = Meridian::NifTest::ParseWeightPayload(a_payload);
        if (!weight)
        {
            LOG_WARN("rejected invalid Skyrim weight payload");
            return;
        }
        auto& state = State();
        state.outfitWeight = *weight;
        QueueActiveOutfit();
    }

    void __cdecl SyncResolvedWeightFromPage(const char* a_payload)
    {
        const auto weight = Meridian::NifTest::ParseWeightPayload(a_payload);
        if (weight)
        {
            State().outfitWeight = *weight;
        }
    }

    void __cdecl SetObjectVisibleFromPage(const char* a_payload)
    {
        const auto parsed = Meridian::NifTest::ParseObjectVisibilityPayload(a_payload);
        auto& state = State();
        if (!parsed || state.nifScene == nullptr ||
            state.surface == Meridian::UI::RenderLayer::INVALID_SURFACE_HANDLE)
        {
            return;
        }
        if (!state.nifScene->SetObjectVisible(
                state.surface, parsed->object, parsed->visible))
        {
            LOG_WARN("Meridian.NifScene/1 rejected object visibility state");
            return;
        }
        constexpr std::array handles{
            CUIRASS_OBJECT, HELMET_OBJECT, GAUNTLETS_OBJECT, BOOTS_OBJECT};
        for (std::size_t index = 0; index < handles.size(); ++index)
        {
            if (handles[index] == parsed->object)
            {
                state.outfitVisibility[index] = parsed->visible;
                break;
            }
        }
    }

    void SetTestVisible(bool a_visible)
    {
        auto& state = State();
        if (state.views == nullptr || state.view == Meridian::UI::View::INVALID_VIEW_HANDLE)
        {
            LOG_WARN("Alt+N ignored because the standalone NIF view is not ready");
            return;
        }

        if (a_visible)
        {
            if (!state.views->Show(state.view))
            {
                LOG_WARN("Alt+N could not show the standalone NIF view");
                return;
            }
            state.testVisible = true;
            if (state.renderLayers != nullptr && state.layoutReady &&
                state.surface != Meridian::UI::RenderLayer::INVALID_SURFACE_HANDLE)
            {
                state.renderLayers->SetVisible(state.surface, true);
            }
            const auto focus = state.views->TryFocus(
                state.view, Meridian::UI::View::FocusMode::PauseGame);
            LOG_INFO("Alt+N opened the standalone NIF view; focus result {}",
                         static_cast<std::uint32_t>(focus));
            return;
        }

        state.testVisible = false;
        state.views->Unfocus(state.view);
        state.views->Hide(state.view);
        if (state.renderLayers != nullptr &&
            state.surface != Meridian::UI::RenderLayer::INVALID_SURFACE_HANDLE)
        {
            state.renderLayers->SetVisible(state.surface, false);
        }
        LOG_INFO("standalone NIF view hidden; focus and native surface released");
    }

    void __cdecl CloseTestFromPage(const char*)
    {
        SetTestVisible(false);
    }

    void __cdecl OnControlsReady(Meridian::UI::View::ViewHandle a_view)
    {
        auto& state = State();
        if (state.views == nullptr || state.view != a_view)
        {
            return;
        }

        if (state.testVisible)
        {
            const auto focus = state.views->TryFocus(
                state.view, Meridian::UI::View::FocusMode::PauseGame);
            LOG_INFO("camera controls ready; focus result {}",
                         static_cast<std::uint32_t>(focus));
        }
        else
        {
            LOG_INFO("camera controls ready and hidden; press Alt+N to open");
        }
    }

    void TryCreateControls()
    {
        auto& state = State();
        if (!state.inputLoaded || !state.dataLoaded || state.views == nullptr ||
            state.view != Meridian::UI::View::INVALID_VIEW_HANDLE)
        {
            return;
        }

        Meridian::UI::View::ViewCreateInfo info{};
        info.ownerName = VIEW_OWNER;
        info.viewName = "camera-controls";
        info.startUrl = VIEW_URL;
        info.frameRate = 60;
        info.initiallyVisible = false;
        info.onDOMReady = OnControlsReady;
        state.view = state.views->CreateView(&info);
        if (state.view == Meridian::UI::View::INVALID_VIEW_HANDLE)
        {
            LOG_ERROR("failed to create the standalone NIF camera control view");
            return;
        }

        const bool cameraRegistered = state.views->RegisterListener(
            state.view, "meridianNifSetCamera", SetCameraFromPage);
        const bool layoutRegistered = state.views->RegisterListener(
            state.view, "meridianNifLayout", SetLayoutFromPage);
        const bool frameRegistered = state.views->RegisterListener(
            state.view, "meridianNifFrame", FrameModelFromPage);
        const bool loadRegistered = state.views->RegisterListener(
            state.view, "meridianNifLoad", LoadModelFromPage);
        const bool lightingRegistered = state.views->RegisterListener(
            state.view, "meridianNifSetLighting", SetLightingFromPage);
        const bool outfitRegistered = state.views->RegisterListener(
            state.view, "meridianNifLoadOutfit", LoadNifOutfitFromPage);
        const bool armorOutfitRegistered = state.views->RegisterListener(
            state.view, "meridianNifLoadArmorOutfit", LoadArmorOutfitFromPage);
        const bool moddedArmorOutfitRegistered = state.views->RegisterListener(
            state.view, "meridianNifLoadModdedArmorOutfit", LoadModdedArmorOutfitFromPage);
        const bool layeredArmorRegistered = state.views->RegisterListener(
            state.view, "meridianNifLoadLayeredArmor", LoadLayeredArmorFromPage);
        const bool textureSwapArmorRegistered = state.views->RegisterListener(
            state.view,
            "meridianNifLoadTextureSwapArmor",
            LoadTextureSwapArmorFromPage);
        const bool playerOutfitRegistered = state.views->RegisterListener(
            state.view, "meridianNifLoadPlayerOutfit", LoadPlayerOutfitFromPage);
        const bool playerActorRegistered = state.views->RegisterListener(
            state.view, "meridianNifLoadPlayerActor", LoadPlayerActorFromPage);
        const bool weightRegistered = state.views->RegisterListener(
            state.view, "meridianNifSetWeight", SetWeightFromPage);
        const bool syncWeightRegistered = state.views->RegisterListener(
            state.view, "meridianNifSyncWeight", SyncResolvedWeightFromPage);
        const bool visibilityRegistered = state.views->RegisterListener(
            state.view, "meridianNifSetObjectVisible", SetObjectVisibleFromPage);
        const bool closeRegistered = state.views->RegisterListener(
            state.view, "meridianNifClose", CloseTestFromPage);
        if (!cameraRegistered || !layoutRegistered || !frameRegistered ||
            !loadRegistered || !lightingRegistered || !outfitRegistered ||
            !armorOutfitRegistered || !moddedArmorOutfitRegistered ||
            !layeredArmorRegistered || !textureSwapArmorRegistered ||
            !playerOutfitRegistered || !playerActorRegistered ||
            !weightRegistered || !syncWeightRegistered ||
            !visibilityRegistered || !closeRegistered)
        {
            LOG_ERROR("failed to register one or more NIF camera control listeners");
            state.views->DestroyView(state.view);
            state.view = Meridian::UI::View::INVALID_VIEW_HANDLE;
            return;
        }

        LOG_INFO("created camera control view {} from '{}'", state.view, VIEW_URL);
    }

    void QueryAPIs()
    {
        auto& state = State();
        if (state.APIsQueried)
        {
            return;
        }
        state.APIsQueried = true;

        state.renderLayers = Meridian::UI::RenderLayer::Query(&state.settings, PLUGIN_NAME);
        state.nifView = Meridian::UI::NifView::Query(&state.settings, PLUGIN_NAME);
        state.nifScene = Meridian::UI::NifScene::Query4(&state.settings, PLUGIN_NAME);
        state.views = Meridian::UI::View::Query(&state.settings, PLUGIN_NAME);
        if (state.renderLayers == nullptr || state.nifView == nullptr ||
            state.nifScene == nullptr)
        {
            LOG_ERROR("Meridian.RenderLayer/1, Meridian.NifView/1, or Meridian.NifScene/4 is unavailable; install/enable the current Meridian UI build");
        }
        if (state.views == nullptr)
        {
            LOG_ERROR("Meridian.View/1 is unavailable; native NIF preview will continue without controls");
        }
    }

    void TryCreateSurface()
    {
        auto& state = State();
        if (!state.inputLoaded || !state.dataLoaded ||
            state.surface != Meridian::UI::RenderLayer::INVALID_SURFACE_HANDLE)
        {
            TryQueueInitialModel();
            return;
        }

        if (state.renderLayers == nullptr || state.nifView == nullptr)
        {
            return;
        }

        Meridian::UI::RenderLayer::SurfaceCreateInfo surfaceInfo{};
        surfaceInfo.ownerName = PLUGIN_NAME;
        surfaceInfo.surfaceName = "standalone-preview";
        surfaceInfo.x = 200;
        surfaceInfo.y = 200;
        surfaceInfo.width = 640;
        surfaceInfo.height = 480;
        // The fullscreen Chromium view is transparent and remains at z=0;
        // the native mesh sits behind its measured preview aperture.
        surfaceInfo.zOrder = -100;
        surfaceInfo.initiallyVisible = false;
        state.surface = state.renderLayers->CreateSurface(&surfaceInfo);
        if (state.surface == Meridian::UI::RenderLayer::INVALID_SURFACE_HANDLE)
        {
            LOG_ERROR("failed to create the standalone Meridian NIF test surface");
            return;
        }

        LOG_INFO("created standalone NIF test surface {}", state.surface);
        TryQueueInitialModel();
    }

    void TryStartTest()
    {
        TryCreateSurface();
        TryCreateControls();
        TryQueueInitialModel();
    }

    class NifTestHotkeySink final : public RE::BSTEventSink<RE::InputEvent*>
    {
    public:
        static NifTestHotkeySink& GetSingleton()
        {
            static NifTestHotkeySink singleton;
            return singleton;
        }

        void Register()
        {
            if (registered)
            {
                return;
            }
            auto* input = RE::BSInputDeviceManager::GetSingleton();
            if (input == nullptr)
            {
                LOG_ERROR("could not register the Alt+N input sink");
                return;
            }
            input->AddEventSink(this);
            registered = true;
            LOG_INFO("registered Alt+N standalone NIF view toggle");
        }

        RE::BSEventNotifyControl ProcessEvent(
            RE::InputEvent* const* a_events,
            RE::BSTEventSource<RE::InputEvent*>*) override
        {
            if (a_events == nullptr)
            {
                return RE::BSEventNotifyControl::kContinue;
            }
            auto* keyboard = RE::BSInputDeviceManager::GetSingleton()->GetKeyboard();
            const auto* keyState = keyboard != nullptr ? keyboard->GetRuntimeData().curState : nullptr;
            const auto stateSize = keyboard != nullptr ? sizeof(keyboard->GetRuntimeData().curState) : 0;
            for (auto* event = *a_events; event != nullptr; event = event->next)
            {
                auto* button = event->AsButtonEvent();
                if (button != nullptr && button->GetDevice() == RE::INPUT_DEVICE::kKeyboard &&
                    Meridian::NifTest::IsToggleHotkey(
                        button->GetIDCode(), button->IsDown(), keyState, stateSize))
                {
                    SetTestVisible(!State().testVisible);
                    break;
                }
            }
            return RE::BSEventNotifyControl::kContinue;
        }

    private:
        bool registered = false;
    };

    void ProcessMessage(SKSE::MessagingInterface::Message* a_message)
    {
        if (a_message == nullptr)
        {
            return;
        }

        auto& state = State();
        switch (a_message->type)
        {
        case SKSE::MessagingInterface::kInputLoaded:
            state.inputLoaded = true;
            QueryAPIs();
            NifTestHotkeySink::GetSingleton().Register();
            TryStartTest();
            break;
        case SKSE::MessagingInterface::kDataLoaded:
            state.dataLoaded = true;
            TryStartTest();
            break;
        default:
            break;
        }
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
    if (a_skse->IsEditor())
    {
        return false;
    }

    SKSE::Init(a_skse, false);
    if (!InitializeLog())
    {
        return false;
    }

    LOG_INFO("{} {} Plugin Loaded", Meridian::UI::LibVersion::PROJECT_NAME,Meridian::UI::LibVersion::AS_STRING);

    if (!SKSE::GetMessagingInterface()->RegisterListener(ProcessMessage))
    {
        LOG_CRITICAL("failed to register the SKSE lifecycle listener");
        return false;
    }

    LOG_INFO("standalone test loaded; Horde is not used or modified");
    return true;
}
