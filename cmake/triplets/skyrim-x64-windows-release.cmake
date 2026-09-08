###
# vcpkg triplet for Skyrim x64 Windows release build
###

# Set architecture to x64
set(VCPKG_TARGET_ARCHITECTURE x64)

# Set MSVC runtime to static for release builds
set(VCPKG_CRT_LINKAGE static)

# Set library linkage to static for release builds
set(VCPKG_LIBRARY_LINKAGE static)

# Set vcpkg build type to release
set(VCPKG_BUILD_TYPE release)

# FFmpeg is loaded via Windows delay-load; build as shared DLLs
if(PORT STREQUAL "cef-prebuilt")
    set(VCPKG_CRT_LINKAGE dynamic)
    set(VCPKG_LIBRARY_LINKAGE dynamic)
endif()
