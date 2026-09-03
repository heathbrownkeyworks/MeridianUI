# Meridian.NifScene/1 Runtime Gate

This gate validates atomic multi-NIF outfit composition on one existing
`Meridian.RenderLayer/1` surface. It uses the standalone `Meridian NIF Test`
consumer and requires no other rendering framework or outfit mod.

## Automated status

| Gate | Status |
|---|---|
| `Meridian.NifScene/1` ABI contract | PASS |
| Bounded object/vertex/index composition | PASS |
| Index and draw-range offset correctness | PASS |
| Stable full-scene bounds | PASS |
| Generation-safe atomic Skyrim task submission | PASS |
| Per-object visibility filtering | PASS |
| Four vanilla archive paths and NIF roundtrips | PASS |
| Complete Release CTest suite | PASS (34/34) |
| Azure signatures and staged/live hashes | PASS (13/13 valid; Meridian 239/239; test 4/4) |

Automated checks do not establish in-game alignment.

## AE 1.6.1170

Four-piece female hide outfit and switching alongside the existing armor and
coin models: **PASS (user-confirmed 2026-08-24)**

1. Enable `MeridianUI` and `Meridian NIF Test`, then load a save.
2. Confirm the default view contains the hide cuirass, helmet, gauntlets, and
   boots in one coherent bind pose with shared camera and lighting.
3. Orbit through front, side, back, high, and low angles. Confirm the pieces do
   not separate, explode, flicker, or z-fight unexpectedly.
4. Toggle each manifest slot. Confirm exactly one object disappears/restores
   and the camera does not shift or zoom.
5. Switch to the single armor and coin presets, then back to HIDE OUTFIT.
   Confirm no stale object returns from an older asynchronous load.
6. Compare Neutral, Bright, and Dramatic and sweep exposure. Confirm all pieces
   update together and retain material detail.
7. Close and reopen the test repeatedly, then quit Skyrim. Confirm no crash,
   stuck cursor/focus, rendering corruption, or surviving CEF helper.
8. Confirm `MeridianUI.log` reports a four-object composition and no archive,
   texture, extraction, or incompatible-dimension errors.

## SE 1.5.97

Status: **NOT RUN**

Repeat the AE matrix on an actual SE 1.5.97 executable with matching SKSE and
Address Library.

## Deferred capability

The version-1 scene displays authored bind-pose meshes in shared model space.
`Meridian.NifScene/2` adds `_0`/`_1` weight interpolation and is gated
separately in `MERIDIAN_NIF_WEIGHT_RUNTIME_GATE.md`. `Meridian.NifScene/3`
adds initial armor-record resolution and is gated separately in
`MERIDIAN_NIF_ARMOR_SCENE_RUNTIME_GATE.md`; its implementation now includes
multi-addon grouping and Armor Addon texture swaps behind the unchanged `/3`
ABI. BodySlide or TRI morphs, custom body/head/hair assembly, bone attachments,
animation, and Havok/SMP remain later phases.

## VR

Status: **DEFERRED / NOT SUPPORTED**
