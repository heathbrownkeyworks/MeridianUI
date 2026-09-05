# Changelog

All notable MeridianUI changes are recorded here.

## 1.2.1 - 2026-09-05

### Fixed

- Preserve Skyrim's launch privileges during CEF startup so an elevated MO2
  launch does not trigger Chromium's host relaunch and exit code 38.
- Stop retrying CEF after initialization fails, retain the original error code,
  and log that Skyrim must be restarted before trying again.

Elevated and ordinary MO2 gameplay validation for this change is **NOT RUN**.
See the [elevated startup runtime gate](docs/testing/MERIDIAN_ELEVATED_STARTUP_RUNTIME_GATE.md).

## 1.2.0 - Candidate

### Fixed

- Reacquire DXGI keyed-mutex ownership before every retained RingBuffer draw.
  Newly painted frames still use key 1, while later Skyrim presents reacquire
  key 0 after the preceding draw releases it, preventing an otherwise static
  browser surface from flickering or appearing only during mouse movement.
- Install a distinct outer input guard after SKSE plugin loading, deliver each
  batch to Meridian once, and hide consumed batches before earlier-installed
  direct input hooks such as Open Animation Replacer's Shift+O handler can
  observe focused browser keystrokes. Sink-priority normalization remains as a
  fallback for ordinary event consumers.
- Emit balancing Chromium key-up events before browser focus is released,
  including Alt-Tab transitions, so interrupted keys and modifiers cannot
  remain logically held when control returns.

### Changed

- Updated the native platform and bridge plugins to CommonLibSSE-NG 7.0.0 at
  commit `8b032fa992750d654d6d38a33731714d8b86be1f` for Skyrim 1.7.99+
  structure layouts and Address Library format 5.
- Kept the public `Meridian.View/1` and extension ABIs unchanged while moving
  Meridian's private browser/submenu dispatch off the runtime-dependent
  `RE::MenuEventHandler` vtable.
- Adapted text-input lease accounting and D3D11 render-target access to the
  CommonLibSSE-NG 7 interfaces.

### Safety

- Verify the expected relative or RIP-relative call instruction before
  installing the Present, shutdown, or optional native-language hooks. An
  unknown Skyrim layout is now logged and refused instead of being patched.
- Added compile-time checks requiring Address Library v5 metadata and updated
  post-1.6.629 structure declarations. Meridian intentionally does not claim
  Recast-style structure independence.
- Package CommonLib's GPL-3.0-or-later license, modding/linking exceptions,
  legacy MIT notice, and the MinHook hde64 license with release artifacts.

### Repository

- Rewrote the project README for public installation, integration, build,
  testing, security, attribution, and licensing use.
- Removed private planning material and the maintainer-specific deployment
  helper from the publishable source tree.
- Replaced tracked signing account configuration with an ignored local file,
  a public placeholder template, and environment-driven signing validation.
- Disabled release signing by default for public source builds. Maintainers can
  opt in explicitly after providing their own signing configuration.

### Candidate status

- The clean unsigned Release build, production manifest, and all 48 automated
  tests pass in the isolated candidate directory.
- Skyrim AE 1.6.1170 in-game validation passed with the deployed unsigned
  1.2.0 candidate.
- The owner confirmed that focused Shift+O now types in Tailor without opening
  Open Animation Replacer 3.1.5 on the post-plugin-load outer-guard candidate.
- The owner confirmed that the SE 1.5.97 Tailor RingBuffer candidate remains
  continuously visible without requiring mouse movement.
- Skyrim 1.7.104 with SKSE 2.3.1 and Address Library 13 is **NOT RUN**.
- SkyrimUpscaler Build 14 with DLSS Neural Reconstruction passed on AE
  1.6.1170 using the opt-in `BeforeRendererEnd` compositor timing.
- The broader SE cross-feature matrix is **NOT RUN**. VR remains deferred.

## 1.1.2 - 2026-09-02

### Fixed

- Deliver keyboard events to Chromium without requiring consumer-owned input
  hooks, restoring typing in search, filter, password, numeric, and naming
  fields.
- Detect focused text inputs, textareas, and editable content centrally for
  every `Meridian.View/1` consumer.
- Own one generation-guarded, balanced Skyrim text-entry lease and release it
  on blur, navigation, focus loss, hide, destruction, or shutdown without
  disturbing another menu or mod's `ControlMap` count.

### Changed

- Consumer mods now keep only their page-specific Escape behavior. Tailor,
  Horde, and Romantasy no longer need competing `PollInputDevices` hooks or
  duplicate focus callbacks.

### Release status

- Automated build, test, signing, and deployment proof is recorded with the
  release handoff.
- The owner confirmed the deployed Meridian 1.1.2 build in-game with Tailor,
  Horde, and Romantasy: all three interfaces render, accept keyboard input,
  and close without changing the player's run/walk state.
- SE 1.5.97 remains a separate **NOT RUN** gate. VR and DLSS 5 compatibility
  remain deferred.

## 1.1.1 - 2026-09-02

### Fixed

- Probe shared keyed-texture support end-to-end on Skyrim's active D3D11
  adapter before selecting RingBuffer. Automatically use the existing
  geometry-aware SyncCopy renderer when the adapter rejects that transport,
  preventing focused but invisible browser views.
- Include exact HRESULT values in every frame-ring initialization diagnostic.
- Preserve the player's exact run/walk setting across a Meridian focus session
  and restore it only after the engine reports that FocusMenu actually closed.
- Allow only releases of keyboard keys that were already held when focus began
  to continue to Skyrim, preventing modifier-based UI hotkeys from leaving the
  engine's input state stuck.

### Release status

- Clean SE/AE production and optional consumer builds pass.
- All 45 signed-release CTest gates pass, including manifest and signature checks.
- Horde, Romantasy, and Tailor in-game retesting is **NOT RUN**.
- Skyrim VR and DLSS 5 compatibility remain deferred.

## 1.1.0 - 2026-09-02

### Added

- `Meridian.View/1`, `Meridian.RenderLayer/1`, `Meridian.NifView/1`, and
  `Meridian.NifScene/1` through `/4` public extension interfaces.
- Native NIF preview support for static and bounded skinned meshes, material
  textures, studio lighting, camera controls, weighted scenes, armor records,
  equipped outfits, and live-reference appearance foundations.
- Public SDK headers, consumer guides, runtime checklists, and complete direct
  and transitive third-party license notices in the release package.
- Reproducible manifest validation, whole-tree deployment pruning, and signing
  verification for every shipped DLL and executable.

### Fixed

- Rejected malformed public API pointers and inconsistent event-binding replay.
- Made unexpected CEF browser closure release focus, input, cursor, and promise
  state without dereferencing a closed browser.
- Added explicit cross-device GPU ownership for the frame ring and removed
  unsynchronized dimension and handoff state.
- Fixed missing mouse-leave events, stale/reused cursor handles, custom cursor
  clipping, and focus-scoped cursor arbitration with PrismaUI.
- Closed COM lifetime leaks and exception cleanup gaps in the render path.
- Honored `nativeMenuLangSwitching=false` instead of always enabling its input
  event sink.
- Restricted release browsers to a pinned `mod://` host. Development-only
  `AllowRemoteContent` permits HTTP/HTTPS only and never exposes native bindings.

### Release status

- Clean SE/AE production and optional consumer builds pass.
- All 43 signed-release CTest gates pass, including manifest and signature checks.
- Fresh AE 1.6.1170 and SE 1.5.97 in-game regression gates remain **NOT RUN**.
- Skyrim VR and DLSS 5 compatibility remain deferred.

## 1.0.0 - 2026-08-20

- First MeridianUI release baseline with independent naming, SE/AE runtime
  targeting, browser focus/input support, and PrismaUI coexistence work.
