# Isolated diagnostic mod; never copied into the runtime's normal Data tree.
if(BUILD_AS_SHARED AND MERIDIAN_BUILD_INPUT_TEST)
    add_library(MeridianInputTest SHARED src/InputTest/main.cpp)
    target_include_directories(MeridianInputTest PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform)
    target_link_libraries(MeridianInputTest PRIVATE CommonLibSSE::CommonLibSSE spdlog::spdlog)
    set_compile_options(MeridianInputTest)
    set_target_properties(MeridianInputTest PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/input-test/$<CONFIG>/SKSE/Plugins")
    add_custom_target(MeridianInputTestAssets
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/input-test/$<CONFIG>/MeridianUI/meridianinputtest"
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different "${CMAKE_CURRENT_SOURCE_DIR}/src/InputTest/web"
            "${CMAKE_BINARY_DIR}/input-test/$<CONFIG>/MeridianUI/meridianinputtest"
        DEPENDS src/InputTest/web/index.html src/InputTest/web/input-test.js
        VERBATIM)
    add_dependencies(MeridianInputTest MeridianInputTestAssets)
endif()
