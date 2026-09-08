# MeridianUI -- the main UI library (built as a SHARED or STATIC library
# depending on BUILD_AS_SHARED).
# Included from the top-level CMakeLists.txt. Requires the caller to have
# already defined: the `cef` INTERFACE library, the `MeridianNifMaterialShaders`
# target and `MERIDIAN_NIF_SHADER_OUTPUT_DIR` (NifMaterialShaders.cmake), the
# `CEFSubprocessProjectName` variable (CEFSubprocess.cmake), the
# `set_compile_options()` / `enable_lto_for_target()` / `sign_output()`
# functions, the `proj_stuff` file list, and the NL_UI_PATH / NL_UI_REL_PATH
# variables.

set(UIPlatformProjectName "MeridianUI")

file(
    GLOB_RECURSE
    UIPlatformProjectName_src
        CONFIGURE_DEPENDS
        "src/UIPlatform/**.h"
        "src/UIPlatform/**.hpp"
        "src/UIPlatform/**.cpp"
)
source_group(
    TREE "${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform"
    FILES ${UIPlatformProjectName_src}
)
list(APPEND UIPlatformProjectName_src ${proj_stuff})

# Build type
if(BUILD_AS_SHARED)
    add_library(${UIPlatformProjectName} SHARED ${UIPlatformProjectName_src})
    set_target_properties(
        ${UIPlatformProjectName}
        PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY_DEBUG ${NL_UI_PATH}
            RUNTIME_OUTPUT_DIRECTORY_RELEASE ${NL_UI_PATH}
            RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${NL_UI_PATH}
            RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${NL_UI_PATH}
            LIBRARY_OUTPUT_DIRECTORY_DEBUG ${NL_UI_PATH}
            LIBRARY_OUTPUT_DIRECTORY_RELEASE ${NL_UI_PATH}
            LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO ${NL_UI_PATH}
            LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL ${NL_UI_PATH}
            #ARCHIVE_OUTPUT_DIRECTORY ${NL_UI_PATH}/archive
            #PDB_OUTPUT_DIRECTORY ${NL_UI_PATH}
    )
else()
add_library(${UIPlatformProjectName} STATIC ${UIPlatformProjectName_src})
endif()
add_dependencies(${UIPlatformProjectName} MeridianNifMaterialShaders)

target_include_directories(
    ${UIPlatformProjectName}
    PUBLIC
        ${CMAKE_CURRENT_BINARY_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/CEFSubprocess
        ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    PRIVATE
        #${SIMPLEINI_INCLUDE_DIRS}
        ${MERIDIAN_NIF_SHADER_OUTPUT_DIR}
)

set_compile_options(${UIPlatformProjectName})

target_precompile_headers(
    ${UIPlatformProjectName}
    PRIVATE
        "src/UIPlatform/PCH.h"
)

# CursorRasterizer.cpp needs real GDI declarations (DrawIconEx, CreateDIBSection,
# CreateCompatibleDC, GdiFlush, et al.) to rasterize HCURSORs. PCH.h defines
# NOGDI project-wide, and wingdi.h's own include guard makes that exclusion
# permanent for the rest of a translation unit once it's set. It also pulls
# in CommonLibSSE-NG, which refuses to compile if a real Windows API header
# was included first. So this pure Win32 + D3D11 helper is excluded from the
# PCH and kept CommonLibSSE-NG-free, free to include a full, un-gated
# <Windows.h> itself. See CursorRasterizer.h for the full explanation.
set_source_files_properties(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform/Render/CursorRasterizer.cpp
    PROPERTIES
        SKIP_PRECOMPILE_HEADERS ON
)
enable_lto_for_target(${UIPlatformProjectName})

target_link_libraries(
    ${UIPlatformProjectName}
    PUBLIC
        CommonLibSSE::CommonLibSSE
        "d3d11.lib"
        "dxguid.lib"
        "gdi32.lib"
        Microsoft::DirectXTK
        cef
        nlohmann_json::nlohmann_json
        bcrypt.lib
)

target_compile_definitions(
    ${UIPlatformProjectName}
    PRIVATE
        NL_UI_REL_PATH="${NL_UI_REL_PATH}"
        NL_UI_SUBPROC_NAME="${CEFSubprocessProjectName}.exe"
        $<$<NOT:$<CONFIG:Release>>:MERIDIAN_PERF_PROBE=1>
)

if(BUILD_AS_SHARED)
    sign_output(${UIPlatformProjectName})
endif()
