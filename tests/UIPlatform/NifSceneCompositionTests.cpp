#include "Render/NifSceneComposition.h"

#include <iostream>
#include <utility>
#include <vector>

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

    Meridian::Render::NifPreview::PreviewMesh Triangle(float a_x)
    {
        using namespace Meridian::Render::NifPreview;
        PreviewMesh mesh;
        mesh.vertices.resize(3);
        mesh.vertices[0].position = {a_x, 0.0f, 0.0f};
        mesh.vertices[1].position = {a_x + 1.0f, 0.0f, 0.0f};
        mesh.vertices[2].position = {a_x, 1.0f, 0.0f};
        mesh.indices = {0, 1, 2};
        mesh.draws.push_back({.startIndex = 0, .indexCount = 3});
        for (const auto& vertex : mesh.vertices)
        {
            mesh.bounds.Include(vertex.position);
        }
        return mesh;
    }
}

int main()
{
    using namespace Meridian::Render::NifPreview;

    std::vector<SceneMeshInput> inputs;
    inputs.push_back({.object = 10, .mesh = Triangle(-2.0f), .visible = true});
    inputs.push_back({.object = 20, .mesh = Triangle(4.0f), .visible = false});
    auto composed = ComposeScene(std::move(inputs));
    Expect(static_cast<bool>(composed), "two valid meshes compose");
    if (composed)
    {
        Expect(composed.mesh.vertices.size() == 6, "vertices are concatenated");
        Expect(composed.mesh.indices == std::vector<std::uint32_t>({0, 1, 2, 3, 4, 5}),
               "later indices are offset by prior vertices");
        Expect(composed.mesh.draws.size() == 2 &&
                   composed.mesh.draws[0].sceneObject == 10 &&
                   composed.mesh.draws[1].sceneObject == 20,
               "draw ranges retain stable scene-object ownership");
        Expect(composed.mesh.draws[1].startIndex == 3,
               "later draw ranges are offset by prior indices");
        Expect(composed.mesh.bounds.minimum.x == -2.0f &&
                   composed.mesh.bounds.maximum.x == 5.0f,
               "full-scene bounds include every object");
        Expect(composed.objects.size() == 2 && !composed.objects[1].visible,
               "object visibility metadata is retained separately from bounds");
    }

    Expect(ComposeScene({}).error == SceneCompositionError::EmptyScene,
           "empty scenes are rejected");
    Expect(ComposeScene({{.object = 0, .mesh = Triangle(0.0f)}}).error ==
               SceneCompositionError::InvalidObject,
           "zero object handles are rejected");
    auto grouped = ComposeScene({{.object = 1, .mesh = Triangle(-4.0f), .visible = true},
                                 {.object = 1, .mesh = Triangle(2.0f), .visible = true}});
    Expect(static_cast<bool>(grouped),
           "multiple private mesh parts may share one public object handle");
    if (grouped)
    {
        Expect(grouped.objects.size() == 1 && grouped.objects[0].object == 1,
               "shared-handle parts publish one public object metadata entry");
        Expect(grouped.objects[0].bounds.minimum.x == -4.0f &&
                   grouped.objects[0].bounds.maximum.x == 3.0f,
               "shared-handle object bounds include every private mesh part");
        Expect(grouped.mesh.draws.size() == 2 &&
                   grouped.mesh.draws[0].sceneObject == 1 &&
                   grouped.mesh.draws[1].sceneObject == 1,
               "every private draw remains owned by the shared public handle");
    }
    Expect(ComposeScene({{.object = 1, .mesh = Triangle(0.0f), .visible = true},
                         {.object = 1, .mesh = Triangle(2.0f), .visible = false}}).error ==
               SceneCompositionError::InconsistentVisibility,
           "private parts sharing a public handle require one visibility state");

    PreviewMesh empty;
    Expect(ComposeScene({{.object = 1, .mesh = std::move(empty)}}).error ==
               SceneCompositionError::EmptyMesh,
           "empty object meshes are rejected");

    auto badIndex = Triangle(0.0f);
    badIndex.indices[2] = 3;
    Expect(ComposeScene({{.object = 1, .mesh = std::move(badIndex)}}).error ==
               SceneCompositionError::InvalidIndex,
           "out-of-range indices are rejected");

    auto badDraw = Triangle(0.0f);
    badDraw.draws[0].indexCount = 4;
    Expect(ComposeScene({{.object = 1, .mesh = std::move(badDraw)}}).error ==
               SceneCompositionError::InvalidDrawRange,
           "out-of-range draw spans are rejected");

    const SceneCompositionLimits oneObject{
        .maxParts = 2, .maxObjects = 1, .maxVertices = 10, .maxIndices = 10};
    Expect(ComposeScene({{.object = 1, .mesh = Triangle(0.0f)},
                         {.object = 2, .mesh = Triangle(2.0f)}}, oneObject).error ==
               SceneCompositionError::TooManyObjects,
           "object caps are enforced");
    const SceneCompositionLimits onePart{
        .maxParts = 1, .maxObjects = 1, .maxVertices = 10, .maxIndices = 10};
    Expect(ComposeScene({{.object = 1, .mesh = Triangle(0.0f)},
                         {.object = 1, .mesh = Triangle(2.0f)}}, onePart).error ==
               SceneCompositionError::TooManyParts,
           "private mesh-part caps are enforced independently of public objects");
    const SceneCompositionLimits twoVertices{
        .maxParts = 2, .maxObjects = 2, .maxVertices = 2, .maxIndices = 10};
    Expect(ComposeScene({{.object = 1, .mesh = Triangle(0.0f)}}, twoVertices).error ==
               SceneCompositionError::TooManyVertices,
           "vertex caps are enforced");
    const SceneCompositionLimits twoIndices{
        .maxParts = 2, .maxObjects = 2, .maxVertices = 10, .maxIndices = 2};
    Expect(ComposeScene({{.object = 1, .mesh = Triangle(0.0f)}}, twoIndices).error ==
               SceneCompositionError::TooManyIndices,
           "index caps are enforced");

    if (g_failures != 0)
    {
        std::cerr << g_failures << " NIF scene composition test(s) failed\n";
        return 1;
    }
    std::cout << "All NIF scene composition tests passed\n";
    return 0;
}
