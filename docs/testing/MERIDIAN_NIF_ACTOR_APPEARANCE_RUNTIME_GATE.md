# Meridian.NifScene/4 Loaded Actor Snapshot Runtime Gate

This gate validates the Tailor-oriented actor appearance foundation. It is a
separate `PLAYER ACTOR` test and does not change the proven `PLAYER OUTFIT`
adapter or Horde.

## Automated status

| Gate | Status |
|---|---|
| `/1`, `/2`, and `/3` ABI inheritance preserved | PASS |
| `/4` fixed-size actor request and exact query dispatch | PASS |
| Actor reference resolved only on Skyrim's task thread | PASS |
| Loaded third-person actor root required | PASS |
| 32 armor biped slots scanned; weapon slots excluded | PASS |
| Runtime equipment and skin `partClone` roots deduplicated | PASS |
| Runtime skinned face root captured | PASS |
| Equipment, skin, and face roots submitted atomically | PASS |
| Player and persistent loaded-NPC FormIDs accepted | PASS |
| Actor-root-relative evaluated world transforms | PASS |
| App-culled runtime geometry excluded | PASS |
| `BSDynamicTriShape` external-position stream decoding | PASS |
| External bind-bone fallback and singular-transform rejection | PASS |
| Runtime FaceGen tint/detail texture capture with TXST fallback | PASS |
| FaceGen RGB overlay blend and fixed detail constants | PASS |
| Tangent-space and model-space normal branches | PASS |
| Record/NIF material path remains isolated | PASS |
| Independent `PLAYER ACTOR` control and bridge listener | PASS |
| Existing `PLAYER OUTFIT` path retained | PASS |
| Complete Release CTest suites | PASS (42/42 in both configurations) |
| Azure/Authenticode verification | PASS (12 Meridian + 1 test binary) |
| Staged/live payload comparison | PASS (Meridian 239 staged paths; test 5/5) |

## AE 1.6.1170

Status: **LOADED ACTOR SNAPSHOT READY / RETEST NOT RUN**

The first AE actor-foundation run passed with the complete Ebony Spell Knight
set, then failed after the helmet was removed. The equipment refresh itself was
correct: Meridian resolved four equipped records and exposed eight head parts.
The atomic scene was rejected when
`Actors\Character\Character Assets\Mouth\MouthHuman.nif` returned extraction
status `1` (`NoSupportedGeometry`) with one skipped shape. Direct inspection of
the vanilla BSA asset confirmed that it is a skinned `BSDynamicTriShape` with
141 dynamic vertices, one in-tree head bone, and one skin partition. Its XYZ
positions and tangent-X component are stored in a separate 16-byte `Vector4`
stream, while the remaining attributes and indices are stored in the skin
partition stream. Meridian previously required all attributes to share the
partition stream.

The repaired extractor locks and copies the bounded dynamic position stream,
then pairs it with the existing partition attributes before bind-pose skinning.
It also safely reconstructs a bind transform for other head-part variants that
reference an external bone. Singular transforms and malformed or truncated
buffers still fail. Successful and failed extraction logs now report both the
dynamic-shape and reconstructed-external-bone counts.

The AE rerun passed the geometry gate: equipment changes refreshed, uncovered
hands, feet, and body sections rendered, all eight requested head-part dynamic
shapes were accepted, and the checkerboard did not return. The remaining
visible defect was material state. The standalone record NIFs retained their
default FaceGen RGB-tint and HairTint colors, producing a black face and an
incorrect pale hair result. Skyrim normally replaces those values after the
actor 3D is assembled.

The former record reconstruction has now been removed from the actor endpoint.
Meridian acquires the loaded actor's third-person root, selected biped clones,
and skinned face node on Skyrim's task thread. The render thread copies the
evaluated assembly into one actor-local CPU mesh, including runtime material
descriptors, then releases all Skyrim scene references. No live actor, node,
shape, bone, or shader pointer remains in persistent renderer state. The same
path accepts any persistent loaded actor FormID; the player is only this test
fixture.

FaceGen materials now retain tint/detail texture paths when named, fall back to
their texture-set slots, use the Skyrim overlay blend, and distinguish
model-space normals from ordinary tangent-space normal maps. Ordinary record,
outfit, and arbitrary-NIF previews retain their existing isolated loading path.

1. Save with the player wearing the known Ebony Spell Knight outfit used for
   the successful `PLAYER OUTFIT` gate.
2. Load that save with `MeridianUI`, `Meridian NIF Test`, and
   `MeridianNifTest.esp` enabled. Press `Alt+N` and select `PLAYER ACTOR`.
3. Confirm the equipped outfit renders in the native viewport and the manifest
   says `LIVE ACTOR FOUNDATION`. The fixed piece and weight controls must be
   disabled because the live actor owns both states.
4. Return to Skyrim, unequip only the helmet, reopen the test, and select
   `PLAYER ACTOR` again. Confirm the face has the same skin/tint family as the
   in-world actor and that face, eyes, mouth, eyebrows, and hair remain one
   aligned assembly.
5. Repeat after unequipping the cuirass, gloves, and boots one category at a
   time. Confirm only uncovered skin foundation sections appear; covered body
   sections must not bleed through the remaining armor.
6. Test a completely unequipped actor. Rotate through a full 360 degrees.
   Confirm body, neck, face, eyes, mouth, eyebrows, and hair remain aligned at
   every angle, with no backwards head or angular hair offset.
7. Return to the fully equipped save and select `PLAYER OUTFIT`. Confirm the
   prior equipment-only preview still renders identically, with no stale actor
   foundation geometry, checkerboard, or viewport shift.
8. Switch through `RECORD OUTFIT`, `MODDED RECORD`, `LAYERED RECORD`,
   `TEXTURE SWAP`, `NIF OUTFIT`, `SKINNED ARMOR`, and `STATIC COIN`. Confirm all
   earlier presets remain functional.
9. Confirm `MeridianNifTest.log` contains `queued live player actor appearance
   00000014`.
10. Confirm `MeridianUI.log` contains:
    - `Meridian.NifScene/4 requested by 'MeridianNifTest'`
    - `queued actor appearance 00000014`
    - one `loaded actor 00000014 snapshot selected` summary with
      equipment/skin/face root counts
    - one `live actor 00000014 accepted ... live shapes` summary with FaceGen,
      RGB tint, hair tint, model-space-normal, and incomplete-texture counts
    - one successful composed-scene upload with no failed shape, texture,
      topology, or stale-generation warning
    - a nonzero dynamic-shape count when the selected live assembly contains
      dynamic FaceGen geometry

## Current boundary

The snapshot is a still image of a loaded actor's currently evaluated Skyrim
geometry and material state. It does not keep a live animation link, and this
phase intentionally excludes weapons, first-person geometry, SMP/Havok motion,
and unloaded actors. GPU-only runtime textures without a recoverable source
name are diagnosed as incomplete and continue through their texture-set
fallback. Tailor's later integration should pass a loaded NPC reference FormID
to this same endpoint.

## SE 1.5.97

Status: **NOT RUN**

## VR

Status: **DEFERRED / NOT SUPPORTED**
