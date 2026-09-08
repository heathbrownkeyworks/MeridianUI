# MeridianNifMaterialShaders -- compile the NIF material HLSL shader with
# the Windows SDK fxc.exe into C-header form (g_NifMaterialVS / g_NifMaterialPS).
# Included from the top-level CMakeLists.txt before UIPlatform.cmake, which
# depends on the MeridianNifMaterialShaders target and the
# MERIDIAN_NIF_SHADER_OUTPUT_DIR variable defined here.

set(MERIDIAN_NIF_SHADER_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform/Render/Shaders/NifMaterial.hlsl")
set(MERIDIAN_NIF_SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/shaders")
set(MERIDIAN_NIF_VERTEX_SHADER_HEADER "${MERIDIAN_NIF_SHADER_OUTPUT_DIR}/NifMaterialVS.h")
set(MERIDIAN_NIF_PIXEL_SHADER_HEADER "${MERIDIAN_NIF_SHADER_OUTPUT_DIR}/NifMaterialPS.h")
find_program(
    MERIDIAN_FXC_EXECUTABLE
    NAMES fxc.exe fxc
    HINTS
        "C:/Program Files (x86)/Windows Kits/10/bin/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/x64"
        "C:/Program Files (x86)/Windows Kits/10/bin/x64"
)
if(NOT MERIDIAN_FXC_EXECUTABLE)
    message(FATAL_ERROR "Windows SDK fxc.exe is required to build Meridian's NIF material shader")
endif()
add_custom_command(
    OUTPUT
        "${MERIDIAN_NIF_VERTEX_SHADER_HEADER}"
        "${MERIDIAN_NIF_PIXEL_SHADER_HEADER}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${MERIDIAN_NIF_SHADER_OUTPUT_DIR}"
    COMMAND "${MERIDIAN_FXC_EXECUTABLE}" /nologo /Ges /O3 /T vs_5_0 /E VSMain
            /Fh "${MERIDIAN_NIF_VERTEX_SHADER_HEADER}" /Vn g_NifMaterialVS
            "${MERIDIAN_NIF_SHADER_SOURCE}"
    COMMAND "${MERIDIAN_FXC_EXECUTABLE}" /nologo /Ges /O3 /T ps_5_0 /E PSMain
            /Fh "${MERIDIAN_NIF_PIXEL_SHADER_HEADER}" /Vn g_NifMaterialPS
            "${MERIDIAN_NIF_SHADER_SOURCE}"
    DEPENDS "${MERIDIAN_NIF_SHADER_SOURCE}"
    COMMENT "Compiling Meridian NIF material shaders..."
    VERBATIM
)
add_custom_target(
    MeridianNifMaterialShaders
    DEPENDS
        "${MERIDIAN_NIF_VERTEX_SHADER_HEADER}"
        "${MERIDIAN_NIF_PIXEL_SHADER_HEADER}"
)
