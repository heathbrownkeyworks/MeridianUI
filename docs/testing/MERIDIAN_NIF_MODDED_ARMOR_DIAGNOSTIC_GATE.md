# Meridian Modded Armor Record Diagnostic Gate

This gate tests a separate BodySlide/modded-armor record path without changing
the proven vanilla `RECORD OUTFIT` fixture. It requires the user's active
`BD Standalone.esp`; Meridian UI itself does not acquire a new dependency.

## Deterministic fixture

| Slot | Source | Plugin-local FormID |
|---|---|---:|
| Cuirass | `BD Standalone.esp` | `000B28` |
| Helmet | `Skyrim.esm` | `00013913` |
| Gauntlets | `BD Standalone.esp` | `000AF3` |
| Boots | `BD Standalone.esp` | `000B51` |

The BD records are resolved through `TESDataHandler::LookupForm` with the plugin
name and local FormID, so their runtime load-order prefix cannot affect the test.

## Automated status

| Gate | Status |
|---|---|
| Separate `MODDED RECORD` web/bridge contract | PASS |
| Plugin-local fixture IDs frozen in tests | PASS |
| Public `Meridian.NifScene/3` ABI unchanged | PASS |
| Atomic all-or-nothing scene publication preserved | PASS |
| Object, endpoint, path, status, and failed-shape diagnostics | PASS |
| Optimized shared-vertex/global-bone partition contract | PASS |
| Full Release build and CTest suite | PASS (35/35) |
| Azure signatures | PASS (13/13 live DLL/EXE files valid) |
| Staged/live Meridian hashes | PASS (239/239 staged files) |
| Built/live standalone test DLL hash | PASS |

## AE 1.6.1170 procedure

Status: **PASS (user-confirmed 2026-08-24)**

The first 2026-08-24 diagnostic run failed only on scene object 104, low endpoint
`BDStandalone\Armor\Hide\F\BootsLight_0.nif`, shape `_BootsLight_1`. The NIF
uses a valid optimized SSE layout: two partition index buffers reference one
561-vertex shape-wide buffer, and packed skin indices address the complete
10-bone skin instance. Meridian incorrectly decoded those partitions as 339-
and 244-vertex local buffers with local bone palettes. The repaired extractor
uses `NiSkinPartition::vertexCount` plus the complete bounded skin-bone array.

The repaired 14:20 AE run composed all four public objects from the BD fixture,
accepted 27 shapes with zero skips, and uploaded 177,612 indices. No
`_BootsLight_1` extraction warning remained. The same session also reconfirmed
the vanilla record outfit, NIF-path outfit, single skinned armor, and static coin.

1. Enable `MeridianUI`, `Meridian NIF Test`, and the existing BD Standalone mod.
2. Load a save and first confirm the default vanilla `RECORD OUTFIT` still
   renders inside the Object Survey viewport.
3. Select `MODDED RECORD` and confirm the full outfit replaces the checkerboard.
4. Select weights 0, 50, and 100 and confirm the boots remain present and
   interpolate with the other weighted pieces.
5. Return to `RECORD OUTFIT`, `NIF OUTFIT`, `SKINNED ARMOR`, and `STATIC COIN`
   to confirm all previous paths still render inside the viewport.
6. Close the test with Escape and quit Skyrim normally.
7. Preserve `%USERPROFILE%\Documents\My Games\Skyrim Special Edition\SKSE\MeridianUI.log`
   and `MeridianNifTest.log` from that run.

The repaired run must contain a successful four-object composition and no
`_BootsLight_1` extraction warning. The generic NIF outfit should no longer
report the two boot-shape skips observed before this repair.

## Compatibility boundaries

- SE 1.5.97: **NOT RUN**
- VR: **DEFERRED / NOT SUPPORTED**
- TRI morph application, animation, and Havok/SMP remain outside this gate.
