# MeridianCEFSubprocess -- the CEF subprocess executable.
# Included from the top-level CMakeLists.txt. Requires the caller to have
# already defined: the `cef` INTERFACE library, the `CefLibraryFiles` target,
# the `set_compile_options()` and `sign_output()` functions, and the
# NL_UI_PATH / SKSE_PLUGIN_PATH variables.

set(CEFSubprocessProjectName "MeridianCEFSubprocess")

file(
    GLOB_RECURSE
    CEFSubprocessProjectName_src 
        CONFIGURE_DEPENDS
        "src/CEFSubprocess/**.h"
        "src/CEFSubprocess/**.hpp"
        "src/CEFSubprocess/**.cpp"
)
source_group(
    TREE "${CMAKE_CURRENT_SOURCE_DIR}/src/CEFSubprocess"
    FILES ${CEFSubprocessProjectName_src}
)

add_executable(
    ${CEFSubprocessProjectName}
    WIN32
        ${CEFSubprocessProjectName_src}
        ${CMAKE_CURRENT_BINARY_DIR}/version.rc
)

target_include_directories(
    ${CEFSubprocessProjectName}
    PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/CEFSubprocess
        # Only for the shared Scheme/ModSchemeHandler.h RegisterModScheme() helper
        # (see the CAUTION comment in that header) -- keeps both processes'
        # OnRegisterCustomSchemes() registering the "mod" scheme with identical
        # option flags from one definition instead of a duplicated copy.
        ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
)

target_link_libraries(
    ${CEFSubprocessProjectName}
    PUBLIC
        cef
        spdlog::spdlog
)

set_compile_options(${CEFSubprocessProjectName})

target_precompile_headers(
    ${CEFSubprocessProjectName}
    PRIVATE
        "src/CEFSubprocess/PCH.h"
)

set_target_properties(
    ${CEFSubprocessProjectName}
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_DEBUG ${NL_UI_PATH}
        RUNTIME_OUTPUT_DIRECTORY_RELEASE ${NL_UI_PATH}
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${NL_UI_PATH}
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${NL_UI_PATH}
        LIBRARY_OUTPUT_DIRECTORY_DEBUG ${NL_UI_PATH}
        LIBRARY_OUTPUT_DIRECTORY_RELEASE ${NL_UI_PATH}
        LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO ${NL_UI_PATH}
        LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL ${NL_UI_PATH}
)

target_compile_definitions(
    ${CEFSubprocessProjectName}
    PRIVATE
        CEF_SUBPROCESS_PROJECT_NAME="${CEFSubprocessProjectName}"
)

add_dependencies(${CEFSubprocessProjectName} CefLibraryFiles)
sign_output(${CEFSubprocessProjectName})
