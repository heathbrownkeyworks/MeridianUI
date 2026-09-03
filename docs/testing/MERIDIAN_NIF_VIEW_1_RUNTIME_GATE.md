# Meridian.NifView/1 Runtime Gate

This gate validates the first self-contained NIF-rendering milestone. Meridian
uses Skyrim's own model/resource loader and a Meridian-owned D3D11 renderer; no
PrismaUI, Mesh Rendering Framework, or other installed framework is required.

## Automated status

| Gate | Status |
|---|---|
| `Meridian.NifView/1` ABI contract | PASS |
| Packed geometry, bounds, normal, and path-validation tests | PASS |
| Packed skin influences and CPU bind-pose tests | PASS |
| Studio-rig, exposure bounds, and output-transfer contracts | PASS |
| Opt-in `MeridianUITest` NIF fixture build | PASS |
| Full Release test suite | PASS (34/34) |
| Azure signature verification | PASS (12 Meridian + 1 test-consumer DLL/EXE artifacts) |
| Release manifest verification | PASS |
| Staged/live deployment hash comparison | PASS (239 files; 0 missing, 0 mismatched) |
| Standalone test-mod deployment comparison | PASS (4 files; 0 missing, 0 mismatched) |
| Horde before/after recursive manifest | PASS (24 files; identical SHA-256 manifest) |

These checks do not replace the in-game gates below.

## Fixture setup

The recommended consumer is now the separate `Meridian NIF Test` MO2 mod. See
`MERIDIAN_NIF_TEST_MOD_GATE.md`. It contains only `MeridianNifTest.dll`, uses a
verified vanilla rigid and skinned meshes, and leaves Horde untouched.

The broader `MeridianUITest` lifecycle fixture remains available for compositor
regression testing. To use that older multipurpose fixture, build Meridian with
`MERIDIAN_BUILD_FIXTURE=ON`, install `MeridianUITest.dll`, and create:

```text
Data/MeridianUI/tests/enable_fixture.txt
Data/MeridianUI/tests/enable_render_layer.txt
Data/MeridianUI/tests/enable_nif_view.txt
Data/MeridianUI/tests/nif_path.txt
```

Put one virtual path relative to `Data\Meshes` on the first line of
`nif_path.txt`, for example `mymod\preview\item.nif`. An optional leading
`meshes\` is accepted. Remove any marker to return the fixture to its dormant
state.

## AE 1.6.1170

Rigid and skinned materials/camera: **PASS (user-confirmed 2026-08-24)**

Studio-lighting controls and output transfer: **PASS (user-confirmed 2026-08-24)**

Multi-NIF outfit composition is gated separately in
`MERIDIAN_NIF_SCENE_1_RUNTIME_GATE.md`.

1. Launch a save and confirm `MeridianUI.log` reports requests for both
   `Meridian.RenderLayer/1` and `Meridian.NifView/1`, followed by a successful
   extraction/upload message.
2. Confirm the mesh replaces the checkerboard, is centered and fully framed,
   has stable neutral lighting, and is clipped to the 640x480 surface.
3. Test a loose NIF and a NIF supplied through an enabled archive. Confirm both
   resolve through the virtual Data path without an absolute filesystem path.
4. Open Horde over/alongside the preview. Confirm z-order remains correct,
   Horde remains interactive, and the Meridian cursor remains above all layers.
5. Resize the game window, reload the same path, and quit to desktop. Confirm no
   corruption, flashing, D3D warning, shutdown crash, or surviving CEF helper.
6. Load `armor\hide\f\cuirasslight_1.nif`. Confirm both skinned shapes render
   together in a coherent bind pose and remain fully framed.
7. Test a nonexistent path and malformed or unsupported NIF. Confirm the
   checkerboard remains visible and the status/log reports failure or
   unsupported geometry.
8. Compare Neutral, Bright, and Dramatic on both the armor and coin. Confirm
   Neutral reveals surface detail, Bright lifts deep shadows, and Dramatic
   preserves readable highlights and silhouette separation.
9. Sweep exposure from `-2.0 EV` through `+2.0 EV`, reset it, and orbit the
   model. Confirm updates are immediate, highlights compress rather than clip,
   and the camera-relative rig does not leave the inspected face unlit.

## SE 1.5.97

Status: **NOT RUN**

Repeat the AE matrix on an actual SE 1.5.97 executable with matching SKSE and
Address Library. A successful multi-runtime compile is not gameplay proof.

## Deferred capability

The v1 milestone now reproduces the supported Skyrim lighting material subset
and renders bounded skin partitions in the loaded skeleton's current bind pose.
It does not assemble multiple actor/equipment NIFs, animate controllers,
evaluate FaceGen/body morphs, or simulate Havok/SMP.

## VR

Status: **DEFERRED / NOT SUPPORTED BY THIS MILESTONE**

VR still requires a dedicated compositor/input backend decision.
