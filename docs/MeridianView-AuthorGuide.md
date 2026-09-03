# Meridian.View/1 Consumer Guide

`Meridian.View/1` is the compatibility interface for mods migrating from PrismaUI. Meridian UI remains a separately installed runtime; consumer mods ship only their SKSE DLL, their web assets, and the public headers they compiled against.

## Package layout

For a consumer named `horde`, package the page as:

```text
Data/
  MeridianUI/
    horde/
      index.html
```

The corresponding URL is `mod://horde/index.html`. `ViewCreateInfo.ownerName` must match the URL host. Meridian pins native bindings to that initial trusted origin and rejects other schemes or a mismatched owner.

## Acquire and create

Copy `MeridianUIAPI/Settings.h`, `ViewAPI.h`, and `ViewDllLoader.h` into the consumer project. Acquire the interface during SKSE's `kInputLoaded` message. Meridian initializes CEF on its designated application thread and deliberately rejects a first API request from later worker-thread lifecycle messages such as `kDataLoaded`.

Use a two-stage startup: acquire Meridian at `kInputLoaded`, then access forms and create the consumer view at `kDataLoaded`. Keep non-UI systems outside the Meridian availability check so a missing or failed UI provider never disables the rest of the mod.

```cpp
#include "MeridianUIAPI/ViewDllLoader.h"

Meridian::UI::View::IViewAPI* g_views = nullptr;
Meridian::UI::View::ViewHandle g_view = Meridian::UI::View::INVALID_VIEW_HANDLE;

void OnInputLoaded()
{
    Meridian::UI::Settings settings{};
    g_views = Meridian::UI::View::Query(&settings, "MyMod");
}

void OnDataLoaded()
{
    InitializeCoreSystems();
    RegisterGameEventsAndInput();

    if (g_views) {
        Meridian::UI::View::ViewCreateInfo info{};
        info.ownerName = "mymod";
        info.viewName = "main";
        info.startUrl = "mod://mymod/index.html";
        info.initiallyVisible = false;
        info.onDOMReady = [](Meridian::UI::View::ViewHandle) {
            logger::info("MyMod UI ready");
        };

        g_view = g_views->CreateView(&info);
    }
}

void OnSKSEMessage(SKSE::MessagingInterface::Message* message)
{
    switch (message->type) {
    case SKSE::MessagingInterface::kInputLoaded:
        OnInputLoaded();
        break;
    case SKSE::MessagingInterface::kDataLoaded:
        OnDataLoaded();
        break;
    }
}
```

The query is exact: the extension name is `Meridian.View` and the interface version is `1`. The loader uses `GetModuleHandleW`, so it does not load an arbitrary second copy of `MeridianUI.dll`.

## JavaScript compatibility

Registering a listener exposes a function with the same name on `window`, which lets an existing Prisma-style page continue calling its native bridge without a page rewrite:

```cpp
views->RegisterListener(view, "myModSave", [](const char* payload) {
    SKSE::GetTaskInterface()->AddTask([json = std::string(payload)]() {
        // Read or mutate Skyrim state here.
    });
});
```

```js
window.myModSave?.(JSON.stringify({ slot: 2 }));
```

Listener callbacks run on Meridian's CEF callback thread. Marshal every Skyrim operation through SKSE's task interface. The payload pointer is valid only for the duration of the callback, so copy it before capturing it in a task.

Use `ExecuteJavaScript` for native-to-page updates:

```cpp
views->ExecuteJavaScript(view, "myModUpdateState({\"ready\":true})");
```

The compatibility bindings are replayed after each main-frame load, so registered window functions survive reload/navigation back to the trusted origin. Meridian also queues registrations while Chromium is still exposing the native bridge; consumers do not need page-side polling or delayed listener registration.

## Visibility and focus

Showing and focusing are separate operations:

```cpp
views->Show(view);
const auto result = views->TryFocus(
    view, Meridian::UI::View::FocusMode::PauseGame);

if (result != Meridian::UI::View::FocusResult::Granted &&
    result != Meridian::UI::View::FocusResult::AlreadyFocused) {
    views->Hide(view);
}
```

`TryFocus` is non-preemptive. It returns `Busy` when Tailor, Romantasy, Horde, or another Meridian view owns focus; it never steals focus. `NotReady` means the page has not completed its main-frame load. `Unfocus` and `Hide` are idempotent. A hidden view automatically relinquishes its focus.

Use `PauseGame` for menu-style UIs such as Horde and `Unpaused` for overlays that intentionally run over live gameplay.

### Keyboard and text entry

While a Meridian view owns focus, Meridian translates keyboard button events into Chromium key-down, character, and key-up events and consumes them before downstream Skyrim input sinks. Consumers must not hook `BSInputDeviceManager::PollInputDevices` or remove keyboard events from its linked list; doing so prevents Meridian and Chromium from ever receiving the keys.

Meridian automatically detects focus in text-capable `input` elements, `textarea`, and `contenteditable` content. It owns one balanced `ControlMap::AllowTextInput` lease for the focused view and releases it on blur, navigation, focus loss, hide, destruction, or shutdown. Consumer pages do not need native `inputFocused`/`inputBlurred` callbacks.

Escape behavior remains page-specific. Handle it with a normal `keydown` listener and call the consumer's registered close function after any modal or sub-screen priorities have been resolved. This lets Chromium receive Escape first while Meridian still prevents the key from leaking into gameplay.

## Lifetime rules

- Treat `ViewHandle` as opaque and compare it only with `INVALID_VIEW_HANDLE`.
- Call `DestroyView` if the consumer intentionally tears a view down before process exit.
- After Meridian begins shutdown, handles become invalid and new work is rejected.
- Never cache Meridian's internal browser pointer; `Meridian.View/1` deliberately does not expose it.
- This interface supports Skyrim SE and AE through the current Meridian/CommonLibSSE-NG build. VR is not part of the current Meridian runtime contract.

## Native render-layer companion interface

`Meridian.RenderLayer/1` is a separate, exact-query extension for native GPU
content that must be positioned with Chromium views. It does not change the
frozen `Meridian.View/1` vtable and does not expose Direct3D devices, textures,
or shader-resource pointers to consumers.

Copy `RenderLayerAPI.h` and `RenderLayerDllLoader.h` beside the other public
headers, then query the interface during `kInputLoaded`:

```cpp
#include "MeridianUIAPI/RenderLayerDllLoader.h"

Meridian::UI::RenderLayer::IRenderLayerAPI* renderLayers = nullptr;
Meridian::UI::RenderLayer::SurfaceHandle preview =
    Meridian::UI::RenderLayer::INVALID_SURFACE_HANDLE;

void OnInputLoaded()
{
    Meridian::UI::Settings settings{};
    renderLayers = Meridian::UI::RenderLayer::Query(&settings, "MyMod");

    if (renderLayers) {
        Meridian::UI::RenderLayer::SurfaceCreateInfo info{};
        info.ownerName = "MyMod";
        info.surfaceName = "nif-preview";
        info.x = 1120;
        info.y = 180;
        info.width = 640;
        info.height = 720;
        info.zOrder = 10;
        info.initiallyVisible = true;
        preview = renderLayers->CreateSurface(&info);
    }
}
```

Until a content provider is attached, each new surface displays Meridian's
internal checkerboard texture. This makes placement, z-order, visibility, and
resolution behavior observable without exposing any Direct3D object.

Use `SetRect`, `SetZOrder`, and `SetVisible` to update a valid handle. Native
surfaces do not accept keyboard or mouse input; the associated Chromium view
continues to own interaction and focus. `DestroySurface` is idempotent. All
handles become invalid once Meridian begins shutdown.

This interface requires Meridian UI itself but no mesh-rendering framework or
other mod. NIF loading and camera controls live in the separate
`Meridian.NifView/1` extension so the render-layer ABI remains stable.

## Self-contained NIF preview extension

`Meridian.NifView/1` renders a Skyrim static mesh into a surface created by
`Meridian.RenderLayer/1`. Copy `NifViewAPI.h` and `NifViewDllLoader.h`, query it
at `kInputLoaded`, and pass a path relative to `Data\Meshes`:

```cpp
#include "MeridianUIAPI/NifViewDllLoader.h"

auto* nifView = Meridian::UI::NifView::Query(&settings, "MyMod");
if (nifView && preview != Meridian::UI::RenderLayer::INVALID_SURFACE_HANDLE) {
    Meridian::UI::NifView::NifLoadInfo load{};
    load.surface = preview;
    load.modelPath = "mymod\\preview\\item.nif";
    load.frameOnLoad = true;
    nifView->LoadModel(&load);
}
```

The path may optionally begin with `meshes\`. Absolute paths, traversal,
non-NIF extensions, and malformed components are rejected. Meridian passes the
normalized virtual path to Skyrim's own `BSModelDB`, so normal loose-file,
archive, and MO2 resource resolution applies. Loading is queued to Skyrim's
task thread; use `GetStatus` to distinguish `Loading`, `Ready`, `Failed`, and
`Unsupported`.

`SetCamera` changes yaw, pitch, zoom, and pan. Pan X/Y are screen-horizontal
and screen-vertical offsets normalized to the model's bounding radius; pan Z
moves along the camera-to-model axis. `FrameModel` restores the default zoom
and pan. A newer load or `ClearModel` invalidates an older queued result,
and destroying the render surface safely invalidates pending work.

The same size-versioned state optionally selects a camera-relative studio rig
and an exposure adjustment in stops:

```cpp
Meridian::UI::NifView::CameraState camera{};
camera.lightingPreset = Meridian::UI::NifView::LightingPreset::Bright;
camera.exposureStops = 0.4f;  // accepted range: -2.0 to +2.0 EV
nifView->SetCamera(preview, &camera);
```

`Neutral`, `Bright`, and `Dramatic` are stable version-1 enum values. The
original 32-byte camera boundary remains accepted: an older consumer receives
Neutral at 0 EV, while a newer consumer remains safe against a provider that
only understands the original fields.

Version 1 supports rigid `BSTriShape` geometry and bounded Skyrim SE/AE skin
partitions. Skinned vertices are decoded from each partition's four packed
half-float weights and palette indices, mapped through `NiSkinInstance`, and
transformed into the loaded skeleton's current bind pose on the CPU before the
ordinary preview vertex buffer is created. This does not change the public
`Meridian.NifView/1` ABI.

Meridian reads packed UVs and Skyrim's split tangent basis, generates a stable
basis when one is absent, and applies the material's UV transform, opacity,
`NiAlphaProperty`, and two-sided shader flag. Its combined shader supports
diffuse DDS textures, tangent-space normal maps, normal-alpha or dedicated
specular masks, specular color/power/strength, exact alpha testing, and masked
cubemap environment reflections. Its inspection lighting uses camera-relative
key/fill/rim lights, upper/lower hemispheric ambient fill, bounded exposure,
ACES-style highlight compression, and explicit linear-to-sRGB output for the
shared UNORM surface. Model-space normal maps currently fall back to geometric
normals.

Texture bytes are opened through Skyrim's virtual resource system, so loose
files, BSAs, and MO2 override priority apply, but every DDS resource and shader
object is created only on Meridian's private D3D11 device. A missing optional
map disables that material contribution instead of rejecting otherwise valid
geometry.

The current skinning milestone displays one loaded NIF in its bind pose. It
does not assemble a body, armor, head, hair, or equipment from separate NIFs;
apply animation controllers; evaluate FaceGen or body morphs; or simulate
Havok/SMP. Model-space normal-map transforms also remain deferred.
Unsupported-only scene graphs report `Unsupported` and keep the checkerboard
visible. Meridian uses Skyrim's loader and its own D3D11 renderer; no PrismaUI,
Mesh Rendering Framework, or other installed mod is required.

## Atomic multi-NIF scene extension

`Meridian.NifScene/1` composes multiple NIFs in one existing native surface
without changing the `Meridian.NifView/1` vtable. Each object receives a
nonzero caller-assigned 64-bit handle. `ReplaceScene` validates and copies the
complete bounded array, loads all paths through Skyrim on one task, and only
publishes the current generation as one composed scene:

```cpp
#include "MeridianUIAPI/NifSceneDllLoader.h"

auto* scenes = Meridian::UI::NifScene::Query(&settings, "MyMod");
std::array objects{
    Meridian::UI::NifScene::SceneObjectLoadInfo{
        .object = 1,
        .modelPath = "mymod\\outfit\\cuirass_1.nif",
    },
    Meridian::UI::NifScene::SceneObjectLoadInfo{
        .object = 2,
        .modelPath = "mymod\\outfit\\boots_1.nif",
    },
};
Meridian::UI::NifScene::SceneLoadInfo scene{};
scene.surface = preview;
scene.objects = objects.data();
scene.objectCount = static_cast<std::uint32_t>(objects.size());
scene.frameOnLoad = true;
scenes->ReplaceScene(&scene);
```

The version-1 scene cap is 32 objects, 2,000,000 composed vertices, and
6,000,000 composed indices. Handles must be unique and paths follow the same
virtual `Data\Meshes` validation rules as `LoadModel`. `SetObjectVisible`
skips only that object's draw ranges. Full-scene bounds deliberately remain
stable when visibility changes, preventing outfit-slot toggles from shifting
or zooming the inspection camera. `ClearScene`, `GetStatus`, and `FrameScene`
operate on the complete surface scene.

This bind-pose milestone assumes separately authored equipment NIFs share the
same skeleton/model coordinate space.

### Weight-aware scenes

`Meridian.NifScene/2` preserves every version-1 vtable slot and descriptor while
adding `ReplaceWeightedScene`. Each object supplies a required low endpoint and
an optional high endpoint. A null high path marks a fixed piece such as a
helmet:

```cpp
auto* scenes = Meridian::UI::NifScene::Query2(&settings, "MyMod");
std::array objects{
    Meridian::UI::NifScene::WeightedSceneObjectLoadInfo{
        .object = 1,
        .lowModelPath = "mymod\\outfit\\cuirass_0.nif",
        .highModelPath = "mymod\\outfit\\cuirass_1.nif",
    },
    Meridian::UI::NifScene::WeightedSceneObjectLoadInfo{
        .object = 2,
        .lowModelPath = "mymod\\outfit\\helmet.nif",
    },
};
Meridian::UI::NifScene::WeightedSceneLoadInfo scene{};
scene.surface = preview;
scene.objects = objects.data();
scene.objectCount = static_cast<std::uint32_t>(objects.size());
scene.weight = 50.0f;
scenes->ReplaceWeightedScene(&scene);
```

Weight is finite and bounded to Skyrim's `[0, 100]` range. Both endpoints are
loaded in the same generation-safe batch. Meridian requires identical vertex,
index, and draw-range topology, then interpolates bind-pose positions, normals,
and tangent directions and rebuilds scene bounds. Bone palettes and weights
have already been consumed by CPU bind-pose skinning, so they are not compared
or interpolated. Tangent handedness selects the nearest valid endpoint because
it is discrete rather than interpolable. An incompatible pair reports
`Unsupported`; Meridian never publishes a partial outfit. A missing high
endpoint uses the low mesh at every weight.

This phase does not apply TRI/BodySlide morphs, resolve armor records
automatically, or attach equipment to animated bones.

### Armor-record scenes

`Meridian.NifScene/3` preserves all version-1 and version-2 vtable slots and
adds `ReplaceArmorScene`. Consumers provide runtime ARMO form IDs plus the
preview race, sex, and Skyrim weight instead of reproducing Armor Addon model
selection:

```cpp
auto* scenes = Meridian::UI::NifScene::Query3(&settings, "MyMod");
std::array objects{
    Meridian::UI::NifScene::ArmorSceneObjectLoadInfo{
        .object = 1,
        .armorFormID = cuirass->GetFormID(),
    },
    Meridian::UI::NifScene::ArmorSceneObjectLoadInfo{
        .object = 2,
        .armorFormID = boots->GetFormID(),
    },
};
Meridian::UI::NifScene::ArmorSceneLoadInfo scene{};
scene.surface = preview;
scene.objects = objects.data();
scene.objectCount = static_cast<std::uint32_t>(objects.size());
scene.raceFormID = actor->GetRace()->GetFormID();
scene.sex = Meridian::UI::NifScene::Sex::Female;
scene.weight = 50.0f;
scenes->ReplaceArmorScene(&scene);
```

The IDs must be current runtime form IDs, not load-order-independent local
IDs. Meridian copies the bounded descriptors synchronously, then resolves the
race, each `TESObjectARMO`, every race-valid `TESObjectARMA`, and each
requested sex model on Skyrim's task thread. An ARMA weight-slider value of
`2` selects the matching `_0`/`_1` pair; other values load one fixed model.
Every resolved model still passes Meridian's virtual-path validation and the
whole outfit remains generation-safe and atomic.

Compatibility armor records used only to suppress or replace an actor body may
reference an intentional zero-geometry model named `blank.nif` or
`*_blank.nif` (for example TNG's `f_blank.nif`). Meridian omits those explicit
blank proxy parts before atomic composition. Other missing, malformed, or
unsupported armor models still fail the request rather than silently producing
an incomplete outfit.

All mesh parts resolved from one ARMO retain the ARMO's single public object
handle, so visibility and bounds remain record-oriented. The public request cap
remains 32 ARMO objects; Meridian separately bounds expanded internal parts at
256. Meridian also copies each resolved ARMA's bounded `TESModelTextureSwap`
entries into renderer-owned descriptors and applies the referenced TXST paths
by exact 3D shape name, with stable shape-index fallback for differing `_0` and
`_1` endpoint names. No live form pointers leave the Skyrim task. This does not
add TRI/BodySlide morphs, a body/head, animation, or
Havok/SMP. Those limitations can be improved behind the version-3 ABI without
changing consumers.

### Live equipped-outfit adapters

`Meridian.NifScene/3` deliberately accepts armor records rather than an
`RE::Actor*`. A consumer that wants a live actor's currently equipped outfit
must read the actor on Skyrim's task thread, scan the 32 biped slot bits with
`Actor::GetWornArmor`, deduplicate multi-slot armor by runtime FormID, and pass
the resulting bounded ARMO list to `ReplaceArmorScene`. The runtime ARMO FormID
is also a useful stable, nonzero scene-object handle.

Do not inspect actors from a `Meridian.View/1` listener callback: those
callbacks run on Meridian's CEF callback thread. The standalone NIF test's
`PLAYER OUTFIT` adapter demonstrates the required CEF-to-SKSE-task handoff and
uses the actor's race, sex, and base weight for its first request. This adapter
previews equipped ARMO meshes only. It is not yet a full actor clone: naked
body/skin, FaceGen head, unequipped hair, NiOverride, OBody/TRI morph state,
animation, and SMP remain outside this milestone.

### Actor appearance foundations

`Meridian.NifScene/4` preserves every `/1` through `/3` vtable slot and adds a
reference-oriented actor request. The consumer passes a runtime actor-reference
FormID; Meridian performs all actor, equipment, skin, head-part, race, sex, and
weight access later on Skyrim's task thread:

```cpp
auto* scenes = Meridian::UI::NifScene::Query4(&settings, "MyMod");
Meridian::UI::NifScene::ActorAppearanceSceneLoadInfo scene{};
scene.surface = preview;
scene.actorFormID = actor->GetFormID();
scene.parts = Meridian::UI::NifScene::ACTOR_APPEARANCE_PARTS_ALL;
scenes->ReplaceActorAppearanceScene(&scene);
```

`parts` may request equipment, skin, head, or any nonempty combination. The
default requests all three. Do not pass a local plugin FormID or an `RE::Actor*`;
the ID must be the actor reference's current runtime FormID.

Meridian scans the actor's 32 biped slots and deduplicates equipped ARMO
records. Their combined BOD2 mask becomes the coverage mask for the foundation:
an entire skin ARMA is omitted when any of its declared slots are occupied,
preventing body geometry from bleeding through armor. Hair is omitted when the
head, hair, or long-hair slots are occupied. Face, eyes, eyebrows, facial hair,
scar, and miscellaneous head parts are omitted when the head or decapitated-head
slot is occupied. The remaining record models join the equipped armor in one
bounded, generation-safe atomic batch.

Version 4 is a record-based appearance foundation, not a clone of the actor's
live 3D. It currently loads base NIF geometry and model texture swaps. It does
not yet apply FaceGen TRI/RaceMenu morphs and tint layers, NiOverride, OBody or
BodySlide morph state, animation pose, weapon placement, or SMP/Havok
simulation. These can be layered behind later interfaces without exposing game
pointers to Chromium or Meridian's private renderer device.
