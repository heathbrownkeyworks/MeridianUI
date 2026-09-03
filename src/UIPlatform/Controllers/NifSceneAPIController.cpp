#include "Controllers/NifSceneAPIController.h"

#include "Controllers/ActorAppearancePolicy.h"
#include "Controllers/LiveActorSnapshot.h"
#include "Controllers/NifLoadGeneration.h"
#include "Controllers/NifArmorModelPath.h"
#include "Controllers/NifModelPath.h"
#include "Controllers/RenderLayerAPIController.h"
#include "Menus/NativeSurfaceMenu.h"
#include "Render/NifSceneComposition.h"
#include "Render/NifMaterialTint.h"
#include "Render/NifTextureOverride.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Meridian::Controllers
{
    namespace
    {
        struct RequestedObject
        {
            Meridian::UI::NifScene::ObjectHandle object =
                Meridian::UI::NifScene::INVALID_OBJECT_HANDLE;
            std::string lowModelPath;
            std::string highModelPath;
            std::vector<Meridian::Render::NifPreview::ModelTextureOverride>
                textureOverrides;
            Meridian::Render::NifPreview::ActorMaterialTintOverride materialTint{};
            bool visible = true;
        };

        struct RequestedArmor
        {
            Meridian::UI::NifScene::ObjectHandle object =
                Meridian::UI::NifScene::INVALID_OBJECT_HANDLE;
            std::uint32_t armorFormID = 0;
            std::uint32_t blockedBipedSlots = 0;
            bool allowEmptyAfterFiltering = false;
            bool visible = true;
        };

        bool CopyTextureOverrides(
            const RE::TESModelTextureSwap& a_model,
            std::vector<Meridian::Render::NifPreview::ModelTextureOverride>& a_output)
        {
            using namespace Meridian::Render::NifPreview;
            a_output.clear();
            if (a_model.numAlternateTextures == 0)
            {
                return true;
            }
            if (a_model.alternateTextures == nullptr ||
                a_model.numAlternateTextures > MAX_MODEL_TEXTURE_OVERRIDES)
            {
                return false;
            }

            a_output.reserve(a_model.numAlternateTextures);
            for (std::uint32_t index = 0; index < a_model.numAlternateTextures; ++index)
            {
                const auto& source = a_model.alternateTextures[index];
                if (source.textureSet == nullptr)
                {
                    return false;
                }

                ModelTextureOverride target{};
                target.index3D = source.index3D;
                if (const auto* name = source.name3D.c_str(); name != nullptr)
                {
                    target.name3D = name;
                }
                const auto copyPath = [&](RE::BSTextureSet::Texture a_texture,
                                          std::string& a_destination) {
                    if (const auto* path = source.textureSet->GetTexturePath(a_texture);
                        path != nullptr)
                    {
                        a_destination = path;
                    }
                };
                copyPath(RE::BSTextureSet::Textures::kDiffuse,
                         target.textureSet.diffuse);
                copyPath(RE::BSTextureSet::Textures::kNormal,
                         target.textureSet.normal);
                copyPath(RE::BSTextureSet::Textures::kEnvironmentMask,
                         target.textureSet.environmentMask);
                copyPath(RE::BSTextureSet::Textures::kEnvironment,
                         target.textureSet.environment);
                copyPath(RE::BSTextureSet::Textures::kSpecular,
                         target.textureSet.specular);
                target.textureSet.modelSpaceNormals = source.textureSet->flags.all(
                    RE::BGSTextureSet::Flag::kHasModelSpaceNormalMap);
                target.textureSet.noSpecularMap = source.textureSet->flags.all(
                    RE::BGSTextureSet::Flag::kNoSpecularMap);
                a_output.push_back(std::move(target));
            }
            return true;
        }

        void LoadAndSubmitRequestedScene(
            const std::weak_ptr<Meridian::Menus::NativeSurfaceMenu>& a_weakSurface,
            std::uint64_t a_generation,
            std::vector<RequestedObject> a_requested,
            float a_weight,
            bool a_frameOnLoad)
        {
            auto& controller = NifSceneAPIController::GetSingleton();
            auto target = a_weakSurface.lock();
            if (controller.IsShuttingDown() || target == nullptr)
            {
                return;
            }

            std::vector<Meridian::Render::NifPreview::SceneSubmissionObject> scenes;
            scenes.reserve(a_requested.size());
            try
            {
                const RE::BSModelDB::DBTraits::ArgsType arguments{};
                for (auto& request : a_requested)
                {
                    const auto demand = [&](const std::string& a_path,
                                            RE::NiPointer<RE::NiNode>& a_scene) {
                        const auto error = RE::BSModelDB::Demand(
                            a_path.c_str(), a_scene, arguments);
                        if (error != RE::BSResource::ErrorCode::kNone || a_scene == nullptr)
                        {
                            spdlog::warn(
                                "{}: BSModelDB could not load scene object {} '{}' (error {})",
                                NameOf(NifSceneAPIController),
                                request.object,
                                a_path,
                                static_cast<std::int32_t>(error));
                            return false;
                        }
                        return true;
                    };

                    RE::NiPointer<RE::NiNode> lowScene;
                    if (!demand(request.lowModelPath, lowScene))
                    {
                        target->FailNifLoad(
                            a_generation, Meridian::UI::NifView::Status::Failed);
                        return;
                    }

                    RE::NiPointer<RE::NiNode> highScene;
                    if (!request.highModelPath.empty() &&
                        !demand(request.highModelPath, highScene))
                    {
                        target->FailNifLoad(
                            a_generation, Meridian::UI::NifView::Status::Failed);
                        return;
                    }
                    scenes.push_back({
                        .object = request.object,
                        .scene = std::move(lowScene),
                        .weightScene = std::move(highScene),
                        .lowModelPath = request.lowModelPath,
                        .highModelPath = request.highModelPath,
                        .textureOverrides = std::move(request.textureOverrides),
                        .materialTint = request.materialTint,
                        .weight = a_weight,
                        .visible = request.visible,
                    });
                }

                if (!controller.IsShuttingDown())
                {
                    target->SubmitNifSceneBatch(
                        a_generation, std::move(scenes), a_frameOnLoad);
                }
            }
            catch (const std::exception& error)
            {
                spdlog::error("{}: exception loading scene: {}",
                              NameOf(NifSceneAPIController), error.what());
                target->FailNifLoad(
                    a_generation, Meridian::UI::NifView::Status::Failed);
            }
            catch (...)
            {
                spdlog::error("{}: unknown exception loading scene",
                              NameOf(NifSceneAPIController));
                target->FailNifLoad(
                    a_generation, Meridian::UI::NifView::Status::Failed);
            }
        }

        bool QueueRequestedScene(
            Meridian::UI::RenderLayer::SurfaceHandle a_surface,
            std::vector<RequestedObject> a_requested,
            float a_weight,
            bool a_frameOnLoad)
        {
            auto& controller = NifSceneAPIController::GetSingleton();
            auto surface = RenderLayerAPIController::GetSingleton().GetNativeSurface(a_surface);
            if (controller.IsShuttingDown() || surface == nullptr || !surface->IsReady())
            {
                return false;
            }
            const auto taskInterface = SKSE::GetTaskInterface();
            if (taskInterface == nullptr)
            {
                return false;
            }

            const auto generation = NextNifLoadGeneration();
            surface->BeginNifLoad(generation);
            const std::weak_ptr<Meridian::Menus::NativeSurfaceMenu> weakSurface = surface;
            spdlog::info("{}: queued {} scene objects at weight {:.1f} for surface {}",
                         NameOf(NifSceneAPIController),
                         a_requested.size(),
                         a_weight * 100.0f,
                         a_surface);

            taskInterface->AddTask([weakSurface,
                                    generation,
                                    weight = a_weight,
                                    frameOnLoad = a_frameOnLoad,
                                    requested = std::move(a_requested)]() mutable {
                LoadAndSubmitRequestedScene(
                    weakSurface,
                    generation,
                    std::move(requested),
                    weight,
                    frameOnLoad);
            });
            return true;
        }

        bool AppendResolvedArmorParts(
            const std::vector<RequestedArmor>& a_requestedArmors,
            RE::TESRace* a_race,
            Meridian::UI::NifScene::Sex a_sex,
            std::vector<RequestedObject>& a_requested)
        {
            using Meridian::Controllers::ActorAppearance::ShouldIncludeSkinAddon;
            const auto sexIndex = static_cast<std::size_t>(a_sex);
            for (const auto& item : a_requestedArmors)
            {
                auto* armor = RE::TESForm::LookupByID<RE::TESObjectARMO>(
                    item.armorFormID);
                if (armor == nullptr)
                {
                    spdlog::warn("{}: armor {:08X} was not found",
                                 NameOf(NifSceneAPIController), item.armorFormID);
                    return false;
                }

                const auto firstPart = a_requested.size();
                bool hadIntentionalBlankProxy = false;
                bool hadCoveredAddon = false;
                std::unordered_set<RE::TESObjectARMA*> resolvedAddons;
                for (auto* addon : armor->armorAddons)
                {
                    if (addon == nullptr || !addon->IsValidRace(a_race) ||
                        !resolvedAddons.insert(addon).second)
                    {
                        continue;
                    }

                    const auto addonSlots = addon->GetSlotMask().underlying();
                    if (item.blockedBipedSlots != 0 &&
                        !ShouldIncludeSkinAddon(addonSlots, item.blockedBipedSlots))
                    {
                        hadCoveredAddon = true;
                        spdlog::info(
                            "{}: skin armor {:08X} ARMA {:08X} slots {:08X} intersect occupied slots {:08X}; skipped",
                            NameOf(NifSceneAPIController),
                            item.armorFormID,
                            addon->GetFormID(),
                            addonSlots,
                            item.blockedBipedSlots);
                        continue;
                    }

                    const auto& modelSwap = addon->bipedModels[sexIndex];
                    const char* model = modelSwap.GetModel();
                    if (model == nullptr || std::string_view(model).empty())
                    {
                        spdlog::info(
                            "{}: armor {:08X} ARMA {:08X} has no sex {} third-person model; skipped",
                            NameOf(NifSceneAPIController),
                            item.armorFormID,
                            addon->GetFormID(),
                            static_cast<std::uint32_t>(a_sex));
                        continue;
                    }
                    if (IsIntentionalBlankArmorModelPath(model))
                    {
                        hadIntentionalBlankProxy = true;
                        spdlog::info(
                            "{}: armor {:08X} ARMA {:08X} uses intentional blank proxy '{}'; skipped",
                            NameOf(NifSceneAPIController),
                            item.armorFormID,
                            addon->GetFormID(),
                            model);
                        continue;
                    }

                    const auto paths = ResolveArmorModelPaths(
                        model, addon->data.modelRange[sexIndex]);
                    if (!paths)
                    {
                        spdlog::warn(
                            "{}: armor {:08X} ARMA {:08X} could not resolve a safe sex model (path status {})",
                            NameOf(NifSceneAPIController),
                            item.armorFormID,
                            addon->GetFormID(),
                            static_cast<std::uint32_t>(paths.error));
                        return false;
                    }
                    if (a_requested.size() >=
                        Meridian::Render::NifPreview::MAX_RESOLVED_SCENE_PARTS)
                    {
                        spdlog::warn("{}: armor scene exceeded the {} resolved-part cap",
                                     NameOf(NifSceneAPIController),
                                     Meridian::Render::NifPreview::MAX_RESOLVED_SCENE_PARTS);
                        return false;
                    }

                    std::vector<Meridian::Render::NifPreview::ModelTextureOverride>
                        textureOverrides;
                    if (!CopyTextureOverrides(modelSwap, textureOverrides))
                    {
                        spdlog::warn(
                            "{}: armor {:08X} ARMA {:08X} has malformed or excessive alternate textures",
                            NameOf(NifSceneAPIController),
                            item.armorFormID,
                            addon->GetFormID());
                        return false;
                    }

                    spdlog::info(
                        "{}: armor {:08X} resolved through ARMA {:08X} to '{}'{} with {} alternate textures",
                        NameOf(NifSceneAPIController),
                        item.armorFormID,
                        addon->GetFormID(),
                        paths.lowModelPath,
                        paths.highModelPath.empty() ? "" : " plus weighted endpoint",
                        textureOverrides.size());
                    a_requested.push_back({
                        .object = item.object,
                        .lowModelPath = paths.lowModelPath,
                        .highModelPath = paths.highModelPath,
                        .textureOverrides = std::move(textureOverrides),
                        .visible = item.visible,
                    });
                }

                if (a_requested.size() == firstPart)
                {
                    if (hadIntentionalBlankProxy)
                    {
                        spdlog::info(
                            "{}: armor {:08X} contains only intentional blank proxy models; omitted from scene",
                            NameOf(NifSceneAPIController),
                            item.armorFormID);
                        continue;
                    }
                    if (item.allowEmptyAfterFiltering && hadCoveredAddon)
                    {
                        spdlog::info(
                            "{}: armor {:08X} contains no visible foundation geometry after filtering; omitted from scene",
                            NameOf(NifSceneAPIController),
                            item.armorFormID);
                        continue;
                    }
                    if (item.allowEmptyAfterFiltering)
                    {
                        spdlog::info(
                            "{}: optional skin armor {:08X} has no renderable race-valid model; omitted from scene",
                            NameOf(NifSceneAPIController),
                            item.armorFormID);
                        continue;
                    }
                    spdlog::warn("{}: armor {:08X} has no race-valid ARMA model for sex {}",
                                 NameOf(NifSceneAPIController),
                                 item.armorFormID,
                                 static_cast<std::uint32_t>(a_sex));
                    return false;
                }
            }
            return true;
        }

        bool QueueRequestedArmorScene(
            Meridian::UI::RenderLayer::SurfaceHandle a_surface,
            std::vector<RequestedArmor> a_requested,
            std::uint32_t a_raceFormID,
            Meridian::UI::NifScene::Sex a_sex,
            float a_weight,
            bool a_frameOnLoad)
        {
            auto& controller = NifSceneAPIController::GetSingleton();
            auto surface = RenderLayerAPIController::GetSingleton().GetNativeSurface(a_surface);
            if (controller.IsShuttingDown() || surface == nullptr || !surface->IsReady())
            {
                return false;
            }
            const auto taskInterface = SKSE::GetTaskInterface();
            if (taskInterface == nullptr)
            {
                return false;
            }

            const auto generation = NextNifLoadGeneration();
            surface->BeginNifLoad(generation);
            const std::weak_ptr<Meridian::Menus::NativeSurfaceMenu> weakSurface = surface;
            spdlog::info("{}: queued {} armor records for race {:08X}, sex {}, weight {:.1f} on surface {}",
                         NameOf(NifSceneAPIController),
                         a_requested.size(),
                         a_raceFormID,
                         static_cast<std::uint32_t>(a_sex),
                         a_weight * 100.0f,
                         a_surface);

            taskInterface->AddTask([weakSurface,
                                    generation,
                                    raceFormID = a_raceFormID,
                                    sex = a_sex,
                                    weight = a_weight,
                                    frameOnLoad = a_frameOnLoad,
                                    requestedArmors = std::move(a_requested)]() mutable {
                auto& asyncController = NifSceneAPIController::GetSingleton();
                auto target = weakSurface.lock();
                if (asyncController.IsShuttingDown() || target == nullptr)
                {
                    return;
                }

                try
                {
                    auto* race = RE::TESForm::LookupByID<RE::TESRace>(raceFormID);
                    if (race == nullptr)
                    {
                        spdlog::warn("{}: armor scene race {:08X} was not found",
                                     NameOf(NifSceneAPIController), raceFormID);
                        target->FailNifLoad(
                            generation, Meridian::UI::NifView::Status::Unsupported);
                        return;
                    }

                    std::vector<RequestedObject> requested;
                    requested.reserve(requestedArmors.size());
                    if (!AppendResolvedArmorParts(
                            requestedArmors, race, sex, requested))
                    {
                        target->FailNifLoad(
                            generation, Meridian::UI::NifView::Status::Unsupported);
                        return;
                    }

                    if (requested.empty())
                    {
                        spdlog::warn(
                            "{}: armor scene contains no renderable geometry after blank-proxy filtering",
                            NameOf(NifSceneAPIController));
                        target->FailNifLoad(
                            generation, Meridian::UI::NifView::Status::Unsupported);
                        return;
                    }

                    LoadAndSubmitRequestedScene(
                        weakSurface,
                        generation,
                        std::move(requested),
                        weight,
                        frameOnLoad);
                }
                catch (const std::exception& error)
                {
                    spdlog::error("{}: exception resolving armor scene: {}",
                                  NameOf(NifSceneAPIController), error.what());
                    target->FailNifLoad(
                        generation, Meridian::UI::NifView::Status::Failed);
                }
                catch (...)
                {
                    spdlog::error("{}: unknown exception resolving armor scene",
                                  NameOf(NifSceneAPIController));
                    target->FailNifLoad(
                        generation, Meridian::UI::NifView::Status::Failed);
                }
            });
            return true;
        }

        bool QueueActorAppearanceScene(
            Meridian::UI::RenderLayer::SurfaceHandle a_surface,
            std::uint32_t a_actorFormID,
            std::uint32_t a_parts,
            bool a_frameOnLoad)
        {
            using namespace Meridian::UI::NifScene;
            auto& controller = NifSceneAPIController::GetSingleton();
            auto surface = RenderLayerAPIController::GetSingleton().GetNativeSurface(a_surface);
            if (controller.IsShuttingDown() || surface == nullptr || !surface->IsReady())
            {
                return false;
            }
            const auto taskInterface = SKSE::GetTaskInterface();
            if (taskInterface == nullptr)
            {
                return false;
            }

            const auto generation = NextNifLoadGeneration();
            surface->BeginNifLoad(generation);
            const std::weak_ptr<Meridian::Menus::NativeSurfaceMenu> weakSurface = surface;
            spdlog::info("{}: queued actor appearance {:08X}, parts {:02X}, on surface {}",
                         NameOf(NifSceneAPIController),
                         a_actorFormID,
                         a_parts,
                         a_surface);

            taskInterface->AddTask([weakSurface,
                                    generation,
                                    actorFormID = a_actorFormID,
                                    parts = a_parts,
                                    frameOnLoad = a_frameOnLoad]() {
                auto& asyncController = NifSceneAPIController::GetSingleton();
                auto target = weakSurface.lock();
                if (asyncController.IsShuttingDown() || target == nullptr)
                {
                    return;
                }

                try
                {
                    using namespace Meridian::Controllers::LiveActorSnapshot;
                    auto* actor = actorFormID == 0x14 ?
                        RE::PlayerCharacter::GetSingleton() :
                        RE::TESForm::LookupByID<RE::Actor>(actorFormID);
                    auto* actor3D = actor != nullptr ? actor->Get3D(false) : nullptr;
                    auto* actorRoot = actor3D != nullptr ? actor3D->AsNode() : nullptr;
                    if (actor == nullptr || actorRoot == nullptr)
                    {
                        spdlog::warn(
                            "{}: loaded actor {:08X} has no third-person 3D root",
                            NameOf(NifSceneAPIController),
                            actorFormID);
                        target->FailNifLoad(
                            generation, Meridian::UI::NifView::Status::Unsupported);
                        return;
                    }

                    std::vector<RE::NiPointer<RE::NiAVObject>> liveActorRoots;
                    liveActorRoots.reserve(33);
                    std::unordered_set<const RE::NiAVObject*> seenRoots;
                    auto addRoot = [&](RE::NiAVObject* a_root) {
                        if (a_root != nullptr && seenRoots.insert(a_root).second)
                        {
                            liveActorRoots.emplace_back(a_root);
                        }
                    };

                    std::size_t equipmentRootCount = 0;
                    std::size_t skinRootCount = 0;
                    if ((parts & (ACTOR_APPEARANCE_PART_EQUIPMENT |
                                  ACTOR_APPEARANCE_PART_SKIN)) != 0)
                    {
                        const auto& biped = actor->GetBiped(false);
                        if (biped == nullptr)
                        {
                            spdlog::warn(
                                "{}: loaded actor {:08X} has no third-person biped",
                                NameOf(NifSceneAPIController),
                                actorFormID);
                            target->FailNifLoad(
                                generation, Meridian::UI::NifView::Status::Unsupported);
                            return;
                        }

                        auto* skin = actor->GetSkin();
                        for (std::uint32_t index = 0; index < 32; ++index)
                        {
                            const auto& object = biped->objects[index];
                            auto* armor = object.item != nullptr ?
                                object.item->As<RE::TESObjectARMO>() : nullptr;
                            if (!IsActorAppearanceBipedSlot(index) || armor == nullptr ||
                                object.partClone == nullptr)
                            {
                                continue;
                            }

                            const auto source = ClassifyBipedSource(armor == skin);
                            if (!ShouldCaptureSource(source, parts))
                            {
                                continue;
                            }
                            const auto previousSize = liveActorRoots.size();
                            addRoot(object.partClone.get());
                            if (liveActorRoots.size() != previousSize)
                            {
                                if (source == SourceKind::Skin)
                                {
                                    ++skinRootCount;
                                }
                                else
                                {
                                    ++equipmentRootCount;
                                }
                            }
                        }
                    }

                    std::size_t headRootCount = 0;
                    if ((parts & ACTOR_APPEARANCE_PART_HEAD) != 0)
                    {
                        auto* faceNode = actor->GetFaceNodeSkinned();
                        if (faceNode == nullptr)
                        {
                            spdlog::warn(
                                "{}: loaded actor {:08X} has no skinned face node",
                                NameOf(NifSceneAPIController),
                                actorFormID);
                            target->FailNifLoad(
                                generation, Meridian::UI::NifView::Status::Unsupported);
                            return;
                        }
                        const auto previousSize = liveActorRoots.size();
                        addRoot(faceNode);
                        headRootCount = liveActorRoots.size() != previousSize ? 1 : 0;
                    }

                    if (liveActorRoots.empty())
                    {
                        spdlog::warn(
                            "{}: loaded actor {:08X} has no selected live appearance roots",
                            NameOf(NifSceneAPIController),
                            actorFormID);
                        target->FailNifLoad(
                            generation, Meridian::UI::NifView::Status::Unsupported);
                        return;
                    }

                    std::vector<Meridian::Render::NifPreview::SceneSubmissionObject> scenes;
                    scenes.push_back({
                        .object = actorFormID,
                        .scene = RE::NiPointer<RE::NiNode>{actorRoot},
                        .liveActorRoots = std::move(liveActorRoots),
                        .liveActorFormID = actorFormID,
                        .lowModelPath = fmt::format("<loaded actor {:08X}>", actorFormID),
                        .visible = true,
                    });
                    spdlog::info(
                        "{}: loaded actor {:08X} snapshot selected {} equipment, {} skin, and {} face roots",
                        NameOf(NifSceneAPIController),
                        actorFormID,
                        equipmentRootCount,
                        skinRootCount,
                        headRootCount);
                    target->SubmitNifSceneBatch(
                        generation, std::move(scenes), frameOnLoad);
                }
                catch (const std::exception& error)
                {
                    spdlog::error("{}: exception resolving actor appearance: {}",
                                  NameOf(NifSceneAPIController), error.what());
                    target->FailNifLoad(
                        generation, Meridian::UI::NifView::Status::Failed);
                }
                catch (...)
                {
                    spdlog::error("{}: unknown exception resolving actor appearance",
                                  NameOf(NifSceneAPIController));
                    target->FailNifLoad(
                        generation, Meridian::UI::NifView::Status::Failed);
                }
            });
            return true;
        }
    }

    bool __cdecl NifSceneAPIController::ReplaceScene(
        const Meridian::UI::NifScene::SceneLoadInfo* a_info)
    {
        using namespace Meridian::UI::NifScene;
        if (m_isShuttingDown.load(std::memory_order_acquire) ||
            a_info == nullptr ||
            a_info->structSize < SCENE_LOAD_INFO_MIN_SIZE_1 ||
            a_info->objects == nullptr ||
            a_info->objectCount == 0 ||
            a_info->objectCount > MAX_SCENE_OBJECTS)
        {
            return false;
        }

        std::vector<RequestedObject> requested;
        requested.reserve(a_info->objectCount);
        std::unordered_set<ObjectHandle> handles;
        for (std::uint32_t index = 0; index < a_info->objectCount; ++index)
        {
            const auto& source = a_info->objects[index];
            if (source.structSize < SCENE_OBJECT_LOAD_INFO_MIN_SIZE_1 ||
                source.object == INVALID_OBJECT_HANDLE ||
                source.modelPath == nullptr ||
                !handles.insert(source.object).second)
            {
                return false;
            }

            std::string path;
            if (!NormalizeNifModelPath(source.modelPath, path))
            {
                spdlog::warn("{}: rejected unsafe or invalid scene NIF path",
                             NameOf(NifSceneAPIController));
                return false;
            }
            requested.push_back({
                .object = source.object,
                .lowModelPath = std::move(path),
                .visible = source.visible,
            });
        }
        return QueueRequestedScene(
            a_info->surface, std::move(requested), 0.0f, a_info->frameOnLoad);
    }

    bool __cdecl NifSceneAPIController::ReplaceWeightedScene(
        const Meridian::UI::NifScene::WeightedSceneLoadInfo* a_info)
    {
        using namespace Meridian::UI::NifScene;
        if (m_isShuttingDown.load(std::memory_order_acquire) ||
            a_info == nullptr ||
            a_info->structSize < WEIGHTED_SCENE_LOAD_INFO_MIN_SIZE_2 ||
            a_info->objects == nullptr ||
            a_info->objectCount == 0 ||
            a_info->objectCount > MAX_SCENE_OBJECTS ||
            !std::isfinite(a_info->weight) ||
            a_info->weight < 0.0f || a_info->weight > 100.0f)
        {
            return false;
        }

        std::vector<RequestedObject> requested;
        requested.reserve(a_info->objectCount);
        std::unordered_set<ObjectHandle> handles;
        for (std::uint32_t index = 0; index < a_info->objectCount; ++index)
        {
            const auto& source = a_info->objects[index];
            if (source.structSize < WEIGHTED_SCENE_OBJECT_LOAD_INFO_MIN_SIZE_2 ||
                source.object == INVALID_OBJECT_HANDLE ||
                source.lowModelPath == nullptr ||
                !handles.insert(source.object).second)
            {
                return false;
            }

            RequestedObject object{.object = source.object, .visible = source.visible};
            if (!NormalizeNifModelPath(source.lowModelPath, object.lowModelPath) ||
                (source.highModelPath != nullptr &&
                 !NormalizeNifModelPath(source.highModelPath, object.highModelPath)))
            {
                spdlog::warn("{}: Meridian.NifScene/{} rejected an unsafe weighted NIF path",
                             NameOf(NifSceneAPIController),
                             WEIGHTED_INTERFACE_VERSION);
                return false;
            }
            requested.push_back(std::move(object));
        }

        return QueueRequestedScene(
            a_info->surface,
            std::move(requested),
            a_info->weight / 100.0f,
            a_info->frameOnLoad);
    }

    bool __cdecl NifSceneAPIController::ReplaceArmorScene(
        const Meridian::UI::NifScene::ArmorSceneLoadInfo* a_info)
    {
        using namespace Meridian::UI::NifScene;
        if (m_isShuttingDown.load(std::memory_order_acquire) ||
            a_info == nullptr ||
            a_info->structSize < ARMOR_SCENE_LOAD_INFO_MIN_SIZE_3 ||
            a_info->objects == nullptr ||
            a_info->objectCount == 0 ||
            a_info->objectCount > MAX_SCENE_OBJECTS ||
            a_info->raceFormID == 0 ||
            !IsValidSex(a_info->sex) ||
            !std::isfinite(a_info->weight) ||
            a_info->weight < 0.0f || a_info->weight > 100.0f)
        {
            return false;
        }

        std::vector<RequestedArmor> requested;
        requested.reserve(a_info->objectCount);
        std::unordered_set<ObjectHandle> handles;
        for (std::uint32_t index = 0; index < a_info->objectCount; ++index)
        {
            const auto& source = a_info->objects[index];
            if (source.structSize < ARMOR_SCENE_OBJECT_LOAD_INFO_MIN_SIZE_3 ||
                source.object == INVALID_OBJECT_HANDLE ||
                source.armorFormID == 0 ||
                !handles.insert(source.object).second)
            {
                return false;
            }
            requested.push_back({
                .object = source.object,
                .armorFormID = source.armorFormID,
                .visible = source.visible,
            });
        }

        return QueueRequestedArmorScene(
            a_info->surface,
            std::move(requested),
            a_info->raceFormID,
            a_info->sex,
            a_info->weight / 100.0f,
            a_info->frameOnLoad);
    }

    bool __cdecl NifSceneAPIController::ReplaceActorAppearanceScene(
        const Meridian::UI::NifScene::ActorAppearanceSceneLoadInfo* a_info)
    {
        using namespace Meridian::UI::NifScene;
        if (m_isShuttingDown.load(std::memory_order_acquire) ||
            a_info == nullptr ||
            a_info->structSize < ACTOR_APPEARANCE_SCENE_LOAD_INFO_MIN_SIZE_4 ||
            a_info->surface == Meridian::UI::RenderLayer::INVALID_SURFACE_HANDLE ||
            a_info->actorFormID == 0 ||
            !IsValidActorAppearanceParts(a_info->parts))
        {
            return false;
        }

        return QueueActorAppearanceScene(
            a_info->surface,
            a_info->actorFormID,
            a_info->parts,
            a_info->frameOnLoad);
    }

    void __cdecl NifSceneAPIController::ClearScene(
        Meridian::UI::RenderLayer::SurfaceHandle a_surface)
    {
        if (m_isShuttingDown.load(std::memory_order_acquire))
        {
            return;
        }
        const auto surface = RenderLayerAPIController::GetSingleton().GetNativeSurface(a_surface);
        if (surface != nullptr)
        {
            surface->ClearNif(NextNifLoadGeneration());
        }
    }

    Meridian::UI::NifView::Status __cdecl NifSceneAPIController::GetStatus(
        Meridian::UI::RenderLayer::SurfaceHandle a_surface) const
    {
        if (m_isShuttingDown.load(std::memory_order_acquire))
        {
            return Meridian::UI::NifView::Status::ShuttingDown;
        }
        const auto surface = RenderLayerAPIController::GetSingleton().GetNativeSurface(a_surface);
        return surface == nullptr ?
            Meridian::UI::NifView::Status::InvalidSurface :
            surface->GetNifStatus();
    }

    bool __cdecl NifSceneAPIController::SetObjectVisible(
        Meridian::UI::RenderLayer::SurfaceHandle a_surface,
        Meridian::UI::NifScene::ObjectHandle a_object,
        bool a_visible)
    {
        if (m_isShuttingDown.load(std::memory_order_acquire) ||
            a_object == Meridian::UI::NifScene::INVALID_OBJECT_HANDLE)
        {
            return false;
        }
        const auto surface = RenderLayerAPIController::GetSingleton().GetNativeSurface(a_surface);
        return surface != nullptr && surface->SetNifObjectVisible(a_object, a_visible);
    }

    bool __cdecl NifSceneAPIController::FrameScene(
        Meridian::UI::RenderLayer::SurfaceHandle a_surface)
    {
        if (m_isShuttingDown.load(std::memory_order_acquire))
        {
            return false;
        }
        const auto surface = RenderLayerAPIController::GetSingleton().GetNativeSurface(a_surface);
        return surface != nullptr && surface->FrameNif();
    }

    void NifSceneAPIController::BeginShutdown()
    {
        m_isShuttingDown.store(true, std::memory_order_release);
    }

    bool NifSceneAPIController::IsShuttingDown() const
    {
        return m_isShuttingDown.load(std::memory_order_acquire);
    }
}
