# MeridianUI CEF Process Lifecycle Gate

This gate separates automated/build proof from the required in-game proof. A successful multi-target build is not evidence that either Skyrim runtime passed.

## Automated proof

From the repository root:

```powershell
cmake -S . -B build-lifecycle -G "Visual Studio 17 2022" -A x64 `
  -DBUILD_TESTING=ON -DMERIDIAN_ENABLE_SIGNING=OFF
cmake --build build-lifecycle --config Debug --parallel 8
ctest --test-dir build-lifecycle -C Debug --output-on-failure
cmake --build build-lifecycle --config MinSizeRel --parallel 8
ctest --test-dir build-lifecycle -C MinSizeRel --output-on-failure
cmake --build build-lifecycle --config RelWithDebInfo --parallel 8
ctest --test-dir build-lifecycle -C RelWithDebInfo --output-on-failure
```

Required results:

- `ProcessDownDetectorTests` passes.
- `MeridianCEFSubprocess.exe`, `MeridianUI.dll`, `MeridianUIPlugin.dll`, and `MeridianUITest.dll` build.
- `Data/MeridianUI/tests/_testLocalPage.html` exists in the distribution tree.
- `rg -n "CefShutdown" src/CEFSubprocess` returns no matches.
- `git diff --check` reports no whitespace errors.

These checks do not launch CEF or Skyrim. Browser callbacks, process stability, and shutdown ordering remain runtime gates below.

## Test profile

Use a clean MO2 profile containing only:

- SKSE for the selected runtime.
- The matching Address Library.
- The newly built MeridianUI distribution.
- `MeridianUITest.dll` and its packaged offline HTML fixture.

Do not enable Tailor or any unrelated mod for the lifecycle gate. Do not navigate to Google, YouTube, or any other network origin.

Record before every run:

- Skyrim runtime and SKSE version.
- SHA-256 hashes of `MeridianUI.dll` and `MeridianCEFSubprocess.exe`.
- Start/end timestamps.
- Complete helper command lines and `--type` values.
- Windows Application Error events for `MeridianCEFSubprocess.exe`.

The fixture automatically releases its first fully loaded browser, recreates it once, and logs `warm browser release/recreate complete; replacement loaded and retained for exit drain`. It then retains the replacement so normal game exit must exercise the separate platform-wide drain path.

## Per-runtime scenarios

Run the same scenarios on AE 1.6.1170 with SKSE 2.2.6 and SE 1.5.97 with SKSE 2.0.20.

1. **Warm release/recreate:** wait for the fixture's completion log. The released browser must reach `OnBeforeClose`, and the replacement must load without helper churn.
2. **Five-minute idle:** after the warm release/recreate, no new helper births, abnormal exits, or renderer-termination callbacks occur.
3. **One renderer kill:** exactly one renderer-termination event is recorded; at most one replacement appears; no continuing churn follows.
4. **20 launch/exit trials:** ten normal Quit to Desktop, five console `qqq`, and five forced Skyrim parent terminations.

## Acceptance criteria

- Every normal exit logs the browser-drain barrier before `CefShutdown()` begins.
- The retained replacement reaches `OnBeforeClose` before the browser-drain barrier completes.
- `CefShutdown()` runs once, on the same application thread that initialized CEF.
- No `MeridianCEFSubprocess.exe` remains ten seconds after Skyrim exits.
- No helper is created after the Skyrim parent has exited.
- No `0xc0000409` or other Meridian helper Application Error occurs.
- Renderer recovery is single and bounded rather than a sequence of short-lived sibling PIDs.
- The identical multi-target DLL hashes pass both SE and AE.

## Current status

| Gate | Status | Evidence |
|---|---|---|
| Process monitor unit tests | PASS | Debug 25/25, MinSizeRel 10/10, RelWithDebInfo 25/25 repeated CTest runs |
| Debug full build | PASS | All four product targets built |
| MinSizeRel full build | PASS | All targets built; helper staged beside CEF runtime |
| RelWithDebInfo clean full build | PASS | All four product targets rebuilt from clean configuration state |
| Bare helper smoke | PASS | Missing parent switch returned `-1`; zero helper processes before and after |
| Final lifecycle code review | PASS | Two independent reviews found no remaining actionable defect |
| AE 1.6.1170 in-game lifecycle | PASS | 2026-08-18, RelWithDebInfo, SKSE 2.2.6. Validated on a full modded profile: warm release/recreate completed; five-minute idle kept a stable helper set; one bounded renderer recovery produced exactly one `renderer terminated` log entry; and 20/20 launch/exit trials completed with no surviving helpers. The Application event log sweep was clean. |
| SE 1.5.97 in-game lifecycle | NOT RUN | Blocked — no verified local SE 1.5.97 executable available. See Phase 0 plan Task 2. |

Current RelWithDebInfo artifact hashes (build `18f4674`, 2026-08-18):

| Artifact | SHA-256 |
|---|---|
| `MeridianUI.dll` | `186206E2398A47891D12A50CA2E6D452006F2B44B9EF85B912D2309A827D388C` |
| `MeridianCEFSubprocess.exe` | `711B92575F976B56E3BF86990DB018649E8F9456567B5C1C3F4C478FB514EA86` |
| `MeridianUIPlugin.dll` | `68954B5D48FDDC52DE7A242A1D16DD81C60915C4F472AE4B51DEBDFC819DE9FF` |
| `MeridianUITest.dll` | `AE7C0C523656539C42C4FD038B6AD2278157CAA89E8A5EDEF3E6CBC83ACC2E5A` |

**Publication policy (owner decision, 2026-08-18):** MeridianUI 1.0 ships **AE-verified**.
The AE row above is the release evidence. SE 1.5.97 remains explicitly **unverified in-game**
— it compiles and passes unit tests via CommonLibSSE-NG runtime detection, but no SE
lifecycle gate has run because no verified SE executable is available. Release documentation
must state this plainly. SE verification lands in a 1.0.x release if and when an SE runtime
becomes available; until then, do not claim SE in-game verification anywhere.
