# Meridian UI Licensing

Copyright (c) 2026 ColdSun.

Meridian UI is distributed as `GPL-3.0-or-later` with the additional permissions in [EXCEPTIONS.md](EXCEPTIONS.md). This license covers the native Meridian UI implementation and the compiled DLL and EXE files distributed as part of Meridian UI.

Meridian UI statically links CommonLibSSE-NG. A compiled native plugin containing CommonLibSSE-NG is a combined work and is not distributed as MIT-only software. The complete GNU GPL version 3 text is in [LICENSE](LICENSE). CommonLibSSE-NG and its own exceptions remain available at the exact revision recorded by the build overlay.

## MIT-licensed portions

Meridian UI is derived from the MIT-licensed NirnLabUIPlatform project by kkEngine. The original MIT permission and attribution are retained in [LICENSES/MIT.txt](LICENSES/MIT.txt).

The standalone headers in `src/UIPlatform/MeridianUIAPI/`, and the copies shipped in `MeridianUI/SDK/MeridianUIAPI/`, are separately available under the MIT License in that directory. This permissive grant applies only to the API header code. It does not relicense Meridian UI's implementation, CommonLibSSE-NG, or dependencies that an API consumer chooses to link.

## Third-party material

Third-party components retain their respective licenses. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and the `licenses/` directory included with release packages.

## Corresponding source

The preferred form for modifying Meridian UI is this repository, including its CMake files and vcpkg overlays. The CommonLibSSE-NG revision used by release builds is pinned in `overlay_ports/commonlibsse-ng/portfile.cmake`. Release documentation identifies that revision and stages the applicable license texts with the binaries.

Binary release packages must include [LICENSE](LICENSE), [EXCEPTIONS.md](EXCEPTIONS.md), this file, [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md), and the applicable API and third-party license texts. Corresponding Source for the exact release, including its build files and pinned CommonLibSSE-NG revision, must accompany the binaries or be made available using a method permitted by GNU GPL version 3 section 6.
