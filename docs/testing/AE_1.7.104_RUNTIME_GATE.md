# Meridian 1.2.0 Skyrim 1.7.104 Runtime Gate

Automated builds prove compilation, metadata, packaging, signatures, and
isolated logic. They do not prove Meridian's CEF renderer, input path, or
instruction-level hooks inside Skyrim 1.7.104.

## Required environment

| Component | Required value | Status |
| --- | --- | --- |
| SkyrimSE.exe | 1.7.104.0 | **NOT RUN** |
| SKSE | 2.3.1 | **NOT RUN** |
| Address Library | 13.0, `versionlib-1-7-104-0.bin` | Database inspected; runtime **NOT RUN** |
| Meridian UI | Unsigned 1.2.0 candidate built from this source | Release build and 46/46 CTest **PASS** |
| Consumers | Tailor, Horde, and Romantasy Meridian builds | **NOT RUN** |

The Address Library format-5 database contains every Meridian base relocation:

| Use | Address Library ID | Skyrim 1.7.104 RVA |
| --- | ---: | ---: |
| Renderer end / Present call owner | 77246 | `0x1009F70` |
| WinMain / shutdown call owner | 36544 | `0x651300` |
| Input translation call owner | 68782 | `0xE127C0` |
| CursorMenu primary vtable | 215246 | `0x1974588` |
| CursorMenu secondary vtable | 215248 | `0x19745D8` |

These IDs establish that the containing functions and vtables are present.
They do not prove Meridian's `+0x9`, `+0x1AE`, and `+0x2CB` call-site offsets.
Version 1.2.0 verifies the expected call opcode before writing each patch and
logs/refuses an unexpected site.

## Acceptance checklist

1. Start Skyrim from MO2 with no stale `MeridianCEFSubprocess.exe` processes.
2. Confirm `MeridianUI.log` reports version 1.2.0 and does not contain
   `install refused`, `platform rendering unavailable`, or an Address Library
   format/version error.
3. Open Tailor, Horde, and Romantasy independently. Confirm every interface is
   visible; Horde and Romantasy pause the game; Tailor remains unpaused.
4. Type, edit, select, delete, and paste in Tailor search/filter fields and
   name an outfit. Exercise any editable fields exposed by Horde and Romantasy.
5. Confirm mouse input, hover/click behavior, and exactly one cursor with and
   without PrismaUI installed.
6. Open each UI once from run mode and once from walk mode. Close by Escape,
   page control, and toggle key where supported; confirm the original movement
   mode is restored every time.
7. Repeat close/reopen cycles and one browser destruction/recreation cycle.
8. Exercise alt-tab, console, pause menu, loading screen, resolution change,
   and fullscreen/windowed transitions.
9. Exit with no browser, an active browser, and after recreation. Confirm no
   crash and no surviving Meridian CEF helper process.
10. Preserve the exact tested DLL/EXE hashes and attach the complete Meridian
    and SKSE logs before marking the gate passed.

## Regression gates

After 1.7.104 passes, repeat the essential rendering, typing, pause semantics,
run/walk restoration, close/reopen, and shutdown checks on:

| Runtime | SKSE | Status |
| --- | --- | --- |
| AE 1.6.1170 | 2.2.6 | **PASS**, owner-confirmed with Meridian 1.2.0 on 2026-09-02 |
| SE 1.5.97 | 2.0.20 | **NOT RUN** for Meridian 1.2.0 |

VR and DLSS 5 remain outside this release gate.
