# Meridian 1.1.1 SE/AE Runtime Gate

Automated builds and CTest prove compilation, packaging, signatures, and isolated
logic only. They do not prove CEF, hooks, input, rendering, or shutdown inside Skyrim.

Version 1.1.0 failed the active AE test: Horde, Romantasy, and Tailor reached DOM ready
but rendered no surface because their shared keyed-texture rings could not be created.
Version 1.1.1 probes that transport on Skyrim's actual adapter before creating a
browser and falls back to the geometry-aware SyncCopy renderer when unsupported.

## Required profiles

| Gate | Runtime | SKSE | Status |
| --- | --- | --- | --- |
| AE | Skyrim 1.6.1170 | SKSE 2.2.6 | **NOT RUN** after 1.1.1 hotfix |
| SE | Skyrim 1.5.97 | SKSE 2.0.20 | **NOT RUN** |
| VR | Skyrim VR | SKSEVR | Unsupported |

Use the exact same staged `MeridianUIPlugin.dll`, `MeridianUI.dll`, CEF subprocess,
and CEF resource hashes for both SE and AE. Confirm the matching Address Library is
active in each profile.

## Immediate hotfix checklist

1. Start from MO2 with no stale `MeridianCEFSubprocess.exe` processes.
2. Confirm `MeridianUI.log` reports version 1.1.1 and either a successful shared
   keyed-texture probe or an explicit `falling back to SyncCopy` warning.
3. Open and close Horde, Romantasy, and Tailor independently. Each UI must render;
   Horde and Romantasy must retain their requested pause behavior; Tailor must remain
   unpaused.
4. For each UI, begin once in run mode and once in walk mode. Close through every
   supported path and confirm the original state is preserved.
5. Exercise modifier-based opening chords and confirm released modifiers do not remain
   logically held after the UI closes.
6. Confirm mouse, keyboard, cursor ownership, native callbacks, promises, view removal,
   and one warm recreation.
7. Repeat with PrismaUI absent and installed. Exactly one cursor must be visible.
8. Exit to desktop from no browser, an active browser, and after warm recreation.
   Confirm no surviving Meridian CEF helper and no crash dump.

## Full regression checklist

1. Exercise overlapping browser rectangles, scale, z-order, hit testing, and final
   handle release/recreation.
2. Exercise window resize, resolution change, alt-tab, fullscreen/windowed changes,
   console, RaceMenu, pause menu, loading screens, and renderer-process termination.
3. Confirm a native-bound browser rejects remote/file/cross-mod navigation and remote
   resources without exposing callbacks.
4. In a development-only profile, confirm `AllowRemoteContent=true` permits an unbound
   remote browser without native bindings, then restore the release default.

Do not mark a row passed without the runtime versions, complete Meridian/SKSE logs,
artifact SHA-256 values, Prisma state, and orphan-process observation.
