# Meridian.NifScene/3 Armor-Record Runtime Gate

This gate validates automatic ARMO-to-ARMA preview resolution without
hardcoded NIF paths in the consumer. It uses only Meridian UI, the standalone
`Meridian NIF Test`, and vanilla Skyrim records/resources.

## Automated status

| Gate | Status |
|---|---|
| Version-1 and version-2 ABI preserved | PASS |
| Version-3 descriptor sizes and inheritance | PASS |
| Runtime FormID, race, sex, weight, and handle validation | PASS |
| ARMA weight-slider and `_0`/`_1` path derivation | PASS |
| Unsafe or malformed Armor Addon paths rejected | PASS |
| Generation-safe atomic record resolution and scene submission | PASS |
| Separate record and NIF-path regression presets | PASS |
| Complete Release CTest suite | PASS (38/38 in both Release configurations) |
| Azure signatures | PASS (13/13 valid across Meridian and test mod) |
| Staged/live hashes | PASS (Meridian 239/239; test 5/5) |
| Layered ESL fixture Spriggit roundtrip | PASS (`000800`, two ARMAs) |
| Grouped-part composition tests | PASS (merged bounds and shared visibility) |
| ARMA alternate-texture copy, matching, and application tests | PASS |
| Alternate-texture ESL fixture Spriggit roundtrip | PASS (`000801`-`000803`) |

## AE 1.6.1170

Four-piece female Nord hide outfit resolved from Skyrim.esm records `00013911`,
`00013913`, `00013912`, and `00013910`: **PASS (user-confirmed 2026-08-24)**

The first attempt used global EditorID lookup and selected plugin-local `B5...`
BD armor records that reused Bethesda EditorIDs. The standalone fixture now uses
the fixed Skyrim.esm FormIDs so load-order duplicates cannot change the requested
records. This does not bypass legitimate overrides of those Skyrim.esm records.

The same session also confirmed that the native preview stayed inside the
Chromium viewport and that the NIF-outfit, single skinned armor, and static coin
regression presets still rendered correctly.

1. Enable `MeridianUI` and `Meridian NIF Test`, then load a save.
2. Confirm the default `RECORD OUTFIT` renders the same four coherent pieces
   as the previously validated hide outfit.
3. Select weights 0, 50, and 100. Confirm the cuirass, gloves, and boots change
   progressively while the helmet remains fixed.
4. Toggle all four manifest entries and confirm each stable ARMO item handle
   controls exactly one piece.
5. Switch between `RECORD OUTFIT` and `NIF OUTFIT`. Confirm their geometry,
   textures, framing, and lighting match.
6. Switch to `SKINNED ARMOR` and `STATIC COIN`, then return to the record
   outfit. Confirm no stale asynchronous scene appears.
7. Change weights rapidly in the order 0, 100, 50 and confirm only the final
   request is displayed.
8. Close/reopen and quit Skyrim. Confirm no crash, stuck input, renderer
   corruption, or surviving CEF helper.
9. Confirm `MeridianUI.log` reports `Meridian.NifScene/3 requested`, four ARMO
   to ARMA resolutions, and a successful four-object composition with no
   record, path, extraction, or topology warning.

## Known limits

This milestone resolves every race-valid Armor Addon with a non-empty base
model for the requested sex. Multiple mesh parts from one ARMO share one public
visibility handle and merged bounds. The deterministic `MeridianNifTest.esp`
fixture references the vanilla hide cuirass and gauntlets ARMAs from one ARMO;
select `LAYERED RECORD` to validate that both render and hide/show together.
ARMA alternate texture swaps now copy bounded TXST descriptors on Skyrim's task
and apply them by 3D name with shape-index fallback; select `TEXTURE SWAP` to
exercise this path. TRI/BodySlide morphs, animation, and Havok/SMP remain
deferred.

AE 1.6.1170 result: **PASS (user-confirmed 2026-08-24)**. Runtime ARMO
`FE4B1800` resolved both `0001B397` and `0001B39B`, then composed one public
object with 70 accepted shapes, zero skipped shapes, and 400,899 indices.

ARMA alternate-texture AE result: **PASS (user-confirmed 2026-08-24)**. Runtime
ARMO `FE4B1803` resolved through ARMA `FE4B1802` with one copied alternate;
both weighted endpoints matched it and composed one object with 66 accepted
shapes, zero skipped shapes, and 363,579 indices. Repeated preset and weight
requests produced the same result without warning or error.

## SE 1.5.97

Status: **NOT RUN**

Repeat the AE matrix on an actual SE 1.5.97 executable with matching SKSE and
Address Library.

## VR

Status: **DEFERRED / NOT SUPPORTED**
