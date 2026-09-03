# Meridian.View/1 Horde Runtime Gate

This gate covers Meridian UI 1.1.1 and Horde's migration from PrismaUI to `Meridian.View/1`. Automated build and contract tests are necessary but do not prove Chromium input, pause, focus, or game-runtime behavior.

## Automated status

| Gate | Status |
|---|---|
| Meridian RelWithDebInfo build | PASS |
| Meridian CTest suite | PASS (18/18) |
| `ViewAPI.h`, `ViewDllLoader.h`, and `Settings.h` vendor parity in Horde | PASS |
| Horde release build | PASS |
| Horde static regression harness | PASS |

## AE 1.6.1170

Status: **1.1.1 RENDERER/FOCUS HOTFIX NOT RETESTED**

User-confirmed on AE: Horde opens through Meridian and has none of PrismaUI's former UI
lag. The first test exposed an unnecessary opening flash and the vanilla cursor rendered
behind the Meridian browser; the user confirmed both fixes in game. A later test exposed
a Chromium startup race that could leave every JavaScript-to-native listener undefined,
including Horde's close glyph. Meridian now retries native bridge initialization and
queues listener registrations until that bridge is available. That framework fix still
requires an in-game retest.

Meridian 1.1.0 later regressed to focused but invisible Horde, Romantasy, and Tailor
views on the active AE adapter because the shared keyed-texture ring could not be
created. Version 1.1.1 probes that exact path before browser creation and selects
SyncCopy when unsupported. It also balances opening-key releases and restores the
captured run/walk state after the real FocusMenu close event.

Run from a fresh Skyrim launch with Meridian UI 1.1.1 and the migrated Horde build:

1. Confirm `MeridianUI.log` reports `Meridian.View/1 requested by 'Horde'` and creation of `Meridian.View/horde/main`.
2. Open Horde only after a save has loaded; confirm the follower grid and state populate
   immediately with no scale/opacity flash.
3. Confirm the game pauses while Horde owns focus.
4. Exercise all major UI directions: follow, wait, passive, summon, home, dismiss, dismissed registry, and settings.
5. Type in each editable control; confirm Skyrim hotkeys do not leak through and text-input state clears on blur/close.
6. With PrismaUI installed, confirm exactly one cursor is visible and it remains above
   Horde across the entire panel. Close with the glyph, Escape, the keybind, and the
   power; confirm vanilla cursor visibility and input recover every time. Verify
   `Horde.log` records `HordeUI: DOM ready` after the page loads.
7. Reload the page or recreate the view once; confirm all 25 native listener functions still work.
8. While another Meridian test view owns focus, try to open Horde. Confirm Horde remains hidden and does not steal focus.
9. Save, reload, fast-travel, open/close Horde repeatedly, then quit to desktop. Confirm no crash and no surviving Meridian CEF helper.

## SE 1.5.97

Status: **NOT RUN**

Repeat the AE matrix on an actual SE 1.5.97 executable and matching SKSE/Address Library environment. A CommonLibSSE-NG multi-runtime build is not sufficient proof.

## VR

Status: **DEFERRED / NOT SUPPORTED BY THIS MILESTONE**

Do not describe Meridian UI 1.1.1 or `Meridian.View/1` as VR compatible. VR requires a dedicated compositor/input backend decision.
