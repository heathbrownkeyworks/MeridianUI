# Meridian controller support status

Updated: 2026-09-10. **Meridian UI 1.5.0 is signed and deployed; ready for mod-specific integration.**

Branch: controllersupport. Implementation commits: 00f2e51 and 10dcfa5d32cdfcb1c746bed8bf0d71d41c390652.
Baseline: Meridian UI 1.4.0, a169ef4f2e6c0f38a5f1af98eccfff8c8e6aef8f.
The local approved plan is docs/plans/2026-09-10-controller-support.md.

Release version: **1.5.0**, built from commit 032afecdca71ddd5b916c4e2dfb06c5c0aeb7f2d. The clean Release build, signing, package verification and live deployment all passed. **Query Meridian.Input/1 to establish API availability.**

Heath confirmed that the controller works and looks good in his tested setup. The installed artifact hashes, exact Skyrim runtime and controller model were not recorded with that confirmation.

## Implemented platform contract

- Separate opt-in Meridian.Input/1 extension beside the unchanged Meridian.View/1 ABI.
- Skyrim-sourced buttons, triggers and sticks; per-control ownership, neutral rearming, repeat/dead zones, and conflict-checked native opening shortcuts.
- Synchronous capture for the eligible focus owner. Selective forwarding preserves housekeeping and gameplay-owned releases while keeping the existing keyboard dispatch guard.
- Injected MeridianInput helper with scopes, spatial navigation, stable selection restoration, modal back, ordinary tabs/ranges/selects, custom action hooks and scrolling.
- Optional shared stick cursor, balanced mouse down/up, input presentation and binding-derived Xbox/PlayStation/generic prompt labels.
- Cancellation for focus/page/window/device/renderer lifecycle changes, bounded deferred delivery, and stale-page/generation rejection.
- Standalone MeridianInputTest consumer with paused/unpaused modes, competing views, reload/recreate commands, ordinary controls, modal/list/drag/analog diagnostics and old-runtime keyboard/mouse fallback.

Implementation files are named in [the consumer guide](MeridianInput-AuthorGuide.md). The adapter and API delegate were kept in small shared headers; transport lives in CEF/ControllerBrowserInput.cpp. Existing ViewBridgeScriptTests plus executed Node/browser tests cover the shared injected source.

## Automated validation

| Check | Result |
|---|---|
| Clean Release build, signing ON | PASS |
| MeridianInputTest native consumer and separate web assets | PASS during implementation; excluded from release |
| Full native CTest suite, including release signatures | 64/64 PASS |
| Node bridge/prompt contract tests | 9/9 PASS |
| Playwright browser interaction tests, including diagnostic page/fallback | 18/18 PASS |
| Normal release manifest and staged Input SDK/guide | PASS |
| Deliberate fixture DLL and web-directory contamination probes | Both rejected |
| Git whitespace checks | PASS |
| Signed runtime DLL/EXE artifacts | All 12 verified |
| Archive contents and per-file SHA-256 | 288 entries verified |
| Live deployment paths/hashes and signatures | PASS; 290 files including two preserved files |
| Public source snapshot and new-commit secret scan | No findings |

Toolchain: MSVC 14.44, C++23, CommonLibSSE-NG 7.4.0 and CEF 152.0.6. Dependencies resolve inside this worktree's build/vcpkg_installed. Browser tests use Playwright 1.63.0 and Chromium headless shell 153.0.8010.12.

Release evidence is retained outside the repository alongside MeridianUI-1.5.0-signed.zip: build-and-sign.log, ctest.log, node-tests.log, browser-tests.log, signed-runtime-manifest.json, deployed-manifest.json and release-receipt.json. The regression suite includes held-on-open/close, passive reconnect gameplay passthrough, mixed forwarding, late-hook visibility, cursor edge balancing, queue invalidation, and existing focus/text/key tests. These are automated model/browser checks, not claims of exercised Skyrim hardware.

## Signed release artifacts

Runtime Data tree: build/release/dist/Release/Data. Package: MeridianUI-1.5.0-signed.zip.
The installed runtime is in the existing Nordic Souls MeridianUI mod folder. A verified recovery backup was retained, and the active INI plus the pre-existing MeridianUI.zip were preserved unchanged. The new versioned archive contains only the signed runtime package, without those local files or diagnostic consumers.

Diagnostic source remains available under src/InputTest with MERIDIAN_BUILD_INPUT_TEST; it is not part of the production package.

SHA-256 of the verified signed artifacts; all three Meridian binaries report file version 1.5.0.0:

| Artifact | SHA-256 |
|---|---|
| MeridianUIPlugin.dll | 7711079EECD7E05317E5889DB26E25A2D4DB9A89575ECC3521721E50A7E41794 |
| MeridianUI.dll | 0384A21F296147941069523A3A410994DB6399CD4306C9293E9B66AD8C8FA0FE |
| MeridianCEFSubprocess.exe | DDF080800DBDE7678A702DA8F46897C859CF40D9E48F5CBFA87DB66F2CCBAAA5 |
| MeridianUI-1.5.0-signed.zip | AAB3788F9D4BE9BF4487E356DB7288033AFF358F00BC1A5814647239CAFFD566 |

## Runtime validation

**Controller smoke test: PASS, user-confirmed.** Heath reported: "The controller is working. Looks good." Treat that as successful operation in his tested setup and proceed with the separate consumer UI integrations.

A fresh Skyrim launch after the 1.5.0 signed deployment has not been run in this release session. The release changes after that controller confirmation were version metadata and documentation.

Individual SE/AE configurations, calibration, paused dispatch cadence, input-hook interoperability, and the full cursor/lifecycle matrix were not separately documented in that report. Their detailed results remain unrecorded, rather than being inferred from the smoke test.

Use [the runtime matrix](testing/MERIDIAN_CONTROLLER_RUNTIME_GATE.md). Record exact runtime/device/translation settings and candidate hashes. TraceInput logs button/stick samples and per-second dispatch/empty-batch cadence so normalization and paused timing can be verified. SE 1.5.97 and each tested AE configuration remain separate gates. Existing earlier AE validation does not establish controller behavior.

Free-text entry still requires a physical keyboard. Direct PlayStation backend behavior, VR, haptics, gyro, touchpads and multiple players are not claimed.

## Consumer follow-up

Horde, Romantasy and Tailor each have docs/MeridianController-Handoff.md plus an AGENTS.md pointer to it. Those notes identify their current native UI files, shared setup steps, and mod-specific work.

- Horde owns follower selection/commands, groups/tabs, confirmations and its paused close lifecycle.
- Romantasy owns actor/enrollment/preferences scopes and the distinction between paused settings and unpaused/passive popups.
- Tailor owns category/outfit/assignment/sliders/naming, custom analog preview behavior, and existing actor/AI/equipment/preview teardown guarantees.

Each mod still needs explicit native opt-in, UI scopes, actions, focus styling, contextual prompts and its own tests. Consumers should use the shared helper and retain keyboard/mouse fallback without independent controller polling. Read the guide and this status before starting one consumer at a time.
