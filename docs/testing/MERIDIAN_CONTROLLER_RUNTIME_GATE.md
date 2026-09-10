# Meridian controller runtime gate

Status: **NOT RUN** in Skyrim. A successful native build or desktop browser test is not controller hardware or in-game proof.

## Candidate and fixture

Use the controllersupport worktree and record its exact commit plus SHA-256 of MeridianUIPlugin.dll, MeridianUI.dll and MeridianInputTest.dll. No version bump, signing, live deployment or public release is implied by this gate.

Build with:
```powershell
cmake --preset release -DMERIDIAN_ENABLE_SIGNING=OFF -DBUILD_TESTING=ON -DMERIDIAN_BUILD_INPUT_TEST=ON
cmake --build --preset release --parallel 8
ctest --test-dir build/release -C Release --output-on-failure
npm --prefix tests/web ci
node tests/web/node_modules/playwright/cli.js install chromium --only-shell
node --test tests/web/controller-bridge.test.mjs tests/web/controller-prompts.test.mjs
npm --prefix tests/web run test:navigation
```

Runtime candidate: build/release/dist/Release/Data.
Separate fixture mod: build/release/input-test/Release.
The normal release manifest must reject fixture artifacts in the runtime Data tree.

Install the candidate/fixture only through the authorized test deployment workflow, preserving the active INI and a recovery backup. The fixture opens with F10 or LeftShoulder + Start. Escape/B closes. It logs interface acquisition and shortcut conflict results to MeridianInputTest.log.

## Evidence to record

For each run: exact Skyrim runtime, SKSE version, controller model, wired/wireless mode, translation software/settings, native D3D11 or Windows DXVK, candidate hashes, MeridianUI.log, MeridianInputTest.log, and pass/fail observations.

With Controller.TraceInput=true, record button IDs and values, trigger range, stick range, center/held behavior, and whether the dispatch cadence continues when paused/idle. Disable tracing afterward. Compare with the adapter assumptions before declaring calibration complete. The runtime samples the engine-owned XInput snapshot only for rearming; other device backends need explicit neutral-transition verification.

## Required cases

| Case | Expected | Status |
|---|---|---|
| SE 1.5.97 | Full fixture usable | NOT RUN |
| Supported AE runtime(s), recorded exactly | Same contract | NOT RUN |
| Reference Xbox/XInput device | Buttons, triggers, both sticks normalize correctly | NOT RUN |
| PlayStation with chosen translation layer | Correct actions and selected prompt labels | NOT RUN |
| Direct PlayStation device, if claimed | Correct mapping and neutral rearming | NOT RUN |
| Navigation, list filtering, tabs, modal | Deterministic selection and restoration; one activation | NOT RUN |
| Slider/select edit; physical-keyboard text input | Correct values, no unexpected editor navigation | NOT RUN |
| Cursor, drag, scroll, mouse handoff | One cursor, no jump, balanced release | NOT RUN |
| Paused and unpaused views | Repeat/cursor/scroll continue; game controls captured | NOT RUN |
| Open while holding movement/modifier; close while holding B/trigger | No opening accept, stuck control or gameplay action on close | NOT RUN |
| Competing focus and duplicate/overlapping opener | One owner; Busy/Conflict results | NOT RUN |
| Reload, hide, destroy/recreate, load game | No stale event, callback, text lease or drag | NOT RUN |
| Alt-tab, disconnect/reconnect while held | Cancel and require neutral, no resumed operation | NOT RUN |
| Mixed device batch; late input sinks/direct competing hook | Owned input hidden; housekeeping and balancing releases forwarded | NOT RUN |
| Tailor focused typing with Open Animation Replacer | Existing O/Shift/Caps Lock protection preserved | NOT RUN |
| Existing non-opted-in Horde/Romantasy/Tailor | Existing mouse/keyboard and no-focus hotkeys unchanged | NOT RUN |
| Native D3D11 and Windows DXVK, resize/scale/ultrawide | Correct pointer conversion and stable rendering | NOT RUN |
| Exit Skyrim | Browser/helper lifecycle ends cleanly | NOT RUN |

Record individual results; do not mark the table passed from an aggregate test count. Free-text entry uses a physical keyboard. VR, haptics and direct hardware compatibility not exercised here remain unclaimed.
