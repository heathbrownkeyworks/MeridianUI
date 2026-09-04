# Third-Party Notices

Meridian UI redistributes or links components from the projects below. The
complete license text supplied by each installed package is included in the
adjacent `licenses` directory of the release.

- Chromium Embedded Framework (`cef-prebuilt`)
- CommonLibSSE-NG 7.0.0 (GPL-3.0-or-later with its Modding Exception and
  GPL-3.0 Linking Exception; its composite notice also includes the legacy MIT
  terms and MinHook hde64 BSD-2-Clause terms)
- DirectXMath
- DirectX Tool Kit for DirectX 11
- {fmt}
- nlohmann/json
- PalSigslot
- rapidcsv
- SimpleIni
- spdlog
- toml11
- Xbyak

Meridian UI is derived from NirnLabUIPlatform by kkEngine. Its MIT attribution
is retained in `LICENSES/MIT.txt`. Meridian's independently reusable public API
headers are also available under that MIT grant. Meridian's native
implementation and binaries are licensed as described in `LICENSING.md`.

Meridian's native DLLs statically link CommonLibSSE-NG. Corresponding source
for the exact CommonLib revision is available from
`https://github.com/alandtse/CommonLibSSE-NG/tree/8b032fa992750d654d6d38a33731714d8b86be1f`;
Meridian source and build files are available from the project repository
named in `README.md`. The complete CommonLib license and exception texts are
shipped as `licenses/commonlibsse-ng.txt`.
