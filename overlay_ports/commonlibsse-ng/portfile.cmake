#vcpkg_from_github(
#    OUT_SOURCE_PATH SOURCE_PATH
#    REPO alandtse/CommonLibSSE-NG
#    REF 8b032fa992750d654d6d38a33731714d8b86be1f
#    SHA512 9def8a8e954fca898f0c57481a5b6415ea53481b59ff66b5982bb1f63cb847aacf2b85ab021a7e26cdb9df7689534621e5c81fb43477fea1d3135e27a44391b6
#    HEAD_REF ng
#)
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO alandtse/CommonLibSSE-NG
    REF 9b17b42fc9db23aea2b60f92690e778784e612b1
    SHA512 25a4c65ebd3c2d34a3e4a8fd1625e73a15563d4af467eb984360381e36bde69fd8abe0225f4a3fda88ede6fd533e5a5fb3fd272fbeedf54e047f8f7fbed548c4
    HEAD_REF ng
)
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH2
    REPO ValveSoftware/openvr
    REF 60eb187801956ad277f1cae6680e3a410ee0873b
    SHA512 bb85b4705e7095ac65df9969112b2df8930cee7917cc5f14231c5a0ffeed7a73ffa60727fd32f8786a403656f95a3ec0f80bf3ceabc5b8ede964aefb920bc718
    HEAD_REF master
)

file(GLOB OPENVR_FILES "${SOURCE_PATH2}/*")

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

# CommonLib's build expects the OpenVR submodule to exist at extern/openvr in the
# source tree (CMakeLists.txt adds ${CMAKE_CURRENT_SOURCE_DIR}/extern/openvr/headers).
# vcpkg only fetched the standalone openvr repo into SOURCE_PATH2, so copy it into the
# CommonLib checkout before configure runs. This is what avoids the missing openvr.h error.
file(MAKE_DIRECTORY "${SOURCE_PATH}/extern/openvr")
file(COPY "${SOURCE_PATH2}/headers" "${SOURCE_PATH2}/lib"
    DESTINATION "${SOURCE_PATH}/extern/openvr")

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DBUILD_TESTS=off
        -DSKSE_SUPPORT_XBYAK=off
        -DSKSE_SUPPORT_PATCH_SAFETY=on
        -DENABLE_SKYRIM_SE=on
        -DENABLE_SKYRIM_AE=on
        -DENABLE_SKYRIM_VR=off
        "-DFETCHCONTENT_SOURCE_DIR_HDE64=${HDE64_SOURCE_PATH}"
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME CommonLibSSE CONFIG_PATH lib/cmake/CommonLibSSE)

# CommonLibSSEConfig.cmake includes this helper module (add_commonlibsse_plugin etc.)
file(INSTALL "${SOURCE_PATH2}/headers/openvr.h" DESTINATION ${CURRENT_PACKAGES_DIR}/include)
file(INSTALL "${SOURCE_PATH}/cmake/CommonLibSSE.cmake" DESTINATION "${CURRENT_PACKAGES_DIR}/share/CommonLibSSE")

# Install openvr_api.lib so its path in the exported targets file is relocatable
file(INSTALL "${SOURCE_PATH2}/lib/win64/openvr_api.lib" DESTINATION "${CURRENT_PACKAGES_DIR}/lib")

# Patch the absolute buildtrees path to openvr_api.lib that CMake bakes into the exported targets
set(_openvr_repl [[:${_IMPORT_PREFIX}/lib/openvr_api.lib]])
file(READ "${CURRENT_PACKAGES_DIR}/share/CommonLibSSE/CommonLibSSE-targets.cmake" _targets)
string(REGEX REPLACE
    ":[A-Za-z]:[^;\"<>]*/openvr_api\\.lib"
    "${_openvr_repl}"
    _targets "${_targets}")
file(WRITE "${CURRENT_PACKAGES_DIR}/share/CommonLibSSE/CommonLibSSE-targets.cmake" "${_targets}")

file(REMOVE_RECURSE
    "${CURRENT_PACKAGES_DIR}/debug/include"
    "${CURRENT_PACKAGES_DIR}/debug/share"
)

vcpkg_install_copyright(
    FILE_LIST
        "${SOURCE_PATH}/COPYING"
        "${SOURCE_PATH}/EXCEPTIONS.md"
        "${SOURCE_PATH}/licenses/LICENSE-MIT"
        "${HDE64_SOURCE_PATH}/LICENSE.txt"
)
