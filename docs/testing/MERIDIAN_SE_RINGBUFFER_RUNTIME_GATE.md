# Meridian SE RingBuffer Retained-Frame Runtime Gate

Automated tests prove the DXGI keyed-mutex transition policy and repeated
consumption of a retained frame on the development GPU. They do not prove the
candidate inside Skyrim SE 1.5.97 on the tester's adapter and driver.

## Submitted failure evidence

The tester's 2026-09-03 Skyrim SE 1.5.97 session opened Tailor successfully,
but its browser surface flickered and was visible only while the mouse moved.
The supplied logs show that the Meridian view reached DOM ready and selected
the RingBuffer renderer without logging a transport failure:

- `E:/Downloads/Test/MeridianUI.log` SHA-256:
  `4CF06FE8811711D7BE7162B8EEB01224F5681C3FF1BA9D5B41A7D1071DF13E2`
- `E:/Downloads/Test/Tailor.log` SHA-256:
  `6011297385E0FA4E3C289E1ABDF8E756DB569AEA40A1D410415C7EFE43827A31`

The previous consumer acquired key 1 only when CEF published a new paint. It
then released key 0 after drawing, but subsequent Skyrim presents sampled the
same shared texture without reacquiring key 0. Mouse movement caused CEF hover
paints, repeatedly creating the one path that reacquired valid ownership.

## Status

| Gate | Status |
| --- | --- |
| Consumer key-transition policy test | PASS |
| Repeated retained-frame D3D11 integration test | PASS |
| Complete isolated candidate CTest suite | PASS — 48/48 |
| Skyrim SE 1.5.97 Tailor flicker retest | PASS — owner confirmed 2026-09-03 |
| Skyrim AE regression retest | **NOT RUN** |

## Confirmed result

The owner confirmed on 2026-09-03 that the candidate fixes the reported Tailor
flicker/visibility failure on Skyrim SE 1.5.97. This passes the specific case
where the interface previously appeared only while the mouse was moving. The
broader SE feature matrix and the separate AE regression row remain open.

## SE 1.5.97 retest

1. Install the complete isolated candidate as one mod; do not mix its DLLs,
   subprocess, or CEF resources with another Meridian build.
2. Start Skyrim SE 1.5.97 through the tester's normal SKSE/MO2 profile.
3. Confirm `MeridianUI.log` reports the shared keyed-texture probe passed and
   Tailor selected the RingBuffer render layer.
4. Open Tailor and stop moving the mouse for at least 15 seconds. Confirm the
   interface remains continuously visible and stable.
5. Move the mouse across controls, open several Tailor screens, type into a
   text field, close Tailor, and reopen it. Confirm no flicker or stale frame.
6. Exit Skyrim and confirm no `MeridianCEFSubprocess.exe` remains.
7. Return the complete Meridian and Tailor logs plus runtime, SKSE, GPU, driver,
   and candidate SHA-256 details.

Do not mark the SE row passed until the above runtime evidence is captured.
