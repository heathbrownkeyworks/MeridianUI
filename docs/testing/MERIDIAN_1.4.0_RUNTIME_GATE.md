# Meridian 1.4.0 DXVK compatibility candidate

This candidate adds a CPU browser transport for Windows DXVK. Skyrim and Meridian's
browser composition still issue D3D11 commands through the game's device. DXVK
handles their Vulkan translation. Browser frames arrive through CEF `OnPaint`, are copied
before the callback returns, and are uploaded on the game's render thread without
shared-resource handles or keyed mutexes. Existing mod APIs remain unchanged.

## Selection and cost

`Data/SKSE/Plugins/MeridianUI.ini` accepts these optional settings:

```ini
[Compatibility]
BrowserTransport=Auto
CpuUploadFrameRate=30
```

- `Auto` identifies the actual device through DXVK's published interop interface.
  Windows DXVK selects `CpuUpload`; native D3D11 and Wine/Proton retain
  `SharedTexture`. A loaded DXVK DLL alone does not select the new path.
- `SharedTexture` and `CpuUpload` explicitly override detection. Restart Skyrim
  after changing either setting. Existing `[General] RendererType` applies only
  to SharedTexture. CPU mode ignores it.
- `CpuUploadFrameRate` accepts 1 through 60, default 30. A mod's lower frame-rate
  request still applies. This limits browser paints, not Skyrim's frame rate.
- CPU mode adds CPU work and memory bandwidth. One retained image and one dirty
  snapshot per view/popup replace an unbounded queue. Each full image is limited
  to 128 MiB and each dimension to 16,384 pixels. BGRA premultiplied alpha is
  converted for Meridian's existing non-premultiplied compositor.
- There is no automatic switch after a running shared-texture browser fails.
  Force CpuUpload and restart for a controlled comparison in that situation.

DXVK must already control Skyrim's device creation. Meridian does not install or
inject DXVK. Keep its wrapper DLLs in the game setup prescribed by DXVK, outside
Meridian's CEF helper directory. The helper can use native D3D11 while the game
uses DXVK because CPU pixel transfer crosses that boundary.

The built-in NIF preview APIs use a separate private device and shared-texture
transport. They remain unavailable on Windows DXVK in this candidate and log
that limitation. Native D3D11 keeps the private device, including when browser
CpuUpload is forced. Consumer-defined native render layers need their own DXVK
validation; this browser change does not establish their compatibility.

References: [DXVK shared-resource support](https://github.com/doitsujin/dxvk/wiki/Feature-support#shared-resources),
[DXVK Windows guidance](https://github.com/doitsujin/dxvk/wiki/Windows),
[DXVK interop interface](https://github.com/doitsujin/dxvk/blob/v3.1/src/dxgi/dxgi_interfaces.h),
[CEF 152 paint contract](https://github.com/chromiumembedded/cef/blob/708dc140cbc3286826a8abef89dc23a44ff9ea72/include/cef_render_handler.h).

## Standalone evidence

Validated on Windows, 2026-09-09, with MSVC 19.44 / SDK 10.0.26100.0, the pinned
CEF 152 / CommonLibSSE-NG 7.4 dependencies, DXVK 3.1 and NVIDIA RTX 4090:

- Release build: **55/55 CTest passed**. Debug build, including the optional
  standalone NIF consumer: **55/55 CTest passed**. Matching static Release and
  Debug dependencies were reused from the verified 1.3.0 dependency installation.
- All three compiled Meridian binaries report **1.4.0.0**. Signing and deployment
  receipts are recorded with the candidate artifacts separately.
- Policy and INI tests cover automatic selection, explicit overrides, Wine/Proton
  preservation, and frame-rate validation.
- CPU buffer tests cover ownership, dirty-region merging, clipping, transparency,
  full refresh on resize, bounded allocation, reset/shutdown, and concurrent
  producer/consumer resizing.
- `CpuTextureSurfaceTests` passed on native D3D11 WARP and actual DXVK. Readback
  checks cover BGRA colors, alpha, partial updates, resize, popup independence,
  retained frames, viewport preservation, and shutdown. Textures have no shared flags.
- The opt-in `CefCpuSmokeTests` passed on native D3D11 and DXVK with a separate
  native CEF helper directory. Real browser paints, transparency, JavaScript
  updates, resize, browser close, and `CefShutdown` passed. No fixture process
  remained after either successful run.
- An initial fixture that put DXVK beside the CEF helper retried after GPU helper
  crashes before producing pixels. That layout is not the validated configuration.

To reproduce the upload test, build with `BUILD_TESTING=ON`, run
`CpuTextureSurfaceTests`, then copy that executable plus the official x64 DXVK
`d3d11.dll` and `dxgi.dll` into an isolated directory and run it with `--expect-dxvk`.
Never copy test DXVK DLLs into the live mod.

For real CEF smoke testing, additionally configure `MERIDIAN_BUILD_CEF_CPU_SMOKE=ON`
and build `CefCpuSmokeTests`. Copy its matching CEF runtime DLLs/resources/locales
beside the executable and run from that directory. For `--expect-dxvk`, add the
two DXVK DLLs there and place the same fixture executable and CEF runtime in a
`helper` subdirectory without DXVK DLLs. Use a fresh directory for each run.
The optional fixture is excluded from production staging and ordinary CTest.

## Skyrim runtime gate

Standalone proof does not exercise Skyrim hooks, mod menus, or graphics injectors.
Record exact Skyrim/SKSE/Meridian/DXVK versions and GPU/driver with each result.

| Check | Status | Evidence |
| --- | --- | --- |
| Native D3D11, RingBuffer and SyncCopy | NOT RUN | Existing menus retain correct colors, input, and close behavior |
| Windows DXVK, Auto | NOT RUN | Log selects CpuUpload with correct device module/adapter; open Horde and Tailor |
| Forced CpuUpload on native D3D11 | NOT RUN | Compare the same menus, transparency, popups, and animation |
| Multiple menus, hide/reopen, geometry/resize | NOT RUN | No stale frames, stretched popup, or cross-view corruption |
| Built-in NIF previews under Windows DXVK | UNSUPPORTED | Separate NIF transport work is required |
| Native D3D11 NIF previews with forced CpuUpload | NOT RUN | Browser transport selection must not disable the private preview device |
| SE 1.5.97 and AE | NOT RUN | Record separately; earlier releases do not validate this candidate |
| Ordinary and elevated MO2 | NOT RUN | Menus open without code 38 or startup retry |
| Alt-Tab and focused input | NOT RUN | No leaked keys, stuck modifiers, flicker, or lost focus |
| HDR off/on | NOT RUN | Fresh launch, menu arrival, and exit; record exact HDR method |
| Normal exit and crash exit | NOT RUN | Skyrim and Meridian helper PIDs terminate; MO2 unlocks |
| Wine/Proton | NOT RUN | Auto keeps SharedTexture; any CpuUpload result recorded separately |

This candidate does not establish an HDR crash fix. Keep DLSS/ENB/ReShade/frame
generation combinations as separate checks. Preserve complete crash and CEF logs
for failures, and use the signed 1.3.0 backup for a controlled rollback comparison.
