# Meridian.NifScene/2 Weight Runtime Gate

This gate validates topology-safe Skyrim `_0`/`_1` body-weight interpolation
for an atomic multi-NIF outfit. It uses only MeridianUI, the standalone
`Meridian NIF Test`, and vanilla resources.

## Automated status

| Gate | Status |
|---|---|
| Version-1 ABI and vtable preserved | PASS |
| Version-2 descriptor sizes and inheritance | PASS |
| Weight validation `[0, 100]` | PASS |
| Endpoint and midpoint position interpolation | PASS |
| Normal and tangent normalization | PASS |
| Bounds reconstruction | PASS |
| Index and draw topology mismatch rejection | PASS |
| Post-skinning palette and tangent-endpoint compatibility | PASS |
| Atomic paired-endpoint loading and generation cancellation | PASS |
| Seven vanilla archive paths | PASS |
| Seven byte-perfect `nif-tool verify` roundtrips | PASS |
| Complete Release CTest suite | PASS (34/34) |
| Azure signatures and staged/live hashes | PASS (13/13 valid; Meridian 239/239; test 4/4) |

The extracted inspection fixture was kept in a temporary directory outside the
repository. No vanilla mesh is redistributed by the test mod.

## AE 1.6.1170

The first AE test exposed an over-strict post-skinning palette/handedness check
and displayed the checkerboard fallback. That validator has been repaired while
retaining vertex, index, and draw topology enforcement.

Female hide outfit at weights 0, 50, and 100 after repair:
**PASS (user-confirmed 2026-08-24)**

1. Enable `MeridianUI` and `Meridian NIF Test`, then load a save.
2. Confirm the default `100 / HEAVY` outfit renders coherently.
3. Orbit to a three-quarter view where the torso, gloves, and boots silhouettes
   are easy to compare.
4. Select `0 / LEAN`, `50 / MID`, and `100 / HEAVY`. Confirm the three paired
   pieces change progressively and the unpaired helmet remains fixed.
5. Confirm weight 0 and 100 are visibly different and weight 50 lies between
   them without exploding vertices, seams, inverted faces, or texture shifts.
6. Hide one piece, change weight, and confirm it remains hidden.
7. Pan, zoom, and orbit, then change weight. Confirm the camera does not reset.
8. Switch to `SKINNED ARMOR` and `STATIC COIN`, then return to `HIDE OUTFIT`.
   Confirm all existing models still render and returning to the outfit frames it.
9. Change weights rapidly in the order 0, 100, 50. Confirm only the final scene
   appears and no stale endpoint load replaces it later.
10. Close/reopen repeatedly and quit Skyrim. Confirm no crash, stuck focus,
    corrupt renderer, or surviving CEF helper.
11. Confirm `MeridianUI.log` contains `Meridian.NifScene/2 requested`, successful
    four-object composition lines, and no weight-topology or extraction warning.

## SE 1.5.97

Status: **NOT RUN**

Repeat the AE matrix on an actual SE 1.5.97 executable with matching SKSE and
Address Library.

## VR

Status: **DEFERRED / NOT SUPPORTED**
