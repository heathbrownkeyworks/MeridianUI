# Meridian 1.3.0 dependency update runtime gate

This candidate updates CEF to 152.0.6 / Chromium 152.0.7977.83 and CommonLibSSE-NG
to 7.4.0. The public platform and extension API versions are unchanged. Production
support remains Skyrim SE and AE; enabling a dependency's VR switches is not a
substitute for Meridian's separate VR backend validation.

The original contribution is https://github.com/langfod/MeridianUI/pull/1,
commits `00ea448` and `e73778c`, by langfod. Integration corrections restore
Release compiler defaults, matching Debug dependencies, test failure reporting,
and the existing malformed-skin check. The integration also corrects the CEF
archive hash after checking the download against the official CEF index, and
keeps debug symbols outside the staged mod files.

## Automated checks

Validated on 2026-09-08 with MSVC 19.44 and Windows SDK 10.0.26100.0:

- Fresh vcpkg installation completed for the pinned dependency graph, including
  both static Debug and Release CEF wrapper/CommonLib configurations.
- Signed Release build: **54/54 CTest passed**.
- Debug build with the optional standalone NIF consumer: **53/53 CTest passed**.
- Build-script regression cases cover default, requested tests, explicit skips,
  configure/build/test failures, and fresh configuration.
- Generated Release compiler settings retain `/O2`, `NDEBUG`, static CRT, and
  link-time optimization; Debug uses its matching static debug CRT.
- All 12 staged DLL/EXE files passed publisher/signature verification. The three
  Meridian binaries report compiled version **1.3.0.0**; libcef reports
  **152.0.6+g708dc14+chromium-152.0.7977.83**.
- The production manifest contains the SDK and notices and excludes test
  fixtures, active user configuration, and debug symbols.

These checks do not exercise CEF inside Skyrim or prove the runtime items below.

## Runtime checks

| Check | Status | Evidence required |
| --- | --- | --- |
| SE 1.5.97 startup, menus, and close | NOT RUN | Exact candidate version, matching Meridian/CEF logs |
| AE startup, menus, and close | NOT RUN | Record the exact AE/SKSE versions separately |
| Ordinary and elevated MO2 | NOT RUN | No CEF code 38/retry; correct menu opening and clean close |
| Focused input, modifiers, and Alt-Tab | NOT RUN | Typing consumed once; hotkeys and key release remain correct |
| Horde/Tailor/browser surfaces | NOT RUN | Open, interact, hide, reopen, and exit without retained-frame flicker |
| NIF previews | NOT RUN | Rigid/skinned meshes display; malformed skin fails without a crash |
| HDR off/on | NOT RUN | Fresh launches, menu arrival and exit in both modes; exact HDR method and graphics stack |
| Helper termination | NOT RUN | Verify actual executable path/PID and whether Skyrim remains alive after closing |

A newer Chromium runtime may change compatibility, but this release does not
claim an HDR crash fix without a controlled test from the affected setup. Save
the full crash log, MeridianUI.log, and cef.log before relaunching. For an exit
hang, preserve the final log entries and actual process identity rather than
relying solely on MO2's displayed process label.
