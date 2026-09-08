# MeridianNifTest -- the standalone NIF camera-control SKSE consumer
# (built when MERIDIAN_BUILD_NIF_TEST is ON).
# Included from the top-level CMakeLists.txt. Requires the caller to have
# already defined the `set_compile_options()` and `sign_output()` functions.

if(BUILD_AS_SHARED AND MERIDIAN_BUILD_NIF_TEST)
    set(NifTestProjectName "MeridianNifTest")

    file(
        GLOB_RECURSE
        NifTestProjectName_src
            CONFIGURE_DEPENDS
            "src/NifTest/**.h"
            "src/NifTest/**.hpp"
            "src/NifTest/**.cpp"
            "src/NifTest/**.rc"
    )
    source_group(
        TREE "${CMAKE_CURRENT_SOURCE_DIR}/src/NifTest"
        FILES ${NifTestProjectName_src}
    )

    add_library(${NifTestProjectName} SHARED ${NifTestProjectName_src})
    set(NIF_TEST_OUTPUT_ROOT "${CMAKE_BINARY_DIR}/nif-test")
    set_target_properties(
        ${NifTestProjectName}
        PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY_DEBUG "${NIF_TEST_OUTPUT_ROOT}/Debug/SKSE/Plugins"
            RUNTIME_OUTPUT_DIRECTORY_RELEASE "${NIF_TEST_OUTPUT_ROOT}/Release/SKSE/Plugins"
            RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${NIF_TEST_OUTPUT_ROOT}/RelWithDebInfo/SKSE/Plugins"
            RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${NIF_TEST_OUTPUT_ROOT}/MinSizeRel/SKSE/Plugins"
            LIBRARY_OUTPUT_DIRECTORY_DEBUG "${NIF_TEST_OUTPUT_ROOT}/Debug/SKSE/Plugins"
            LIBRARY_OUTPUT_DIRECTORY_RELEASE "${NIF_TEST_OUTPUT_ROOT}/Release/SKSE/Plugins"
            LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${NIF_TEST_OUTPUT_ROOT}/RelWithDebInfo/SKSE/Plugins"
            LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL "${NIF_TEST_OUTPUT_ROOT}/MinSizeRel/SKSE/Plugins"
    )
    target_include_directories(
        ${NifTestProjectName}
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
            ${CMAKE_CURRENT_SOURCE_DIR}/src/NifTest
    )
    target_link_libraries(${NifTestProjectName} PUBLIC CommonLibSSE::CommonLibSSE)
    set_compile_options(${NifTestProjectName})
    target_precompile_headers(${NifTestProjectName} PRIVATE "src/NifTest/PCH.h")

    add_custom_command(
        TARGET ${NifTestProjectName} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
                "${NIF_TEST_OUTPUT_ROOT}/$<CONFIG>/MeridianUI/meridianniftest"
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/src/NifTest/web"
                "${NIF_TEST_OUTPUT_ROOT}/$<CONFIG>/MeridianUI/meridianniftest"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/src/NifTest/fixture/MeridianNifTest.esp"
                "${NIF_TEST_OUTPUT_ROOT}/$<CONFIG>/MeridianNifTest.esp"
        COMMENT "Staging standalone NIF camera-control view..."
        VERBATIM
    )
    sign_output(${NifTestProjectName})

    if(DEFINED MERIDIAN_NIF_TEST_DEPLOY_PATH)
        add_custom_target(
            DeployNifTestMod ALL
            COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
                    "${NIF_TEST_OUTPUT_ROOT}/$<CONFIG>"
                    "${MERIDIAN_NIF_TEST_DEPLOY_PATH}"
            DEPENDS ${NifTestProjectName}
            COMMENT "Deploying standalone NIF test mod: ${MERIDIAN_NIF_TEST_DEPLOY_PATH}"
            VERBATIM
        )
    endif()
endif()
