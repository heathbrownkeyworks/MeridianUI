# Meridian DLSS 5 / SkyrimUpscaler Build 14 Runtime Gate

Automated builds prove the selectable presentation order, configuration parser,
and D3D11 code compile. The owner-confirmed AE run below proves Meridian surfaces
are visible through SkyrimUpscaler Build 14's DLSS Neural Reconstruction path;
the wider regression checklist remains separate.

## Test environment

| Component | Required value | Status |
| --- | --- | --- |
| Branch | `DLSS5` candidate built from the recorded commit and dirty-tree diff | Release build and 48/48 CTest **PASS** |
| SkyrimSE.exe | AE 1.6.1170 | **PASS**, owner-confirmed 2026-09-04 |
| SKSE | 2.2.6 | **PASS**, owner-confirmed 2026-09-04 |
| Meridian consumers | Horde, Romantasy, and Tailor | **PASS**, all three created and visible |
| SkyrimUpscaler | Build 14 prerelease, `SkyrimUpscaler.dll` | **PASS** |
| Upscaler mode | DLSS, DLSS Neural Reconstruction enabled | **PASS** |
| Meridian timing | `BeforeRendererEnd` | **PASS** |

Use a dedicated active `Data\SKSE\Plugins\MeridianUI.ini` for the compatibility run:

```ini
[Compatibility]
CompositorTiming=BeforeRendererEnd

[Debug]
LogLevel=debug
```

Do not rename or edit SkyrimUpscaler's prerelease binaries for this gate. Preserve
the exact tested DLL hashes, `SkyrimUpscaler.ini`, `MeridianUI.ini`, full SKSE log,
`MeridianUI.log`, and SkyrimUpscaler log with the result.

## Iteration 1 evidence: proxy swap-chain device mismatch

The first `BeforeRendererEnd` candidate was exercised on AE 1.6.1170 on
2026-09-03. The timing override loaded correctly, but Meridian acquired
SkyrimUpscaler's 2560x1440 proxy-swap-chain backbuffer. The log proved that the
proxy backbuffer and Meridian's game render device were different D3D11 devices:

```text
PresentHook: ... compositor timing=BeforeRendererEnd
RenderHost: compositor target ... dimensions=2560x1440 ... sameDevice=false
RenderHost: refusing backbuffer from a different D3D11 device
```

That run did not render a Meridian surface. The second candidate therefore uses
the render-target view already bound to Skyrim's own immediate context in the
`BeforeRendererEnd` path; only the default `AfterRendererEnd` path acquires a
swap-chain backbuffer.

## Successful candidate evidence

The working candidate normalizes the REX renderer interface through the immediate
context's native D3D11 device and compares canonical COM identities. The
owner-confirmed 2026-09-04 run created Horde, Romantasy, and Tailor views and logged:

```text
RenderHost: normalized render device ... sameIdentity=true
RenderHost: compositor target source=BoundGameRenderTarget ... dimensions=3840x2160 ... sameDevice=true
```

No device refusal followed. All three Meridian interfaces appeared in game, and CEF
shut down successfully with three active browsers drained.

## Baseline control

1. Run the same Meridian candidate with SkyrimUpscaler disabled and no active
   `CompositorTiming` override. Confirm the Present hook logs
   `compositor timing=AfterRendererEnd`.
2. Open, focus, type into, close, and reopen the selected Meridian consumer.
3. Confirm the surface, Meridian cursor, vanilla cursor, and console behave as they
   did before this branch.

Status: **NOT RUN**.

## DLSS 5 compatibility run

1. Enable SkyrimUpscaler Build 14 with DLSS Neural Reconstruction and activate the
   `BeforeRendererEnd` INI above.
2. Confirm `MeridianUI.log` reports `compositor timing=BeforeRendererEnd` and no
   hook-install refusal.
3. Open the consumer and capture the first `RenderHost: compositor target` line.
   It must report `source=BoundGameRenderTarget`; record the owner/texture
   identities, dimensions, format, and require `sameDevice=true`.
4. Confirm the full Meridian surface is visible, positioned correctly, and remains
   stable for at least five minutes in gameplay and menu transitions.
5. Exercise hover, click, typing, selection, paste, Escape, the consumer toggle key,
   and cursor ownership. Confirm input is delivered once and only one cursor is
   visible.
6. Exercise alt-tab, console, pause menu, loading screen, fullscreen/windowed
   transition, and a resolution change. Confirm any target-change log matches the
   visible transition and rendering resumes.
7. Close and reopen the consumer ten times, then perform one browser destruction and
   recreation cycle.
8. Exit Skyrim once with no browser and once with an active browser. Confirm no crash
   and no surviving Meridian CEF helper process.
9. Inspect image quality separately: record whether DLSS NR reconstructs, blurs,
   ghosts, or otherwise alters Meridian. Visibility/usability is the compatibility
   gate; pixel-perfect post-NR composition would require a distinct final-output
   integration if the upscaler provides no UI mask or callback.

Status: **VISIBILITY PASS** on AE 1.6.1170 with Build 14 DLSS NR. Extended stress
steps 4-9 are not all independently recorded.

## Regression matrix

| Runtime / path | Required result | Status |
| --- | --- | --- |
| AE 1.6.1170, no upscaler, `AfterRendererEnd` | Existing Meridian behavior preserved | **NOT RUN** |
| AE 1.6.1170, Build 14 DLSS NR, `BeforeRendererEnd` | Meridian visible and usable | **PASS**, owner-confirmed 2026-09-04 |
| SE 1.5.97, no upscaler, `AfterRendererEnd` | Separate compatibility regression | **NOT RUN** |
| SE 1.5.97, Build 14 DLSS NR | Only after AE compatibility passes | **NOT RUN** |

VR remains outside this backend's supported scope.
