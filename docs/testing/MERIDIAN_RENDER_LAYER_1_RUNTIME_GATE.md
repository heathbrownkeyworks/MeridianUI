# Meridian.RenderLayer/1 Checkerboard Runtime Gate

This gate validates the first native-rendering milestone. It proves that a
Meridian-owned D3D11 texture can share the compositor with Chromium views; it
does not yet parse or render a NIF.

## Automated status

| Gate | Status |
|---|---|
| `Meridian.RenderLayer/1` ABI contract test | PASS |
| CPU RGBA checkerboard generator test | PASS |
| Meridian RelWithDebInfo integration build | PASS |
| Full Release build and CTest suite | PASS (21/21) |
| Azure signature verification | PASS (12/12 DLL/EXE artifacts) |
| Staged/live deployment hash comparison | PASS (239 files; 0 missing, 0 mismatched) |

## Fixture setup

Build Meridian with `MERIDIAN_BUILD_FIXTURE=ON`, install the resulting
`MeridianUITest.dll`, and create these empty marker files:

```text
Data/MeridianUI/tests/enable_fixture.txt
Data/MeridianUI/tests/enable_render_layer.txt
```

The fixture creates a visible 640x480 checkerboard at `(200, 200)` with z-order
`1000`. Remove either marker to restore the dormant test configuration.

## AE 1.6.1170

Status: **NOT RUN**

1. Launch a save and confirm `MeridianUI.log` reports
   `Meridian.RenderLayer/1 requested by 'MeridianUITest'` and creation of
   `Meridian.RenderLayer/MeridianUITest/checkerboard`.
2. Confirm the checkerboard is visible, correctly clipped to its rectangle, and
   has no opening flash.
3. Open Horde. Confirm the checkerboard and Horde obey their z-order, Horde
   remains interactive, and the Meridian cursor remains above both layers.
4. Change game resolution or window size. Confirm the checkerboard rectangle
   rescales proportionally and remains stable.
5. Quit to desktop. Confirm no crash, D3D warning, or surviving CEF helper.

## SE 1.5.97

Status: **NOT RUN**

Repeat the AE matrix on an actual SE 1.5.97 executable with its matching SKSE
and Address Library. A successful multi-runtime compile is not gameplay proof.

## VR

Status: **DEFERRED / NOT SUPPORTED BY THIS MILESTONE**

VR requires a dedicated compositor/input backend and remains outside the
current runtime contract.
