# Embed the tested readable helper. Reconfigure when it changes.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform/Web/meridian-input.js")
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform/Web/meridian-input.js" MERIDIAN_INPUT_SCRIPT)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform/Web/InputScript.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/InputScript.h" @ONLY)
