# MeridianUIPlugin -- the SKSE plugin that loads the MeridianUI library.
# Only built when BUILD_AS_SHARED is ON.
# Included from the top-level CMakeLists.txt after UIPlatform.cmake. Requires
# the caller to have already defined: the `UIPlatformProjectName` variable,
# the `set_compile_options()` / `enable_lto_for_target()` / `sign_output()`
# functions, the `proj_stuff` file list, and the NL_UI_PATH / NL_UI_REL_PATH /
# SKSE_PLUGIN_PATH variables.

if(BUILD_AS_SHARED)
    set(UIPluginProjectName "MeridianUIPlugin")

    file(
        GLOB_RECURSE
        UIPluginProjectName_src
            CONFIGURE_DEPENDS
            "src/UIPlugin/**.h"
            "src/UIPlugin/**.hpp"
            "src/UIPlugin/**.cpp"
    )
    source_group(
        TREE "${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlugin"
        FILES ${UIPluginProjectName_src}
    )
    list(
        APPEND
        UIPluginProjectName_src
            ${proj_stuff}
            ${CMAKE_CURRENT_BINARY_DIR}/include/Version.h
    )

    add_library(${UIPluginProjectName} SHARED ${UIPluginProjectName_src})
    set_target_properties(
        ${UIPluginProjectName}
        PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY_DEBUG ${SKSE_PLUGIN_PATH}
            RUNTIME_OUTPUT_DIRECTORY_RELEASE ${SKSE_PLUGIN_PATH}
            RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${SKSE_PLUGIN_PATH}
            RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${SKSE_PLUGIN_PATH}
            LIBRARY_OUTPUT_DIRECTORY_DEBUG ${SKSE_PLUGIN_PATH}
            LIBRARY_OUTPUT_DIRECTORY_RELEASE ${SKSE_PLUGIN_PATH}
            LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO ${SKSE_PLUGIN_PATH}
            LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL ${SKSE_PLUGIN_PATH}
            #ARCHIVE_OUTPUT_DIRECTORY ${SKSE_PLUGIN_PATH}/archive
            #PDB_OUTPUT_DIRECTORY ${SKSE_PLUGIN_PATH}
    )

    target_include_directories(
        ${UIPluginProjectName}
        PUBLIC
            ${CMAKE_CURRENT_BINARY_DIR}/include
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    set_compile_options(${UIPluginProjectName})
    enable_lto_for_target(${UIPluginProjectName})

    target_link_libraries(
        ${UIPluginProjectName}
        PUBLIC
            CommonLibSSE::CommonLibSSE
    )

    target_compile_definitions(
        ${UIPluginProjectName}
        PRIVATE
            NL_UI_REL_PATH="${NL_UI_REL_PATH}"
            NL_UI_LIB_NAME="${UIPlatformProjectName}"
    )

    sign_output(${UIPluginProjectName})

    add_custom_command(
        TARGET ${UIPluginProjectName} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
                "${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform/Config/MeridianUI.ini.disabled"
                "${SKSE_PLUGIN_PATH}/MeridianUI.ini.disabled"
        COMMENT "Staging MeridianUI.ini.disabled template..."
    )
endif()
