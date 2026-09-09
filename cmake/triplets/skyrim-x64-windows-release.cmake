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

# The prebuilt CEF runtime remains a DLL; its static C++ wrapper uses the
# same static CRT as Meridian. Do not override the wrapper's CRT linkage.
