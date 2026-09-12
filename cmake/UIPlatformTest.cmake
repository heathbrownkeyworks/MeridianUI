# MeridianUITest -- the in-game MeridianUI lifecycle fixture
# (built when MERIDIAN_BUILD_FIXTURE is ON).
# Included from the top-level CMakeLists.txt. Requires the caller to have
# already defined: the `set_compile_options()` and `sign_output()` functions
# and the NL_UI_PATH / SKSE_PLUGIN_PATH variables.

if(BUILD_AS_SHARED AND MERIDIAN_BUILD_FIXTURE)
    set(UIPlatformTestName "MeridianUITest")

    file(
        GLOB_RECURSE
        UIPlatformTestName_src
            CONFIGURE_DEPENDS
            "src/UIPlatformTest/**.h"
            "src/UIPlatformTest/**.hpp"
            "src/UIPlatformTest/**.cpp"
    )
    source_group(
        TREE "${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatformTest"
        FILES ${UIPlatformTestName_src}
    )

    add_commonlibsse_plugin(
        ${UIPlatformTestName}
        SOURCES ${UIPlatformTestName_src}
        VERSION ${LIB_VERSION}
        AUTHOR "ColdSun"
        EMAIL ""
        USE_ADDRESS_LIBRARY
        MINIMUM_SKSE_VERSION "2.0.1.04"
    )
    set_target_properties(
        ${UIPlatformTestName}
        PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY_DEBUG ${SKSE_PLUGIN_PATH}
            RUNTIME_OUTPUT_DIRECTORY_RELEASE ${SKSE_PLUGIN_PATH}
            RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${SKSE_PLUGIN_PATH}
            RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${SKSE_PLUGIN_PATH}
            LIBRARY_OUTPUT_DIRECTORY_DEBUG ${SKSE_PLUGIN_PATH}
            LIBRARY_OUTPUT_DIRECTORY_RELEASE ${SKSE_PLUGIN_PATH}
            LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO ${SKSE_PLUGIN_PATH}
            LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL ${SKSE_PLUGIN_PATH}
    )

    target_include_directories(
        ${UIPlatformTestName}
        PUBLIC
            ${CMAKE_CURRENT_BINARY_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatformTest
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )

    set_compile_options(${UIPlatformTestName})

    target_precompile_headers(
        ${UIPlatformTestName}
        PRIVATE
            "src/UIPlatformTest/PCH.h"
    )

    add_custom_command(
        TARGET ${UIPlatformTestName} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${NL_UI_PATH}/tests"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatformTest/TestCases/_testLocalPage.html"
                "${NL_UI_PATH}/tests/_testLocalPage.html"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${NL_UI_PATH}/MeridianUITest"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatformTest/TestCases/_testLocalPage.html"
                "${NL_UI_PATH}/MeridianUITest/index.html"
        COMMENT "Copying offline MeridianUI lifecycle fixture..."
        VERBATIM
    )

    sign_output(${UIPlatformTestName})
endif()
