# Meridian controller support status

Updated: 2026-09-10. **Controller operation confirmed by Heath; ready for mod-specific integration.**

Branch: controllersupport. Implementation commits: 00f2e51 and 10dcfa5d32cdfcb1c746bed8bf0d71d41c390652.
Baseline: Meridian UI 1.4.0, a169ef4f2e6c0f38a5f1af98eccfff8c8e6aef8f.
The local approved plan is docs/plans/2026-09-10-controller-support.md.

Release version: **1.5.0**. Heath authorized building, signing, deploying and pushing this version. Release verification is in progress; the artifact table below records the earlier implementation candidate, not the signed release. **Query Meridian.Input/1 to establish API availability.**

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
| Full Release build, signing OFF | PASS |
| MeridianInputTest native consumer and separate web assets | PASS |
| Full native CTest suite | 63/63 PASS |
| Node bridge/prompt contract tests | 9/9 PASS |
| Playwright browser interaction tests, including diagnostic page/fallback | 18/18 PASS |
| Normal release manifest and staged Input SDK/guide | PASS |
| Deliberate fixture DLL and web-directory contamination probes | Both rejected |
| Git whitespace checks | PASS |

Toolchain: MSVC 14.44, C++23, CommonLibSSE-NG 7.4.0 and CEF 152.0.6. Dependencies resolve inside this worktree's build/vcpkg_installed. Browser tests use Playwright 1.63.0 and Chromium headless shell 153.0.8010.12.

Logs: build/controller-full-build.log, build/controller-ctest.log, build/controller-node-tests.log and build/controller-browser-tests.log. The regression suite includes held-on-open/close, passive reconnect gameplay passthrough, mixed forwarding, late-hook visibility, cursor edge balancing, queue invalidation, and existing focus/text/key tests. These are automated model/browser checks, not claims of exercised Skyrim hardware.

## Earlier implementation candidate artifacts

Runtime Data tree: build/release/dist/Release/Data.
Separate diagnostic mod: build/release/input-test/Release.
F10 or LeftShoulder + Start opens the diagnostic UI; Escape/back follows its close/modal behavior.

SHA-256 snapshot of the built candidate:

| Artifact | SHA-256 |
|---|---|
| MeridianUIPlugin.dll | 7C13E922CB7E0CC70CB699A16BD4E4FACAADA6F8CADC9272C9724157CC73A16C |
| MeridianUI.dll | 1957114BB9E9BC93736A8B752143FB540315E7576978797462C3EFFC3916D977 |
| MeridianInputTest.dll | BB40D9E25689C621551497FE5B199591CB71B392054D986D73CE1DFA2ECF0884 |

## Runtime validation

**Controller smoke test: PASS, user-confirmed.** Heath reported: "The controller is working. Looks good." Treat that as successful operation in his tested setup and proceed with the separate consumer UI integrations.

Individual SE/AE configurations, calibration, paused dispatch cadence, input-hook interoperability, and the full cursor/lifecycle matrix were not separately documented in that report. Their detailed results remain unrecorded, rather than being inferred from the smoke test.

Use [the runtime matrix](testing/MERIDIAN_CONTROLLER_RUNTIME_GATE.md). Record exact runtime/device/translation settings and candidate hashes. TraceInput logs button/stick samples and per-second dispatch/empty-batch cadence so normalization and paused timing can be verified. SE 1.5.97 and each tested AE configuration remain separate gates. Existing earlier AE validation does not establish controller behavior.

Free-text entry still requires a physical keyboard. Direct PlayStation backend behavior, VR, haptics, gyro, touchpads and multiple players are not claimed.

## Consumer follow-up

Horde, Romantasy and Tailor each have docs/MeridianController-Handoff.md plus an AGENTS.md pointer to it. Those notes identify their current native UI files, shared setup steps, and mod-specific work.

- Horde owns follower selection/commands, groups/tabs, confirmations and its paused close lifecycle.
- Romantasy owns actor/enrollment/preferences scopes and the distinction between paused settings and unpaused/passive popups.
- Tailor owns category/outfit/assignment/sliders/naming, custom analog preview behavior, and existing actor/AI/equipment/preview teardown guarantees.

Each mod still needs explicit native opt-in, UI scopes, actions, focus styling, contextual prompts and its own tests. Consumers should use the shared helper and retain keyboard/mouse fallback without independent controller polling. Read the guide and this status before starting one consumer at a time.
