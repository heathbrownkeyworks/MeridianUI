# Meridian NIF Test Standalone Mod Gate

`Meridian NIF Test` is a dedicated SKSE consumer for testing
`Meridian.NifView/1` and `Meridian.NifScene/1` through `/4`. It does not use, patch,
replace, or require Horde.

## Installed layout

```text
<MO2 mods>\Meridian NIF Test\
├── MeridianNifTest.esp
├── SKSE\Plugins\MeridianNifTest.dll
└── MeridianUI\meridianniftest\
    ├── index.html
    ├── styles.css
    └── camera-controls.js
```

The test mod contains one ESL-flagged, test-only ESP with two ARMO records, one
ARMA, one TXST, and no redistributed mesh or texture. The layered record
references two vanilla race-valid ARMAs so
the `LAYERED RECORD` preset can prove grouped add-on composition without an
external mod dependency. The second record uses a local female ARMA plus a
coin-texture TXST so `TEXTURE SWAP` can prove alternate-texture propagation
using only vanilla resources. The test defaults to an
atomic four-piece female hide outfit scene containing the verified cuirass,
helmet, gauntlets, and boots paths from `Skyrim - Meshes0.bsa`. The cuirass,
gauntlets, and boots use paired `_0`/`_1` paths with a 0/50/100 calibration
rail; the helmet remains fixed. Single skinned
`armor\hide\f\cuirasslight_1.nif` and rigid `clutter\coin01.nif` presets remain
available. The selector also accepts a validated virtual path relative to
`Data\Meshes`.

The armor-record preset uses fixed Skyrim.esm FormIDs rather than globally
ambiguous EditorIDs. The native preview surface remains hidden at startup until
the Chromium page's measured viewport rectangle has crossed the Meridian.View
bridge, preventing the fallback rectangle from appearing outside the controls.

## Automated evidence

| Gate | Status |
|---|---|
| Release consumer build | PASS |
| Complete Release CTest suite | PASS (39/39 in both Release configurations) |
| Azure Artifact Signing | PASS |
| Authenticode status | PASS (`Valid`, 13 deployed DLL/EXE files) |
| Required SKSE exports | PASS |
| Staged/deployed Meridian payload hashes | PASS (239/239 staged paths matched; 5 pre-existing live-only development files preserved) |
| Staged/deployed test-mod hashes | PASS (5/5 files) |
| Deployed test-mod file count | PASS (5) |
| Fixture Spriggit roundtrip/check | PASS (ESL; layered ARMO `000800`; TXST/ARMA/ARMO `000801`-`000803`; alternate retained) |
| Repeated-handle composition, merged bounds, and visibility tests | PASS |
| Alternate-texture descriptor, name precedence, index fallback, and TXST flag tests | PASS |
| Equipped-manifest 32-slot scan, deduplication, order, handle, and cap tests | PASS |
| CEF-callback to Skyrim-task actor-equipment contract | PASS |
| Hidden startup, `Alt+N` edge-trigger, and paired view/surface lifecycle contract | PASS |
| Horde recursive manifest before/after deployment | PASS (24 files, identical SHA-256 manifest) |
| Vanilla archive path | PASS (`meshes\clutter\coin01.nif`) |
| Extracted NIF integrity | PASS (6,480 bytes; byte-perfect `nif-tool verify`) |
| MVP geometry suitability | PASS (`BSTriShape` present; no skin-instance block) |
| Diffuse path inspection | PASS (`textures\clutter\Coin01.dds`) |
| Vanilla diffuse archive resource | PASS (349,656-byte DDS extracted for inspection) |
| Normal-map path/resource inspection | PASS (`textures\clutter\Coin01_n.dds`; 174,896-byte DDS) |
| Cubemap path/resource inspection | PASS (`textures\cubemaps\Bronze_e.dds`; 4,304-byte cube DDS) |
| Environment-mask path/resource inspection | PASS (`textures\Gray.dds`; 816-byte DDS) |
| Skyrim packed tangent-basis decoding | PASS |
| Build-compiled combined material shader | PASS |
| UV/material path tests | PASS |
| Private-device DDS architecture test | PASS |
| Skinned vanilla archive path | PASS (`meshes\armor\hide\f\cuirasslight_1.nif`) |
| Skinned NIF integrity | PASS (316,058 bytes; byte-perfect `nif-tool verify`) |
| Skinned structure inspection | PASS (2 shapes; 4 partitions; 27/22 skin bones) |
| Packed influence and bind-pose tests | PASS |
| Studio-rig and exposure-bound tests | PASS |
| Tone-map and linear-to-sRGB shader contract | PASS |
| `Meridian.NifScene/1` ABI contract | PASS |
| `Meridian.NifScene/2` weighted ABI contract | PASS |
| `Meridian.NifScene/4` actor appearance ABI and slot-policy contract | PASS |
| Multi-mesh index/draw/bounds composition tests | PASS |
| Topology-safe endpoint/midpoint weight tests | PASS |
| Hide outfit archive paths | PASS (4/4 verified in `Skyrim - Meshes0.bsa`) |
| Hide outfit NIF integrity | PASS (4/4 byte-perfect `nif-tool verify`) |
| Hide outfit skin flags | PASS (4/4 `Skinned`) |
| Hide outfit weighted archive fixture | PASS (7/7 paths and byte-perfect NIF roundtrips) |

Release DLL SHA-256:
`97A560614CAD5602CE74B34D32A936737D908EB8892477C504D179AD68FB839C`

Deployed Meridian renderer DLL SHA-256:
`03C146D8D50F808D7E739981F0A40483C87139569DE890845CD9823B4622727A`

Fixture ESP SHA-256:
`06DC342BA05264D75E63A598317B041FC9CC10E5C2966053CE1E9B3103214D72`

## Running the test in MO2

1. Refresh MO2 or restart it so `Meridian NIF Test` appears in the left pane.
2. Keep the production `MeridianUI` mod enabled.
3. Enable `Meridian NIF Test`, then enable its ESL-flagged
   `MeridianNifTest.esp` in the right pane.
4. Launch Skyrim through MO2 and load a save.
5. Confirm no test UI or checkerboard obstructs the main menu or loaded game.
   Press `Alt+N`, then confirm the transparent Object Survey control overlay
   opens and focuses.
6. Confirm the female vanilla hide cuirass, helmet, gauntlets, and boots replace
   the checkerboard as one fully framed outfit. All pieces must share one
   coherent bind pose rather than appearing at separate skeleton origins.
7. Toggle CUIRASS, HELMET, GLOVES, and BOOTS off and on individually. Confirm
   only the selected piece changes, the `4 / 4 VISIBLE` readout follows, and
   the camera framing does not move.
8. Use `SKINNED ARMOR` and `STATIC COIN` to confirm both previous single-model
   regressions still render, then return to `HIDE OUTFIT`.
9. Select `LAYERED RECORD`. Confirm one record shows both the hide cuirass and
   hide gauntlets. Toggle CUIRASS off and on; both internal parts must hide and
   reappear together while the readout reports `0 / 1` and `1 / 1`.
10. Select `TEXTURE SWAP` at weights `0`, `50`, and `100`. Confirm one hide
    cuirass appears with the unmistakable gold coin diffuse instead of its
    normal hide texture, and remains correctly weighted and framed.
11. Before loading the save, equip the player with a recognizable mixed outfit.
    Select `PLAYER OUTFIT` and confirm all equipped ARMO pieces render together,
    including any modded pieces, with `RUNTIME EQUIPPED` shown in the manifest.
    The four fixed hide-piece toggles should be disabled in this mode.
12. Confirm the first `PLAYER OUTFIT` request adopts the player base weight,
    then select explicit weights `0`, `50`, and `100`. Geometry should change
    coherently without losing records, textures, framing, or the dynamic mode.
13. Select `PLAYER ACTOR` and execute
    `MERIDIAN_NIF_ACTOR_APPEARANCE_RUNTIME_GATE.md`. Confirm the separate live
    actor foundation mode does not change the `PLAYER OUTFIT` result.
14. Select body weights `0`, `50`, and `100`. Confirm the cuirass, gloves, and
   boots change shape coherently, the helmet remains fixed, hidden pieces stay
   hidden, and camera orientation/zoom do not reset between weights.
15. Select `NEUTRAL`, `BRIGHT`, and `DRAMATIC` while viewing the outfit. Confirm
   Neutral reveals the hide and body detail, Bright opens the darkest areas,
   and Dramatic increases contrast without losing the silhouette.
16. Move the exposure slider from `-2.0 EV` through `+2.0 EV`, then press
   `RESET`. Confirm the readout, highlight compression, and neutral default all
   update immediately without reloading the model.
17. Rotate and tilt the outfit and single-model presets. Confirm the studio rig
    remains camera-relative and material detail does not unexpectedly fall into
    darkness while orbiting.
18. Hold the on-screen orbit controls and the `A`/`D` or left/right arrow keys;
   rotation must remain smooth and stop immediately on release.
19. Test tilt (`W`/`S`), zoom (`Q`/`E`), shifted pan, and `F` to reframe.
20. Confirm typing `A`, `D`, `W`, or `S` in the path field does not move the
    camera. Alt-tab while holding a control and confirm movement does not stick.
21. Press Escape and confirm the overlay, cursor focus, and native surface all
    close together so the Skyrim menu becomes interactive again. Press `Alt+N`
    and confirm both visual layers reopen together.
22. Check `MeridianNifTest.log` for:
   - `standalone test loaded; Horde is not used or modified`
   - `created standalone NIF test surface`
   - `queued four-piece female hide outfit`
   - `queued one public object from alternate-texture ARMA armor`
   - `queued player-equipped actor` with its race, sex, weight, unique record
     count, and FormID list
   - `queued live player actor appearance 00000014`
23. Check `MeridianUI.log` for:
   - `Meridian.RenderLayer/1 requested by 'MeridianNifTest'`
   - `Meridian.NifView/1 requested by 'MeridianNifTest'`
   - `Meridian.NifScene/4 requested by 'MeridianNifTest'`
   - a successful four-object composition/upload message
   - `with 1 alternate textures`
   - `matched 2 texture overrides` for the weighted texture-swap fixture
   - no `texture resource`, `DDS decode failed`, or incompatible-dimension
     warning for the selected model's texture paths
24. Quit Skyrim and disable `Meridian NIF Test` in MO2. This removes the entire
   consumer test without changing Horde or production Meridian files.

## Runtime status

- AE 1.6.1170 static NIF render with ENB: **PASS (user-confirmed 2026-08-23)**
- AE 1.6.1170 camera interaction overlay: **PASS (user-confirmed 2026-08-24)**
- AE 1.6.1170 diffuse-textured static material: **PASS (user-confirmed 2026-08-24)**
- AE 1.6.1170 normal/specular/environment material: **PASS (user-confirmed 2026-08-24)**
- AE 1.6.1170 skinned bind-pose material: **PASS (user-confirmed 2026-08-24)**
- AE 1.6.1170 studio-lighting controls and output transfer: **PASS (user-confirmed 2026-08-24)**
- AE 1.6.1170 four-piece hide outfit scene and single-model preset switching:
  **PASS (user-confirmed 2026-08-24)**
- AE 1.6.1170 `_0`/`_1` body-weight interpolation:
  **PASS (user-confirmed 2026-08-24)**
- AE 1.6.1170 deterministic Skyrim.esm armor-record outfit and viewport
  placement repair: **PASS (user-confirmed 2026-08-24)**
- AE 1.6.1170 deterministic `BD Standalone.esp` armor-record diagnostic:
  **PASS (user-confirmed 2026-08-24; 27 shapes, 0 skipped)**
- AE 1.6.1170 one-ARMO/two-ARMA grouped composition and shared visibility:
  **PASS (user-confirmed 2026-08-24; 1 object, 70 shapes, 0 skipped)**
- AE 1.6.1170 ARMA alternate-texture propagation at weights 0/50/100:
  **PASS (user-confirmed 2026-08-24; 1 copied alternate, 2 endpoint matches,
  66 shapes, 0 skipped)**
- AE 1.6.1170 player-equipped ARMO discovery and dynamic outfit preview:
  **PASS (user-confirmed 2026-08-24; five equipped records discovered, TNG
  `FE06BAFF` blank proxy omitted, four Ebony Spell Knight objects composed,
  10 shapes accepted, 0 skipped, 114,822 indices uploaded)**
- AE 1.6.1170 actor appearance foundation (equipment + uncovered skin +
  record-based head parts): **NOT RUN**
- SE 1.5.97: **NOT RUN**
- VR: **DEFERRED / NOT SUPPORTED**

The automated build and signing proof does not count as gameplay validation.
