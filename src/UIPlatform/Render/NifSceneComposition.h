#pragma once

#include "MeridianUIAPI/NifSceneAPI.h"
#include "Render/NifPreviewMesh.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Meridian::Render::NifPreview
{
    inline constexpr std::size_t MAX_RESOLVED_SCENE_PARTS =
        Meridian::UI::NifScene::MAX_SCENE_OBJECTS * 8;

    struct SceneCompositionLimits
    {
        std::size_t maxParts = MAX_RESOLVED_SCENE_PARTS;
        std::size_t maxObjects = Meridian::UI::NifScene::MAX_SCENE_OBJECTS;
        std::size_t maxVertices = 2'000'000;
        std::size_t maxIndices = 6'000'000;
    };

    struct SceneMeshInput
    {
        Meridian::UI::NifScene::ObjectHandle object =
            Meridian::UI::NifScene::INVALID_OBJECT_HANDLE;
        PreviewMesh mesh;
        bool visible = true;
    };

    struct SceneObjectMetadata
    {
        Meridian::UI::NifScene::ObjectHandle object =
            Meridian::UI::NifScene::INVALID_OBJECT_HANDLE;
        PreviewBounds bounds{};
        bool visible = true;
    };

    enum class SceneCompositionError
    {
        None,
        EmptyScene,
        TooManyParts,
        TooManyObjects,
        InvalidObject,
        DuplicateObject,
        InconsistentVisibility,
        EmptyMesh,
        TooManyVertices,
        TooManyIndices,
        InvalidIndex,
        InvalidDrawRange,
    };

    struct SceneCompositionResult
    {
        SceneCompositionError error = SceneCompositionError::EmptyScene;
        PreviewMesh mesh;
        std::vector<SceneObjectMetadata> objects;

        explicit operator bool() const
        {
            return error == SceneCompositionError::None;
        }
    };

    inline SceneCompositionResult ComposeScene(
        std::vector<SceneMeshInput> a_inputs,
        const SceneCompositionLimits& a_limits = {})
    {
        if (a_inputs.empty())
        {
            return {.error = SceneCompositionError::EmptyScene};
        }
        if (a_inputs.size() > a_limits.maxParts)
        {
            return {.error = SceneCompositionError::TooManyParts};
        }

        std::unordered_map<Meridian::UI::NifScene::ObjectHandle, bool> handles;
        std::size_t totalVertices = 0;
        std::size_t totalIndices = 0;
        std::size_t totalDraws = 0;
        for (const auto& input : a_inputs)
        {
            if (input.object == Meridian::UI::NifScene::INVALID_OBJECT_HANDLE)
            {
                return {.error = SceneCompositionError::InvalidObject};
            }
            const auto [handle, inserted] = handles.try_emplace(input.object, input.visible);
            if (!inserted && handle->second != input.visible)
            {
                return {.error = SceneCompositionError::InconsistentVisibility};
            }
            if (handles.size() > a_limits.maxObjects)
            {
                return {.error = SceneCompositionError::TooManyObjects};
            }
            if (input.mesh.Empty() || input.mesh.draws.empty())
            {
                return {.error = SceneCompositionError::EmptyMesh};
            }
            if (input.mesh.vertices.size() > a_limits.maxVertices -
                    std::min(totalVertices, a_limits.maxVertices))
            {
                return {.error = SceneCompositionError::TooManyVertices};
            }
            totalVertices += input.mesh.vertices.size();
            if (input.mesh.indices.size() > a_limits.maxIndices -
                    std::min(totalIndices, a_limits.maxIndices))
            {
                return {.error = SceneCompositionError::TooManyIndices};
            }
            totalIndices += input.mesh.indices.size();
            totalDraws += input.mesh.draws.size();

            for (const auto index : input.mesh.indices)
            {
                if (index >= input.mesh.vertices.size())
                {
                    return {.error = SceneCompositionError::InvalidIndex};
                }
            }
            for (const auto& draw : input.mesh.draws)
            {
                if (draw.indexCount == 0 || draw.startIndex > input.mesh.indices.size() ||
                    draw.indexCount > input.mesh.indices.size() - draw.startIndex)
                {
                    return {.error = SceneCompositionError::InvalidDrawRange};
                }
            }
        }
        if (totalVertices > std::numeric_limits<std::uint32_t>::max() ||
            totalIndices > std::numeric_limits<std::uint32_t>::max())
        {
            return {.error = SceneCompositionError::TooManyIndices};
        }

        SceneCompositionResult result{.error = SceneCompositionError::None};
        result.mesh.vertices.reserve(totalVertices);
        result.mesh.indices.reserve(totalIndices);
        result.mesh.draws.reserve(totalDraws);
        result.objects.reserve(handles.size());
        std::unordered_map<Meridian::UI::NifScene::ObjectHandle, std::size_t>
            objectMetadata;
        objectMetadata.reserve(handles.size());

        for (auto& input : a_inputs)
        {
            const auto vertexOffset = static_cast<std::uint32_t>(result.mesh.vertices.size());
            const auto indexOffset = static_cast<std::uint32_t>(result.mesh.indices.size());
            result.mesh.vertices.insert(
                result.mesh.vertices.end(),
                std::make_move_iterator(input.mesh.vertices.begin()),
                std::make_move_iterator(input.mesh.vertices.end()));
            for (const auto index : input.mesh.indices)
            {
                result.mesh.indices.push_back(index + vertexOffset);
            }
            for (auto& draw : input.mesh.draws)
            {
                draw.sceneObject = input.object;
                draw.startIndex += indexOffset;
                result.mesh.draws.push_back(std::move(draw));
            }
            result.mesh.bounds.Include(input.mesh.bounds);
            const auto [metadata, inserted] =
                objectMetadata.try_emplace(input.object, result.objects.size());
            if (inserted)
            {
                result.objects.push_back({
                    .object = input.object,
                    .bounds = input.mesh.bounds,
                    .visible = input.visible,
                });
            }
            else
            {
                result.objects[metadata->second].bounds.Include(input.mesh.bounds);
            }
        }
        return result;
    }
}
