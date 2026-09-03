#include "MeridianUIAPI/NifSceneAPI.h"

#include <iostream>
#include <type_traits>

namespace
{
    int g_failures = 0;

    void Expect(bool a_condition, const char* a_message)
    {
        if (!a_condition)
        {
            ++g_failures;
            std::cerr << "FAILED: " << a_message << '\n';
        }
    }
}

int main()
{
    using namespace Meridian::UI::NifScene;

    static_assert(std::is_standard_layout_v<SceneObjectLoadInfo>);
    static_assert(std::is_standard_layout_v<SceneLoadInfo>);
    static_assert(std::is_standard_layout_v<WeightedSceneObjectLoadInfo>);
    static_assert(std::is_standard_layout_v<WeightedSceneLoadInfo>);
    static_assert(std::is_standard_layout_v<ArmorSceneObjectLoadInfo>);
    static_assert(std::is_standard_layout_v<ArmorSceneLoadInfo>);
    static_assert(std::is_standard_layout_v<ActorAppearanceSceneLoadInfo>);
    static_assert(std::is_same_v<ObjectHandle, std::uint64_t>);
    static_assert(std::is_base_of_v<INifSceneAPI, INifSceneAPI2>);
    static_assert(std::is_base_of_v<INifSceneAPI2, INifSceneAPI3>);
    static_assert(std::is_base_of_v<INifSceneAPI3, INifSceneAPI4>);
    static_assert(sizeof(Sex) == sizeof(std::uint32_t));

    SceneObjectLoadInfo object{};
    Expect(object.structSize == sizeof(SceneObjectLoadInfo),
           "scene object advertises its compiled size");
    Expect(object.object == INVALID_OBJECT_HANDLE, "scene object defaults invalid");
    Expect(object.modelPath == nullptr, "scene object path defaults null");
    Expect(object.visible, "scene object defaults visible");
    Expect(SCENE_OBJECT_LOAD_INFO_MIN_SIZE_1 == sizeof(SceneObjectLoadInfo),
           "scene object version 1 size is frozen");

    SceneLoadInfo scene{};
    Expect(scene.structSize == sizeof(SceneLoadInfo), "scene advertises its compiled size");
    Expect(scene.surface == Meridian::UI::RenderLayer::INVALID_SURFACE_HANDLE,
           "scene defaults to an invalid surface");
    Expect(scene.objects == nullptr && scene.objectCount == 0,
           "scene defaults to an empty object array");
    Expect(scene.frameOnLoad, "scene frames automatically by default");
    Expect(SCENE_LOAD_INFO_MIN_SIZE_1 == sizeof(SceneLoadInfo),
           "scene version 1 size is frozen");
    Expect(MAX_SCENE_OBJECTS == 32, "scene object cap is stable and bounded");

    WeightedSceneObjectLoadInfo weightedObject{};
    Expect(weightedObject.structSize == sizeof(WeightedSceneObjectLoadInfo),
           "weighted scene object advertises its compiled size");
    Expect(weightedObject.object == INVALID_OBJECT_HANDLE,
           "weighted scene object defaults invalid");
    Expect(weightedObject.lowModelPath == nullptr && weightedObject.highModelPath == nullptr,
           "weighted scene object paths default null");
    Expect(weightedObject.visible, "weighted scene object defaults visible");
    Expect(WEIGHTED_SCENE_OBJECT_LOAD_INFO_MIN_SIZE_2 ==
               sizeof(WeightedSceneObjectLoadInfo) &&
               sizeof(WeightedSceneObjectLoadInfo) == 40,
           "weighted scene object version 2 size is frozen");

    WeightedSceneLoadInfo weightedScene{};
    Expect(weightedScene.structSize == sizeof(WeightedSceneLoadInfo),
           "weighted scene advertises its compiled size");
    Expect(weightedScene.surface == Meridian::UI::RenderLayer::INVALID_SURFACE_HANDLE,
           "weighted scene defaults to an invalid surface");
    Expect(weightedScene.objects == nullptr && weightedScene.objectCount == 0,
           "weighted scene defaults to an empty object array");
    Expect(weightedScene.weight == 100.0f,
           "weighted scene defaults to Skyrim weight 100");
    Expect(weightedScene.frameOnLoad, "weighted scene frames automatically by default");
    Expect(WEIGHTED_SCENE_LOAD_INFO_MIN_SIZE_2 == sizeof(WeightedSceneLoadInfo) &&
               sizeof(WeightedSceneLoadInfo) == 40,
           "weighted scene version 2 size is frozen");

    ArmorSceneObjectLoadInfo armorObject{};
    Expect(armorObject.structSize == sizeof(ArmorSceneObjectLoadInfo) &&
               armorObject.object == INVALID_OBJECT_HANDLE &&
               armorObject.armorFormID == 0 && armorObject.visible,
           "armor scene object defaults are stable");
    Expect(ARMOR_SCENE_OBJECT_LOAD_INFO_MIN_SIZE_3 ==
               sizeof(ArmorSceneObjectLoadInfo) &&
               sizeof(ArmorSceneObjectLoadInfo) == 32,
           "armor scene object version 3 size is frozen");

    ArmorSceneLoadInfo armorScene{};
    Expect(armorScene.structSize == sizeof(ArmorSceneLoadInfo) &&
               armorScene.surface == Meridian::UI::RenderLayer::INVALID_SURFACE_HANDLE &&
               armorScene.objects == nullptr && armorScene.objectCount == 0,
           "armor scene defaults to an empty invalid surface");
    Expect(armorScene.raceFormID == 0 && armorScene.sex == Sex::Female &&
               armorScene.weight == 100.0f && armorScene.frameOnLoad,
           "armor scene preview context defaults are stable");
    Expect(ARMOR_SCENE_LOAD_INFO_MIN_SIZE_3 == sizeof(ArmorSceneLoadInfo) &&
               sizeof(ArmorSceneLoadInfo) == 48,
           "armor scene version 3 size is frozen");
    Expect(IsValidSex(Sex::Male) && IsValidSex(Sex::Female) &&
               !IsValidSex(static_cast<Sex>(2)),
           "only Skyrim male and female sex values are accepted");

    ActorAppearanceSceneLoadInfo actorScene{};
    Expect(actorScene.structSize == sizeof(ActorAppearanceSceneLoadInfo) &&
               actorScene.surface == Meridian::UI::RenderLayer::INVALID_SURFACE_HANDLE &&
               actorScene.actorFormID == 0,
           "actor appearance scene defaults to an invalid actor and surface");
    Expect(actorScene.parts == ACTOR_APPEARANCE_PARTS_ALL &&
               actorScene.frameOnLoad,
           "actor appearance scene defaults to all foundation parts and framing");
    Expect(ACTOR_APPEARANCE_SCENE_LOAD_INFO_MIN_SIZE_4 ==
               sizeof(ActorAppearanceSceneLoadInfo) &&
               sizeof(ActorAppearanceSceneLoadInfo) == 32,
           "actor appearance scene version 4 size is frozen");
    Expect(IsValidActorAppearanceParts(ACTOR_APPEARANCE_PART_EQUIPMENT) &&
               IsValidActorAppearanceParts(ACTOR_APPEARANCE_PART_SKIN) &&
               IsValidActorAppearanceParts(ACTOR_APPEARANCE_PART_HEAD) &&
               IsValidActorAppearanceParts(ACTOR_APPEARANCE_PARTS_ALL),
           "each actor appearance foundation part is independently valid");
    Expect(!IsValidActorAppearanceParts(0) &&
               !IsValidActorAppearanceParts(ACTOR_APPEARANCE_PARTS_ALL | 0x80000000u),
           "empty and unknown actor appearance part masks are rejected");

    Expect(IsSupported("Meridian.NifScene", 1), "exact Meridian.NifScene/1 is supported");
    Expect(IsSupported("Meridian.NifScene", 2), "exact Meridian.NifScene/2 is supported");
    Expect(IsSupported("Meridian.NifScene", 3), "exact Meridian.NifScene/3 is supported");
    Expect(IsSupported("Meridian.NifScene", 4), "exact Meridian.NifScene/4 is supported");
    Expect(!IsSupported("Meridian.NifScene", 5), "future scene versions are rejected");
    Expect(!IsSupported("meridian.nifscene", 1), "scene extension name matching is exact");
    Expect(!IsSupported(nullptr, 1), "null scene extension names are rejected");

    return g_failures == 0 ? 0 : 1;
}
