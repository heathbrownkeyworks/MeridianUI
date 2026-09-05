# Meridian UI

Meridian UI is a native UI platform for Skyrim Special Edition and Anniversary Edition. It lets SKSE plugins create interfaces with HTML, CSS, and JavaScript through Chromium Embedded Framework while retaining Skyrim-aware input, focus, rendering, and lifecycle behavior.

Meridian is an independent fork of [NirnLabUIPlatform](https://github.com/kkEngine/NirnLabUIPlatform). It uses its own plugin names, runtime directory, messaging channel, and public APIs, so it can be installed alongside the original project.

## Current status

The current source version is 1.2.1. The public `IUIPlatformAPI` version 1.0, `Meridian.View/1`, and native rendering API versions remain stable.

The runtime results below were recorded for 1.2.0. Elevated and ordinary MO2
gameplay validation of the 1.2.1 startup fix is **NOT RUN**; see the
[startup runtime gate](docs/testing/MERIDIAN_ELEVATED_STARTUP_RUNTIME_GATE.md).

| Runtime | Status |
| --- | --- |
| Skyrim AE 1.6.1170 | In-game validation passed, including SkyrimUpscaler Build 14 with DLSS Neural Reconstruction |
| Skyrim AE 1.7.104 | Builds against CommonLibSSE-NG 7 and resolves all required Address Library IDs; in-game validation is pending |
| Skyrim SE 1.5.97 | Tailor RingBuffer stability passed; the broader cross-feature regression is pending |
| Skyrim VR | Not supported |

SkyrimUpscaler Build 14 uses the opt-in `BeforeRendererEnd` compositor timing described below. Meridian's default presentation order remains unchanged for users who do not need this compatibility mode.

## What's new in 1.2.1

- Preserve Skyrim's launch privileges during CEF startup, preventing Chromium
  from relaunching the game when MO2 runs as administrator.
- Reject further CEF initialization attempts after a failure and log that
  Skyrim must be restarted before trying again.

## Previous changes in 1.2.0

- Added an opt-in compositor path for SkyrimUpscaler Build 14 and DLSS Neural Reconstruction.
- Fixed retained RingBuffer surfaces that could flicker or update only while the mouse was moving on Skyrim SE.
- Added a post-plugin-load input guard so focused browser keystrokes are delivered once and hidden from competing direct input hooks.
- Added balanced Chromium key releases during focus changes, Alt-Tab, browser closure, and shutdown.
- Updated the native plugins to CommonLibSSE-NG 7.0.0 with Address Library format-5 metadata for Skyrim 1.7.99 and newer layouts.
- Added instruction validation before Meridian installs runtime-sensitive Present, shutdown, and native-language hooks.
- Kept the public browser, View, and native rendering API versions stable.

## Features

- Off-screen Chromium rendering integrated with Skyrim's Direct3D 11 presentation path
- Triple-buffered shared-texture RingBuffer transport with automatic SyncCopy fallback
- Multiple independent browser views
- Shared keyboard, mouse, cursor, focus, pause, and text-entry ownership across consumer mods
- Focused input isolation that prevents duplicate delivery and interference from competing hooks
- Automatic restoration of the player's movement mode when a focused interface closes
- JavaScript-to-C++ functions, C++-to-JavaScript events, and asynchronous promise bindings
- Native compositor layers and rigid, skinned, weighted, armor-aware, and live-reference NIF preview APIs
- Opt-in SkyrimUpscaler Build 14 and DLSS Neural Reconstruction compatibility
- Secure `mod://` resource loading with host pinning and path traversal protection
- Browser cleanup and CEF subprocess shutdown during normal game exit
- Runtime validation before instruction-level Skyrim hooks are installed
- SE and AE runtime detection through CommonLibSSE-NG 7 and Address Library

## Public APIs

| API | Purpose |
| --- | --- |
| `IUIPlatformAPI` 1.0 | Browser creation and the original platform interface |
| `Meridian.View/1` | Shared browser hosting for mods such as Tailor, Horde, and Romantasy |
| `Meridian.RenderLayer/1` | Native compositor layers |
| `Meridian.NifView/1` | Rigid and skinned NIF previews with camera and lighting control |
| `Meridian.NifScene/1` | Atomic multi-NIF scene composition |
| `Meridian.NifScene/2` | Low and high weight mesh interpolation |
| `Meridian.NifScene/3` | Runtime ARMO and ARMA resolution by race, sex, and body weight |
| `Meridian.NifScene/4` | Live-reference appearance data for armor, skin, and head parts |

Public headers are located in [`src/UIPlatform/MeridianUIAPI`](src/UIPlatform/MeridianUIAPI). Release packages include the same headers under `MeridianUI/SDK/MeridianUIAPI`.

See the [Meridian UI author guide](docs/MeridianUI-AuthorGuide.md) and [Meridian View author guide](docs/MeridianView-AuthorGuide.md) for the complete interface contracts.

## Requirements

Users need:

- Skyrim Special Edition or Anniversary Edition
- [SKSE](https://skse.silverlock.org/) matching the installed Skyrim runtime
- [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444) matching the installed Skyrim runtime

Skyrim 1.7.104 requires SKSE 2.3.1 and the Address Library format-5 database.

## Installation

Install a release package with a mod manager. The package already uses Skyrim's `Data` directory layout:

```text
Data/
|-- MeridianUI/
|   |-- MeridianUI.dll
|   |-- MeridianCEFSubprocess.exe
|   `-- Chromium runtime files
`-- SKSE/
    `-- Plugins/
        |-- MeridianUIPlugin.dll
        `-- MeridianUI.ini.disabled
```

Do not copy source files or build directories into the game. When using Mod Organizer 2, install the release as its own mod and launch Skyrim through MO2.

## DLSS 5 compatibility

Meridian 1.2.0 was validated on Skyrim AE 1.6.1170 with SkyrimUpscaler Build 14, DLSS, and DLSS Neural Reconstruction enabled. This configuration needs Meridian to composite into Skyrim's bound game render target before the renderer-end call.

Rename `Data\SKSE\Plugins\MeridianUI.ini.disabled` to `MeridianUI.ini`, then enable the compatibility timing:

```ini
[Compatibility]
CompositorTiming=BeforeRendererEnd
```

Leave this setting at its default `AfterRendererEnd` value when SkyrimUpscaler Build 14 compatibility is not required. The setting changes Meridian's internal presentation order and does not change any consumer API.

See the [DLSS 5 runtime gate](docs/testing/MERIDIAN_DLSS5_RUNTIME_GATE.md) for the validated environment, expected log evidence, and remaining regression checks.

## Integrating an SKSE plugin

`SKSELoader.h` is the preferred entry point. It validates Meridian's version at `kPostPostLoad` and requests the API at `kInputLoaded`, after Skyrim input services are available.

```cpp
#include "MeridianUIAPI/SKSELoader.h"

Meridian::UI::IUIPlatformAPI* g_meridian = nullptr;
Meridian::UI::Settings g_meridianSettings{};

bool RegisterMeridian()
{
    auto* messaging = SKSE::GetMessagingInterface();
    if (messaging == nullptr) {
        return false;
    }

    messaging->RegisterListener([](SKSE::MessagingInterface::Message* message) {
        Meridian::UI::SKSELoader::ProcessSKSEMessage(message, &g_meridianSettings);
    });

    Meridian::UI::SKSELoader::GetUIPlatformAPIWithVersionCheck(
        [](Meridian::UI::IUIPlatformAPI* api) {
            g_meridian = api;
        });
    return true;
}
```

`DllLoader.h` is also available for consumers that intentionally load the exported DLL interface directly.

## Browser content security

Release browsers should begin at `mod://<mod-name>/<path>`. Meridian pins the initial mod host and rejects remote navigation, local file URLs, cross-mod access, traversal attempts, unsafe redirects, and untrusted subresources.

HTTP and HTTPS content requires the explicit development-only `AllowRemoteContent` setting. Remote documents never receive native Skyrim bindings. The setting does not permit `file://`, FTP, or arbitrary custom schemes.

## Building from source

Build requirements:

- Windows 10 or Windows 11
- Visual Studio 2022 with Desktop development with C++
- CMake 3.23 or newer
- A current [vcpkg](https://github.com/microsoft/vcpkg) checkout

Set `VCPKG_ROOT`, configure an out-of-source build, and build the Release configuration:

```powershell
$env:VCPKG_ROOT = "C:\path\to\vcpkg"
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
    -DBUILD_TESTING=ON `
    -DMERIDIAN_ENABLE_SIGNING=OFF
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

The repository supplies vcpkg overlays for CommonLibSSE-NG and the CEF binary distribution. Production output is staged below `build/dist/Release/Data`.

Optional build flags:

| Option | Default | Purpose |
| --- | --- | --- |
| `MERIDIAN_ENABLE_SIGNING` | `OFF` | Sign and verify Release binaries with Azure Artifact Signing |
| `MERIDIAN_BUILD_FIXTURE` | `OFF` | Build the in-game Meridian UI lifecycle fixture |
| `MERIDIAN_BUILD_NIF_TEST` | `OFF` | Build the standalone NIF test consumer |
| `BUILD_TESTING` | CMake default | Build the native regression suite |

## Release signing

Signing is optional and disabled by default. Maintainers who use Azure Artifact Signing can:

1. Copy `signing/metadata.example.json` to the ignored file `signing/metadata.json` and fill in their own account values, or set `MERIDIAN_SIGNING_METADATA` to a metadata file outside the repository.
2. Set `MERIDIAN_SIGNING_PUBLISHER` to the exact Authenticode publisher name.
3. Put `signtool.exe` on `PATH` or set `MERIDIAN_SIGNTOOL_PATH`.
4. Install the Azure Artifact Signing client tools and sign in with the Azure CLI.
5. Configure with `-DMERIDIAN_ENABLE_SIGNING=ON`.

The release pipeline preserves valid vendor signatures, appends the maintainer signature, verifies every staged DLL and EXE, and blocks deployment if validation fails.

## Testing and issue reports

The automated suite covers API validation, browser security, focus ownership,
input handling, frame transport, process shutdown, CEF initialization failures
and concurrent retries, NIF scene behavior, runtime call-site validation, and
release packaging. Signed Release builds also verify every packaged DLL/EXE
signature. Build proof and pending gameplay checks are recorded separately in
[`docs/testing`](docs/testing).

When reporting a problem, include:

- Exact Skyrim and SKSE versions
- Address Library version
- Meridian version
- The complete `MeridianUI.log`
- Reproduction steps and the affected consumer mod
- Whether another UI framework or graphics injector is installed

## Project layout

```text
src/UIPlatform/       Core browser, input, rendering, and public API implementation
src/UIPlugin/         SKSE bridge plugin
src/CEFSubprocess/    Chromium helper executable
src/UIPlatformTest/   Optional in-game integration fixture
src/NifTest/          Optional NIF integration consumer
tests/UIPlatform/     Native automated tests
docs/                 Public author guides and runtime test gates
overlay_ports/        Reproducible vcpkg dependency overlays
signing/              Optional public signing scripts and configuration template
```

## Attribution

Meridian UI is derived from NirnLabUIPlatform by [kkEngine](https://github.com/kkEngine). Thanks also to [Pospelove](https://github.com/Pospelove), [Skymp](https://github.com/skyrim-multiplayer/skymp), the [CommonLibSSE](https://github.com/Ryan-rsm-McKenzie/CommonLibSSE) contributors, the [CommonLibSSE-NG](https://github.com/alandtse/CommonLibSSE-NG) contributors, and the [Chromium Embedded Framework](https://bitbucket.org/chromiumembedded/cef) project.

## License

Meridian UI's native implementation and distributed binaries are licensed as
GPL-3.0-or-later with the Modding Exception and GPL-3.0 Linking Exception in
[EXCEPTIONS.md](EXCEPTIONS.md). This is required because the native plugins
statically link CommonLibSSE-NG.

The standalone public API headers remain available under the MIT License, and
the original NirnLabUIPlatform MIT attribution is retained. The exact boundary,
corresponding-source information, and third-party terms are documented in
[LICENSING.md](LICENSING.md) and
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
