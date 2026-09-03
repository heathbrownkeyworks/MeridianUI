#pragma once

#include "NifViewAPI.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace Meridian::UI::NifScene
{
    inline constexpr char EXTENSION_NAME[] = "Meridian.NifScene";
    inline constexpr std::uint32_t INTERFACE_VERSION = 1;
    inline constexpr std::uint32_t WEIGHTED_INTERFACE_VERSION = 2;
    inline constexpr std::uint32_t ARMOR_INTERFACE_VERSION = 3;
    inline constexpr std::uint32_t ACTOR_APPEARANCE_INTERFACE_VERSION = 4;
    inline constexpr std::uint32_t MAX_SCENE_OBJECTS = 32;

    using ObjectHandle = std::uint64_t;
    inline constexpr ObjectHandle INVALID_OBJECT_HANDLE = 0;

    struct SceneObjectLoadInfo
    {
        std::uint32_t structSize = sizeof(SceneObjectLoadInfo);
        ObjectHandle object = INVALID_OBJECT_HANDLE;
        const char* modelPath = nullptr;
        bool visible = true;
        std::uint8_t reserved[7] = {};
    };

    inline constexpr std::uint32_t SCENE_OBJECT_LOAD_INFO_MIN_SIZE_1 =
        static_cast<std::uint32_t>(offsetof(SceneObjectLoadInfo, reserved) +
                                   sizeof(SceneObjectLoadInfo::reserved));
    static_assert(sizeof(SceneObjectLoadInfo) == SCENE_OBJECT_LOAD_INFO_MIN_SIZE_1);

    struct SceneLoadInfo
    {
        std::uint32_t structSize = sizeof(SceneLoadInfo);
        RenderLayer::SurfaceHandle surface = RenderLayer::INVALID_SURFACE_HANDLE;
        const SceneObjectLoadInfo* objects = nullptr;
        std::uint32_t objectCount = 0;
        bool frameOnLoad = true;
        std::uint8_t reserved[11] = {};
    };

    inline constexpr std::uint32_t SCENE_LOAD_INFO_MIN_SIZE_1 =
        static_cast<std::uint32_t>(offsetof(SceneLoadInfo, reserved) +
                                   sizeof(SceneLoadInfo::reserved));
    static_assert(sizeof(SceneLoadInfo) == SCENE_LOAD_INFO_MIN_SIZE_1);

    struct WeightedSceneObjectLoadInfo
    {
        std::uint32_t structSize = sizeof(WeightedSceneObjectLoadInfo);
        ObjectHandle object = INVALID_OBJECT_HANDLE;
        const char* lowModelPath = nullptr;
        const char* highModelPath = nullptr;
        bool visible = true;
        std::uint8_t reserved[7] = {};
    };

    inline constexpr std::uint32_t WEIGHTED_SCENE_OBJECT_LOAD_INFO_MIN_SIZE_2 =
        static_cast<std::uint32_t>(offsetof(WeightedSceneObjectLoadInfo, reserved) +
                                   sizeof(WeightedSceneObjectLoadInfo::reserved));
    static_assert(sizeof(WeightedSceneObjectLoadInfo) ==
                  WEIGHTED_SCENE_OBJECT_LOAD_INFO_MIN_SIZE_2);

    struct WeightedSceneLoadInfo
    {
        std::uint32_t structSize = sizeof(WeightedSceneLoadInfo);
        RenderLayer::SurfaceHandle surface = RenderLayer::INVALID_SURFACE_HANDLE;
        const WeightedSceneObjectLoadInfo* objects = nullptr;
        std::uint32_t objectCount = 0;
        float weight = 100.0f;
        bool frameOnLoad = true;
        std::uint8_t reserved[7] = {};
    };

    inline constexpr std::uint32_t WEIGHTED_SCENE_LOAD_INFO_MIN_SIZE_2 =
        static_cast<std::uint32_t>(offsetof(WeightedSceneLoadInfo, reserved) +
                                   sizeof(WeightedSceneLoadInfo::reserved));
    static_assert(sizeof(WeightedSceneLoadInfo) == WEIGHTED_SCENE_LOAD_INFO_MIN_SIZE_2);

    enum class Sex : std::uint32_t
    {
        Male = 0,
        Female = 1,
    };

    inline bool IsValidSex(Sex a_sex)
    {
        return a_sex == Sex::Male || a_sex == Sex::Female;
    }

    struct ArmorSceneObjectLoadInfo
    {
        std::uint32_t structSize = sizeof(ArmorSceneObjectLoadInfo);
        ObjectHandle object = INVALID_OBJECT_HANDLE;
        std::uint32_t armorFormID = 0;
        bool visible = true;
        std::uint8_t reserved[11] = {};
    };

    inline constexpr std::uint32_t ARMOR_SCENE_OBJECT_LOAD_INFO_MIN_SIZE_3 =
        static_cast<std::uint32_t>(offsetof(ArmorSceneObjectLoadInfo, reserved) +
                                   sizeof(ArmorSceneObjectLoadInfo::reserved));
    static_assert(sizeof(ArmorSceneObjectLoadInfo) ==
                  ARMOR_SCENE_OBJECT_LOAD_INFO_MIN_SIZE_3);

    struct ArmorSceneLoadInfo
    {
        std::uint32_t structSize = sizeof(ArmorSceneLoadInfo);
        RenderLayer::SurfaceHandle surface = RenderLayer::INVALID_SURFACE_HANDLE;
        const ArmorSceneObjectLoadInfo* objects = nullptr;
        std::uint32_t objectCount = 0;
        std::uint32_t raceFormID = 0;
        Sex sex = Sex::Female;
        float weight = 100.0f;
        bool frameOnLoad = true;
        std::uint8_t reserved[7] = {};
    };

    inline constexpr std::uint32_t ARMOR_SCENE_LOAD_INFO_MIN_SIZE_3 =
        static_cast<std::uint32_t>(offsetof(ArmorSceneLoadInfo, reserved) +
                                   sizeof(ArmorSceneLoadInfo::reserved));
    static_assert(sizeof(ArmorSceneLoadInfo) == ARMOR_SCENE_LOAD_INFO_MIN_SIZE_3);

    inline constexpr std::uint32_t ACTOR_APPEARANCE_PART_EQUIPMENT = 1u << 0;
    inline constexpr std::uint32_t ACTOR_APPEARANCE_PART_SKIN = 1u << 1;
    inline constexpr std::uint32_t ACTOR_APPEARANCE_PART_HEAD = 1u << 2;
    inline constexpr std::uint32_t ACTOR_APPEARANCE_PARTS_ALL =
        ACTOR_APPEARANCE_PART_EQUIPMENT |
        ACTOR_APPEARANCE_PART_SKIN |
        ACTOR_APPEARANCE_PART_HEAD;

    inline bool IsValidActorAppearanceParts(std::uint32_t a_parts)
    {
        return a_parts != 0 && (a_parts & ~ACTOR_APPEARANCE_PARTS_ALL) == 0;
    }

    struct ActorAppearanceSceneLoadInfo
    {
        std::uint32_t structSize = sizeof(ActorAppearanceSceneLoadInfo);
        RenderLayer::SurfaceHandle surface = RenderLayer::INVALID_SURFACE_HANDLE;
        std::uint32_t actorFormID = 0;
        std::uint32_t parts = ACTOR_APPEARANCE_PARTS_ALL;
        bool frameOnLoad = true;
        std::uint8_t reserved[7] = {};
    };

    inline constexpr std::uint32_t ACTOR_APPEARANCE_SCENE_LOAD_INFO_MIN_SIZE_4 =
        static_cast<std::uint32_t>(offsetof(ActorAppearanceSceneLoadInfo, reserved) +
                                   sizeof(ActorAppearanceSceneLoadInfo::reserved));
    static_assert(sizeof(ActorAppearanceSceneLoadInfo) ==
                  ACTOR_APPEARANCE_SCENE_LOAD_INFO_MIN_SIZE_4);

    inline bool IsSupported(const char* a_name, std::uint32_t a_version)
    {
        return a_name != nullptr &&
               std::strcmp(a_name, EXTENSION_NAME) == 0 &&
               (a_version == INTERFACE_VERSION ||
                a_version == WEIGHTED_INTERFACE_VERSION ||
                a_version == ARMOR_INTERFACE_VERSION ||
                a_version == ACTOR_APPEARANCE_INTERFACE_VERSION);
    }

    class INifSceneAPI
    {
    public:
        virtual ~INifSceneAPI() = default;

        virtual bool __cdecl ReplaceScene(const SceneLoadInfo* a_info) = 0;
        virtual void __cdecl ClearScene(RenderLayer::SurfaceHandle a_surface) = 0;
        virtual NifView::Status __cdecl GetStatus(
            RenderLayer::SurfaceHandle a_surface) const = 0;
        virtual bool __cdecl SetObjectVisible(RenderLayer::SurfaceHandle a_surface,
                                               ObjectHandle a_object,
                                               bool a_visible) = 0;
        virtual bool __cdecl FrameScene(RenderLayer::SurfaceHandle a_surface) = 0;
    };

    class INifSceneAPI2 : public INifSceneAPI
    {
    public:
        virtual bool __cdecl ReplaceWeightedScene(
            const WeightedSceneLoadInfo* a_info) = 0;
    };

    class INifSceneAPI3 : public INifSceneAPI2
    {
    public:
        virtual bool __cdecl ReplaceArmorScene(
            const ArmorSceneLoadInfo* a_info) = 0;
    };

    class INifSceneAPI4 : public INifSceneAPI3
    {
    public:
        virtual bool __cdecl ReplaceActorAppearanceScene(
            const ActorAppearanceSceneLoadInfo* a_info) = 0;
    };

    using QueryMeridianExtensionFn = bool(__cdecl*)(const char* a_name,
                                                    std::uint32_t a_version,
                                                    void** a_outInterface,
                                                    Meridian::UI::Settings* a_settings,
                                                    const char* a_consumerName);
}
