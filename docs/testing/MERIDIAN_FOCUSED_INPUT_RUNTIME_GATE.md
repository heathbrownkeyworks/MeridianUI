# Meridian Focused-Input Runtime Gate

Automated tests prove sink-priority normalization, call-site validation, and
held-key cleanup in isolation. They do not prove the Skyrim input dispatcher,
Chromium focus, or another SKSE mod's hotkey behavior in a real load order.

## Required setup

- Skyrim AE 1.6.1170 with SKSE 2.2.6.
- Tailor using `Meridian.View/1`.
- Open Animations (or the exact mod whose menu uses `O`) enabled.
- The candidate `MeridianUI.dll`, bridge plugin, CEF subprocess, and resources
  deployed as one hash-recorded build.

## Status

| Gate | Status |
| --- | --- |
| Automated input-priority and cleanup tests | PASS |
| Tailor focused Shift+O versus Open Animation Replacer 3.1.5 | PASS — owner confirmed 2026-09-03 |
| Full focus/key-state cleanup matrix | **NOT RUN** |
| Skyrim SE 1.5.97 confirmation | **NOT RUN** |

### Runtime finding from the first candidate

The 2026-09-03 retest failed: Shift+O in Tailor still opened Open Animation
Replacer 3.1.5 even though `MeridianUI.log` confirmed the priority-layer hook
was installed. OAR's shipped PDB and matching source show that it hooks the
same `PollInputDevices` dispatch call, processes the event list directly, and
only then invokes the previous call target. Because OAR loaded after Meridian,
sink reordering could not affect that earlier direct handler.

The replacement candidate keeps the priority layer and installs a distinct
outer guard after SKSE plugin loading. It delivers the batch to Meridian once,
then gives chained hooks an empty batch only when Meridian consumed the
original. A new run must show both `priority layer installed` and `outer
focused-input guard installed` in `MeridianUI.log` before this gate is tested.

### Confirmed replacement-candidate result

The owner confirmed the focused Shift+O regression fixed on 2026-09-03. The
matching session ran Skyrim AE 1.6.1170 with SKSE 2.2.6 and recorded both the
priority layer and post-plugin-load outer guard before Tailor requested
`Meridian.View/1`. The deployed unsigned `MeridianUI.dll` SHA-256 was
`2EFF092A334EC86B700EEB19D3BA23804F18EBA1EBC1307FFC23AA186247E33E`.
The session subsequently drained all three browsers and completed
`CefShutdown`; Skyrim was no longer running when the evidence was audited.

This passes the reported Tailor/OAR chord collision only. The remaining
modifier, Alt-Tab, alternate close-path, no-focus control, and SE checks below
remain separate gates until explicitly exercised.

## Focused typing regression

1. Start Skyrim through the intended MO2 profile and confirm `MeridianUI.log`
   reports `InputDispatchHook` installed successfully.
2. Open Tailor and navigate to **Create Outfit**.
3. Focus the outfit-name field and type a name containing lowercase and
   uppercase `o`, including repeated `O` presses.
4. Confirm every character reaches the field and Open Animations does not open.
5. While the field remains focused, type keys assigned to other enabled mod
   hotkeys. Confirm none of those menus or actions fire.

## Focus and key-state cleanup

1. Hold Shift, type `O`, release `O`, then release Shift. Confirm the field has
   the expected case and no external hotkey fires.
2. Toggle Caps Lock on and off while typing. Confirm case follows the actual
   toggle and does not remain stuck after focus changes.
3. Hold a normal key and then close or unfocus Tailor through each supported
   path. Reopen it and confirm the key is not logically held or repeating.
4. Repeat with Shift, Control, and Alt held during the transition. Confirm no
   modifier remains stuck in Chromium, Skyrim, or another mod.
5. Alt-tab away and back while Tailor owns focus. Confirm keyboard and mouse
   control return with no repeated key and no stuck modifier.
6. Exercise Escape through Tailor sub-screens and final close. Confirm no extra
   Escape presses are required to regain normal control.

## No-focus control

1. Close or unfocus every Meridian view.
2. Press `O` and several other configured mod hotkeys.
3. Confirm Open Animations and the other mods behave exactly as they did before
   the candidate build.

Do not mark the runtime row passed without the runtime/SKSE versions, relevant
Meridian and consumer logs, artifact SHA-256 values, exact focus-transition
paths exercised, and a clean process-exit observation.
