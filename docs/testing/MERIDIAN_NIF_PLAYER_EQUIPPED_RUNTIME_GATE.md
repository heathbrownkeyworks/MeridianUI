# Meridian.NifScene/3 Player-Equipped Outfit Runtime Gate

This gate validates the standalone consumer adapter that discovers a live
player's equipped ARMO records and submits them through the existing armor-scene
API. It does not add or change a Meridian ABI.

## Automated status

| Gate | Status |
|---|---|
| All 32 biped slot bits scanned | PASS |
| Empty slots ignored | PASS |
| Multi-slot ARMO FormIDs deduplicated in stable order | PASS |
| Runtime ARMO FormIDs used as stable nonzero object handles | PASS |
| Excess records rejected without partial publication | PASS |
| Actor read marshalled from CEF callback to Skyrim task | PASS |
| Race, sex, and base weight derived on Skyrim task | PASS |
| Existing `Meridian.NifScene/3` ABI reused unchanged | PASS |
| Hidden startup and left/right `Alt+N` down-edge toggle | PASS |
| Chromium view and native surface shown/hidden as one lifecycle | PASS |

## AE 1.6.1170

Status: **PASS (user-confirmed 2026-08-24)**

The first AE run on 2026-08-24 discovered the four equipped Ebony Spell Knight
pieces correctly, plus TNG body proxy armor `FE06BAFF`. Its intentional
`actors\character\character assets\TNG\f_blank.nif` contains no renderable
geometry and caused the original atomic submission to fail. Meridian now omits
only explicit `blank.nif` and `*_blank.nif` armor models while retaining strict
failure behavior for ordinary missing, malformed, or unsupported armor.

The repaired run discovered five equipped ARMO records, omitted the one TNG
blank proxy, and successfully composed the remaining four Ebony Spell Knight
objects: 10 accepted shapes, 0 skipped shapes, and 114,822 uploaded indices.

1. Save with the player wearing a recognizable multi-piece outfit. A mixture
   of vanilla and modded ARMO records is preferred.
2. Load that save with `MeridianUI`, `Meridian NIF Test`, and
   `MeridianNifTest.esp` enabled.
3. The test must remain completely hidden at the main menu and after loading
   the save. Press `Alt+N` to open it, then select `PLAYER OUTFIT`.
4. Confirm every equipped armor piece appears together with the correct mesh
   and textures. The readout must say `RUNTIME EQUIPPED`; the four fixed hide
   toggles must be disabled.
5. Confirm the initial weight readout changes to the player base weight.
6. Select explicit weights 0, 50, and 100. Confirm the equipped record set stays
   intact and compatible weighted pieces change coherently.
7. Switch through `RECORD OUTFIT`, `MODDED RECORD`, `LAYERED RECORD`,
   `TEXTURE SWAP`, `NIF OUTFIT`, `SKINNED ARMOR`, and `STATIC COIN`, then return
   to `PLAYER OUTFIT`. Confirm no stale scene, checkerboard, or viewport shift.
8. Confirm `MeridianNifTest.log` contains one `queued player-equipped actor`
   entry with actor/race/sex/weight, a nonzero unique record count, and FormIDs.
9. Confirm `MeridianUI.log` resolves every listed ARMO and reports a successful
   composition with no missing record, path, extraction, texture, or topology
   warning.
10. Press Escape or the close button. Confirm both the Chromium controls and
    native preview disappear. Press `Alt+N` and confirm they reopen together.

## Boundary

This preview contains equipped ARMO meshes only. It does not yet reproduce the
actor's naked body/skin, FaceGen head, unequipped hair, NiOverride overrides,
OBody/TRI morph state, animation pose, or SMP/Havok simulation. Those require a
full actor-appearance scene rather than an equipment adapter.

## SE 1.5.97

Status: **NOT RUN**

## VR

Status: **DEFERRED / NOT SUPPORTED**
