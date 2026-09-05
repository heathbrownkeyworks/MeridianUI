# Meridian Elevated Startup Runtime Gate

## Scope

Meridian 1.2.1 addresses CEF exit code 38 (`NORMAL_EXIT_AUTO_DE_ELEVATED`) when
Skyrim inherits administrator privileges from MO2. It adds `do-not-de-elevate`
in the browser-process command-line callback. Ordinary launches remain ordinary;
an elevated host retains its privileges. CEF is embedded inside Skyrim, so it
must not independently relaunch the game to drop elevation.

The CEF maintainer recommends disabling this relaunch for embedded game hosts:
[CEF issue 3960](https://github.com/chromiumembedded/cef/issues/3960#issuecomment-3114022537).
The bundled SDK is CEF `141.0.11+g7e73ac4+chromium-141.0.7390.123`.

An initialization failure now lasts for the process lifetime. Meridian preserves
the exit code, rejects platform retries before constructing CEF settings, rejects
browser creation, and makes no CEF shutdown calls for an initialization that
never succeeded. Restart Skyrim after a fatal initialization error. This guard
prevents re-entry; it does not claim that CEF supports recovering in place after
`CefInitialize` returns false. CEF documents exit as the required response.

CEF runs with the elevated host's privileges in an administrator session. Remote
content and remote debugging remain disabled by default, and native-bound views
remain restricted to their trusted local mod content. No graphics transport,
consumer API, renderer selection, sandbox setting, or VR support changes are
included. The separate cross-device render-target report is outside this fix.

## Automated evidence

| Check | Status | Evidence |
|---|---|---|
| Failure regression before fix | PASS | Both code 38 and code 1 tests detected forbidden re-entry; the successful initialization control passed. |
| Failure regression after fix | PASS | Both failure cases reject eight concurrent retries, preserve the original code, reject browser creation, and perform cleanup without further CEF calls. |
| Successful lifecycle control | PASS | One initialization and scheme registration, duplicate rejection, one close barrier and shutdown, no reinitialization after shutdown. |
| Initial unsigned candidate build | PASS | MSVC 19.44, all native targets built; no compiler warnings or errors in the build log. |
| Initial unsigned candidate tests | PASS | 51/51 tests passed, including the release manifest check. |
| Signed 1.2.1 build and tests | PASS | All 52 Release tests passed, including release manifest and signature verification. All 12 packaged DLL/EXE artifacts have valid Meridian signatures; the three native Meridian binaries report 1.2.1.0. |

The regression executable compiles the actual `CEFService.cpp` against counted
CEF test doubles. It does not launch Chromium or Skyrim. These tests prove the
service's retry and cleanup behavior; they do not prove the elevated game launch.

## Signed 1.2.1 build

- Implementation commit: `342129f` (`fix: support elevated MO2 startup in Meridian UI 1.2.1`).
- Build: `E:/tmp/MeridianUI-1.2.1-release-20260905/build`.
- Staged Data: `E:/tmp/MeridianUI-1.2.1-release-20260905/build/dist/Release/Data`.
- Compiled native version: `1.2.1.0`; public API versions remain unchanged.
- All 12 packaged DLL/EXE files passed Authenticode verification with the
  Meridian publisher. Existing valid vendor signatures were preserved.
- Full Release build and 52/52 CTest checks passed. Logs are
  `E:/tmp/MeridianUI-1.2.1-release-20260905/build-release.log` and
  `E:/tmp/MeridianUI-1.2.1-release-20260905/ctest-release.log`.

| Artifact | SHA-256 |
|---|---|
| `Data/MeridianUI/MeridianUI.dll` | `D15D78AFB6DB1D20B4F00886C09FB45EE74798177AA7CE89265CE393561A46C1` |
| `Data/MeridianUI/MeridianCEFSubprocess.exe` | `DFF7EBB5CE65AEE72747734181A52EB425976BC720C927585373725835B80ADB` |
| `Data/SKSE/Plugins/MeridianUIPlugin.dll` | `97928CDE92A9943C4764754E521ACAB2A8DCCC2870292E96CEB74BD2732B8B45` |

## Initial unsigned candidate

- Source: `E:/tmp/MeridianUI-elevated-cef-20260905`, branch `codex/elevated-cef-startup`.
- Base: `cfd7ebe`, with this startup patch applied.
- Build: `E:/tmp/MeridianUI-elevated-cef-build-20260905`.
- Staged Data: `E:/tmp/MeridianUI-elevated-cef-build-20260905/dist/Release/Data`.
- These initial candidate binaries are unsigned and superseded by the signed
  1.2.1 build. Their native version is 1.2.0; hashes below identify this initial
  test artifact only.

| Artifact | SHA-256 |
|---|---|
| `Data/MeridianUI/MeridianUI.dll` | `BE508B1323D9B6C2C5671B0CFFD539E40E26E995D5B382656AD4CF3D8FAF777A` |
| `Data/MeridianUI/MeridianCEFSubprocess.exe` | `A33BD7177C1FE20146DCBBE82AF503E5D2934E438C09A4879BA2657173C452F9` |
| `Data/SKSE/Plugins/MeridianUIPlugin.dll` | `7C25422E406774060418338680DA868ED8A9113E37263A89AB07E885224407D4` |

Build-directory evidence: `build-release.log`, `build-final-manifest.log`,
`startup-before-fix.log`, `startup-after-fix.log`, and `ctest-release.log`.

## Required gameplay checks

Record the Skyrim/SKSE versions, MO2 version, administrator state of both MO2 and
Skyrim, candidate DLL hashes, and full matching `MeridianUI.log`/CEF logs for
every run. Test ordinary and elevated launches from the same profile, with all
prior Skyrim and Meridian helper processes exited before switching modes.

1. Launch Skyrim through MO2 normally, then repeat with MO2 running as
   administrator. Neither run should create an extra Skyrim process during CEF
   initialization. Both should log
   `preserving host launch privileges (do-not-de-elevate)` followed by successful
   `CEFService::CEFInitialize`, without code 38.
2. Open Horde, Tailor, and Romantasy menus in the intended profile. Confirm
   content appears, input works, and closing/reopening each menu works.
3. Quit Skyrim normally with a menu having been used. Verify browser close
   callbacks drain before `CefShutdown`, and no `MeridianCEFSubprocess.exe`
   survives ten seconds after the Skyrim process exits.
4. If initialization fails for another reason, collect the first error code and
   subsequent restart diagnostic. Repeated consumer requests must not start CEF
   again. Exit Skyrim and begin a new process before retesting; do not create a
   deliberately damaged installation to exercise this path in the game.

| Runtime | Ordinary MO2 | Elevated MO2 | Menus and shutdown |
|---|---|---|---|
| AE 1.6.1170 | NOT RUN | NOT RUN | NOT RUN |
| SE 1.5.97 | NOT RUN | NOT RUN | NOT RUN |
| AE 1.7.104 | NOT RUN | NOT RUN | NOT RUN |

Previous release validation does not validate this patch. Keep SE and AE results
separate; VR remains outside scope.
