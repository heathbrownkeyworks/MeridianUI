vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO alandtse/CommonLibSSE-NG
    REF 8b032fa992750d654d6d38a33731714d8b86be1f
    SHA512 9def8a8e954fca898f0c57481a5b6415ea53481b59ff66b5982bb1f63cb847aacf2b85ab021a7e26cdb9df7689534621e5c81fb43477fea1d3135e27a44391b6
    HEAD_REF ng
)

# CommonLib's patch-safety option compiles MinHook's three-file hde64 decoder.
# vcpkg configures ports with FetchContent disconnected, so provide that source
# explicitly instead of allowing CommonLib's nested FetchContent download.
vcpkg_from_github(
    OUT_SOURCE_PATH HDE64_SOURCE_PATH
    REPO TsudaKageyu/minhook
    REF v1.3.4
    SHA512 8a33233598b56ad9da44d22d470c2432f68364dac31bc719fcd6b085e681fa10ddd41865fbde056ee7f4e7a075cc135344b6bf444eadbd7e7314ee1bedfd89b5
    HEAD_REF master
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DBUILD_TESTS=off
        -DSKSE_SUPPORT_XBYAK=on
        -DSKSE_SUPPORT_PATCH_SAFETY=on
        -DENABLE_SKYRIM_SE=on
        -DENABLE_SKYRIM_AE=on
        -DENABLE_SKYRIM_VR=off
        "-DFETCHCONTENT_SOURCE_DIR_HDE64=${HDE64_SOURCE_PATH}"
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME CommonLibSSE CONFIG_PATH lib/cmake/CommonLibSSE)
vcpkg_copy_pdbs()

# Upstream exports its OpenVR library path behind a false generator expression
# even when VR is disabled. Remove that dead expression so the SE/AE package is
# relocatable and does not retain an absolute vcpkg buildtree path.
vcpkg_replace_string(
    "${CURRENT_PACKAGES_DIR}/share/CommonLibSSE/CommonLibSSE-targets.cmake"
    "${SOURCE_PATH}/extern/openvr/lib/win64/openvr_api.lib"
    "openvr_api.lib"
)

# CommonLibSSEConfig.cmake includes this helper module (add_commonlibsse_plugin etc.)
file(INSTALL "${SOURCE_PATH}/cmake/CommonLibSSE.cmake" DESTINATION "${CURRENT_PACKAGES_DIR}/share/CommonLibSSE")

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")

vcpkg_install_copyright(
    FILE_LIST
        "${SOURCE_PATH}/COPYING"
        "${SOURCE_PATH}/EXCEPTIONS.md"
        "${SOURCE_PATH}/licenses/LICENSE-MIT"
        "${HDE64_SOURCE_PATH}/LICENSE.txt"
)
