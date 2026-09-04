cmake_minimum_required(VERSION 3.23)

option(BUILD_AS_SHARED "Build as dll skse plugin else build as static lib" ON)
option(MERIDIAN_ENABLE_SIGNING "Code-sign release binaries with Azure Artifact Signing" OFF)
option(MERIDIAN_BUILD_FIXTURE "Build and stage the in-game MeridianUITest fixture" OFF)
option(MERIDIAN_BUILD_NIF_TEST "Build the standalone MeridianNifTest SKSE consumer" OFF)

# Version
set(LIB_MAJOR_VERSION 1)
set(LIB_MINOR_VERSION 2)
set(LIB_PATCH_VERSION 0)
set(API_MAJOR_VERSION 1)
set(API_MINOR_VERSION 0)

# VCPKG config
string(REPLACE "\\" "/" ENV_VCPKG_ROOT "$ENV{VCPKG_ROOT}")
set(ENV{VCPKG_ROOT} ${ENV_VCPKG_ROOT})

set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
set(VCPKG_TARGET_TRIPLET "x64-windows-static")
set(VCPKG_OVERLAY_TRIPLETS "${CMAKE_CURRENT_LIST_DIR}/overlay_triplets")
set(VCPKG_OVERLAY_PORTS "${CMAKE_CURRENT_LIST_DIR}/overlay_ports")

# Includes pdb files in release mode
#set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")
#set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")

# Options
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "/EHsc /MP /W4")
add_compile_definitions(WINDOWS)

project(
    MeridianUI
    LANGUAGES CXX
    VERSION ${LIB_MAJOR_VERSION}.${LIB_MINOR_VERSION}.${LIB_PATCH_VERSION}
)
include(CTest)
include(${CMAKE_SOURCE_DIR}/cmake/set_compile_options.cmake)
set(CMAKE_GENERATOR_PLATFORM x64)

string(REPLACE "/Ob2" "/Ob3" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
string(REPLACE "/Ob2" "/Ob3" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")

# Configure files
configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/src/Version.h.in
	${CMAKE_CURRENT_BINARY_DIR}/include/Version.h
	@ONLY
)
configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/src/version.rc.in
	${CMAKE_CURRENT_BINARY_DIR}/version.rc
	@ONLY
)

file(
    GLOB 
    proj_stuff
        ${CMAKE_SOURCE_DIR}/.clang-format
        ${CMAKE_CURRENT_BINARY_DIR}/version.rc
)
source_group(" " FILES ${proj_stuff})

# Deps
find_package(CommonLibSSE CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(directxtk CONFIG REQUIRED)
find_package(directxmath CONFIG REQUIRED)
find_package(PalSigslot CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_path(SIMPLEINI_INCLUDE_DIRS "ConvertUTF.c" REQUIRED)

# Code-sign release binaries via Azure Artifact Signing (see signing/sign.ps1).
# Only runs in Release config; debug builds need no Azure login or network.
function(sign_output TARGET_NAME)
    if(NOT MERIDIAN_ENABLE_SIGNING)
        return()
    endif()

    add_custom_command(
        TARGET ${TARGET_NAME} POST_BUILD
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass
                -File "${CMAKE_SOURCE_DIR}/signing/cmake_sign.ps1" "$<CONFIG>" "$<TARGET_FILE:${TARGET_NAME}>"
        COMMENT "Signing ${TARGET_NAME} (Release only)..."
        VERBATIM
    )
endfunction()

# Paths
set(API_FOLDER_PATH ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform/MeridianUIAPI)
if (NOT EXISTS ${API_FOLDER_PATH})
    message(FATAL_ERROR "API folder not found \"${API_FOLDER_PATH}\"")
endif()
if (NOT DEFINED OUTPUT_PATH)
    set(OUTPUT_PATH ${CMAKE_BINARY_DIR}/dist/$<CONFIG>)
endif()

set(SKSE_PLUGIN_REL_PATH "Data/SKSE/Plugins")
set(SKSE_PLUGIN_PATH ${OUTPUT_PATH}/${SKSE_PLUGIN_REL_PATH})

if(NOT DEFINED NL_UI_REL_PATH)
    set(NL_UI_REL_PATH "Data/MeridianUI")
endif()
set(NL_UI_PATH ${OUTPUT_PATH}/${NL_UI_REL_PATH})

# ============================ CEF LIB ============================
add_library(cef INTERFACE)
find_library(CEF_LIBRARY_DEBUG libcef_dll_wrapper)
string(REPLACE "/debug/lib/" "/lib/" CEF_LIBRARY_RELEASE ${CEF_LIBRARY_DEBUG})
find_path(CEF_INCLUDE_DIR include/cef_app.h)
find_path(CEF_SHARE share/cef-prebuilt)
set(CEF_SHARE "${CEF_SHARE}/share/cef-prebuilt")
target_link_libraries(cef INTERFACE "$<IF:$<CONFIG:Debug>,${CEF_LIBRARY_DEBUG},${CEF_LIBRARY_RELEASE}>")
target_link_libraries(cef INTERFACE "${CEF_SHARE}/$<IF:$<CONFIG:Debug>,Debug,Release>/libcef.lib")
target_include_directories(cef INTERFACE ${CEF_INCLUDE_DIR})
set(cef_files_path "${CEF_SHARE}/$<IF:$<CONFIG:Debug>,Debug,Release>")
add_custom_target(
    CefLibraryFiles
    # Cef files
    COMMAND ${CMAKE_COMMAND} -E make_directory                                                  ${NL_UI_PATH}/
    COMMAND ${CMAKE_COMMAND} -E copy ${cef_files_path}/chrome_elf.dll                           ${NL_UI_PATH}
    COMMAND ${CMAKE_COMMAND} -E copy ${cef_files_path}/d3dcompiler_47.dll                       ${NL_UI_PATH}
    COMMAND ${CMAKE_COMMAND} -E copy ${cef_files_path}/dxcompiler.dll                           ${NL_UI_PATH}
    COMMAND ${CMAKE_COMMAND} -E copy ${cef_files_path}/dxil.dll                                 ${NL_UI_PATH}
    COMMAND ${CMAKE_COMMAND} -E copy ${cef_files_path}/libcef.dll                               ${NL_UI_PATH}
    COMMAND ${CMAKE_COMMAND} -E copy ${cef_files_path}/libEGL.dll                               ${NL_UI_PATH}
    COMMAND ${CMAKE_COMMAND} -E copy ${cef_files_path}/libGLESv2.dll                            ${NL_UI_PATH}
    COMMAND ${CMAKE_COMMAND} -E copy ${cef_files_path}/v8_context_snapshot.bin                  ${NL_UI_PATH}
    COMMAND ${CMAKE_COMMAND} -E copy ${cef_files_path}/vk_swiftshader.dll                       ${NL_UI_PATH}
    COMMAND ${CMAKE_COMMAND} -E copy ${cef_files_path}/vk_swiftshader_icd.json                  ${NL_UI_PATH}
    COMMAND ${CMAKE_COMMAND} -E copy ${cef_files_path}/vulkan-1.dll                             ${NL_UI_PATH}
    COMMAND ${CMAKE_COMMAND} -E copy ${cef_files_path}/../Resources/chrome_100_percent.pak      ${NL_UI_PATH}
    COMMAND ${CMAKE_COMMAND} -E copy ${cef_files_path}/../Resources/chrome_200_percent.pak      ${NL_UI_PATH}
    COMMAND ${CMAKE_COMMAND} -E copy ${cef_files_path}/../Resources/icudtl.dat                  ${NL_UI_PATH}
    COMMAND ${CMAKE_COMMAND} -E copy ${cef_files_path}/../Resources/resources.pak               ${NL_UI_PATH}
    COMMAND ${CMAKE_COMMAND} -E make_directory                                                  ${NL_UI_PATH}/locales
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${cef_files_path}/../Resources/locales           ${NL_UI_PATH}/locales
    # Output Message
    COMMENT "Copying cef files..." VERBATIM
)

# ============================ CEF Subprocess ============================
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
        #ARCHIVE_OUTPUT_DIRECTORY ${NL_UI_PATH}/archive
        #PDB_OUTPUT_DIRECTORY ${NL_UI_PATH}
)

target_compile_definitions(
    ${CEFSubprocessProjectName}
    PRIVATE
        CEF_SUBPROCESS_PROJECT_NAME="${CEFSubprocessProjectName}"
)

add_dependencies(${CEFSubprocessProjectName} CefLibraryFiles)
sign_output(${CEFSubprocessProjectName})

if(BUILD_TESTING)
    add_executable(
        ProcessDownDetectorTests
            tests/CEFSubprocess/ProcessDownDetectorTests.cpp
            src/CEFSubprocess/ProcessDownDetector.cpp
    )
    target_include_directories(
        ProcessDownDetectorTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/CEFSubprocess
    )
    set_compile_options(ProcessDownDetectorTests)
    add_test(NAME ProcessDownDetectorTests COMMAND ProcessDownDetectorTests)

    add_executable(
        PromiseRegistryTests
            tests/CEFSubprocess/PromiseRegistryTests.cpp
    )
    target_include_directories(
        PromiseRegistryTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/CEFSubprocess
    )
    set_compile_options(PromiseRegistryTests)
    add_test(NAME PromiseRegistryTests COMMAND PromiseRegistryTests)

    add_executable(
        JsBindingMessageTests
            tests/IPC/JsBindingMessageTests.cpp
    )
    target_include_directories(
        JsBindingMessageTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/CEFSubprocess
    )
    set_compile_options(JsBindingMessageTests)
    add_test(NAME JsBindingMessageTests COMMAND JsBindingMessageTests)

    add_executable(
        VersionGateTests
            tests/UIPlatform/VersionGateTests.cpp
    )
    target_include_directories(
        VersionGateTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(VersionGateTests)
    add_test(NAME VersionGateTests COMMAND VersionGateTests)

    add_executable(
        ViewAPIContractTests
            tests/UIPlatform/ViewAPIContractTests.cpp
    )
    target_include_directories(
        ViewAPIContractTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(ViewAPIContractTests)
    add_test(NAME ViewAPIContractTests COMMAND ViewAPIContractTests)

    add_executable(
        RenderLayerAPIContractTests
            tests/UIPlatform/RenderLayerAPIContractTests.cpp
    )
    target_include_directories(
        RenderLayerAPIContractTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(RenderLayerAPIContractTests)
    add_test(NAME RenderLayerAPIContractTests COMMAND RenderLayerAPIContractTests)

    add_executable(
        NifViewAPIContractTests
            tests/UIPlatform/NifViewAPIContractTests.cpp
    )
    target_include_directories(
        NifViewAPIContractTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(NifViewAPIContractTests)
    add_test(NAME NifViewAPIContractTests COMMAND NifViewAPIContractTests)

    add_executable(
        NifSceneAPIContractTests
            tests/UIPlatform/NifSceneAPIContractTests.cpp
    )
    target_include_directories(
        NifSceneAPIContractTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(NifSceneAPIContractTests)
    add_test(NAME NifSceneAPIContractTests COMMAND NifSceneAPIContractTests)

    add_executable(
        CheckerboardTextureTests
            tests/UIPlatform/CheckerboardTextureTests.cpp
    )
    target_include_directories(
        CheckerboardTextureTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(CheckerboardTextureTests)
    add_test(NAME CheckerboardTextureTests COMMAND CheckerboardTextureTests)

    add_executable(
        PackedGeometryDecoderTests
            tests/UIPlatform/PackedGeometryDecoderTests.cpp
    )
    target_include_directories(
        PackedGeometryDecoderTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(PackedGeometryDecoderTests)
    add_test(NAME PackedGeometryDecoderTests COMMAND PackedGeometryDecoderTests)

    add_executable(
        BindPoseSkinningTests
            tests/UIPlatform/BindPoseSkinningTests.cpp
    )
    target_include_directories(
        BindPoseSkinningTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(BindPoseSkinningTests)
    add_test(NAME BindPoseSkinningTests COMMAND BindPoseSkinningTests)

    add_executable(
        NifMaterialTintTests
            tests/UIPlatform/NifMaterialTintTests.cpp
    )
    target_include_directories(
        NifMaterialTintTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(NifMaterialTintTests)
    add_test(NAME NifMaterialTintTests COMMAND NifMaterialTintTests)

    add_executable(
        FaceGenMaterialTests
            tests/UIPlatform/FaceGenMaterialTests.cpp
    )
    target_include_directories(
        FaceGenMaterialTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(FaceGenMaterialTests)
    add_test(NAME FaceGenMaterialTests COMMAND FaceGenMaterialTests)

    add_executable(
        NifSceneCompositionTests
            tests/UIPlatform/NifSceneCompositionTests.cpp
    )
    target_include_directories(
        NifSceneCompositionTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(NifSceneCompositionTests)
    add_test(NAME NifSceneCompositionTests COMMAND NifSceneCompositionTests)

    add_executable(
        NifWeightInterpolationTests
            tests/UIPlatform/NifWeightInterpolationTests.cpp
    )
    target_include_directories(
        NifWeightInterpolationTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(NifWeightInterpolationTests)
    add_test(NAME NifWeightInterpolationTests COMMAND NifWeightInterpolationTests)

    add_executable(
        NifModelPathTests
            tests/UIPlatform/NifModelPathTests.cpp
    )
    target_include_directories(
        NifModelPathTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(NifModelPathTests)
    add_test(NAME NifModelPathTests COMMAND NifModelPathTests)

    add_executable(
        NifArmorModelPathTests
            tests/UIPlatform/NifArmorModelPathTests.cpp
    )
    target_include_directories(
        NifArmorModelPathTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(NifArmorModelPathTests)
    add_test(NAME NifArmorModelPathTests COMMAND NifArmorModelPathTests)

    add_executable(
        ActorAppearancePolicyTests
            tests/UIPlatform/ActorAppearancePolicyTests.cpp
    )
    target_include_directories(
        ActorAppearancePolicyTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(ActorAppearancePolicyTests)
    add_test(NAME ActorAppearancePolicyTests COMMAND ActorAppearancePolicyTests)

    add_executable(
        LiveActorSnapshotTests
            tests/UIPlatform/LiveActorSnapshotTests.cpp
    )
    target_include_directories(
        LiveActorSnapshotTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(LiveActorSnapshotTests)
    add_test(NAME LiveActorSnapshotTests COMMAND LiveActorSnapshotTests)

    add_executable(
        NifTexturePathTests
            tests/UIPlatform/NifTexturePathTests.cpp
    )
    target_include_directories(
        NifTexturePathTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(NifTexturePathTests)
    add_test(NAME NifTexturePathTests COMMAND NifTexturePathTests)

    add_executable(
        NifTextureOverrideTests
            tests/UIPlatform/NifTextureOverrideTests.cpp
    )
    target_include_directories(
        NifTextureOverrideTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(NifTextureOverrideTests)
    add_test(NAME NifTextureOverrideTests COMMAND NifTextureOverrideTests)

    add_executable(
        EquippedArmorManifestTests
            tests/UIPlatform/EquippedArmorManifestTests.cpp
    )
    target_include_directories(
        EquippedArmorManifestTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/NifTest
    )
    set_compile_options(EquippedArmorManifestTests)
    add_test(NAME EquippedArmorManifestTests COMMAND EquippedArmorManifestTests)

    add_executable(
        NifTestHotkeyTests
            tests/UIPlatform/NifTestHotkeyTests.cpp
    )
    target_include_directories(
        NifTestHotkeyTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/NifTest
    )
    set_compile_options(NifTestHotkeyTests)
    add_test(NAME NifTestHotkeyTests COMMAND NifTestHotkeyTests)

    add_executable(
        NifTestControlPayloadTests
            tests/UIPlatform/NifTestControlPayloadTests.cpp
    )
    target_include_directories(
        NifTestControlPayloadTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/NifTest
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(NifTestControlPayloadTests)
    add_test(NAME NifTestControlPayloadTests COMMAND NifTestControlPayloadTests)

    add_executable(
        NifCameraMathTests
            tests/UIPlatform/NifCameraMathTests.cpp
    )
    target_include_directories(
        NifCameraMathTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(NifCameraMathTests)
    add_test(NAME NifCameraMathTests COMMAND NifCameraMathTests)

    add_executable(
        NifLightingTests
            tests/UIPlatform/NifLightingTests.cpp
    )
    target_include_directories(
        NifLightingTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(NifLightingTests)
    add_test(NAME NifLightingTests COMMAND NifLightingTests)

    add_executable(
        NifControlWebContractTests
            tests/UIPlatform/NifControlWebContractTests.cpp
    )
    target_compile_definitions(
        NifControlWebContractTests
        PRIVATE
            MERIDIAN_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    )
    set_compile_options(NifControlWebContractTests)
    add_test(NAME NifControlWebContractTests COMMAND NifControlWebContractTests)

    add_executable(
        NifPreviewRendererArchitectureTests
            tests/UIPlatform/NifPreviewRendererArchitectureTests.cpp
    )
    target_compile_definitions(
        NifPreviewRendererArchitectureTests
        PRIVATE
            MERIDIAN_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    )
    set_compile_options(NifPreviewRendererArchitectureTests)
    add_test(NAME NifPreviewRendererArchitectureTests COMMAND NifPreviewRendererArchitectureTests)

    add_executable(
        ViewBridgeScriptTests
            tests/UIPlatform/ViewBridgeScriptTests.cpp
            src/UIPlatform/Controllers/ViewBridgeScripts.cpp
    )
    target_include_directories(
        ViewBridgeScriptTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    target_link_libraries(
        ViewBridgeScriptTests
        PRIVATE
            nlohmann_json::nlohmann_json
    )
    set_compile_options(ViewBridgeScriptTests)
    add_test(NAME ViewBridgeScriptTests COMMAND ViewBridgeScriptTests)

    add_executable(
        CompositorMathTests
            tests/UIPlatform/CompositorMathTests.cpp
    )
    target_include_directories(
        CompositorMathTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(CompositorMathTests)
    add_test(NAME CompositorMathTests COMMAND CompositorMathTests)

    add_executable(
        SettingsIngestTests
            tests/UIPlatform/SettingsIngestTests.cpp
    )
    target_include_directories(
        SettingsIngestTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(SettingsIngestTests)
    add_test(NAME SettingsIngestTests COMMAND SettingsIngestTests)

    add_executable(
        IniConfigTests
            tests/UIPlatform/IniConfigTests.cpp
            src/UIPlatform/Config/IniConfig.cpp
    )
    target_include_directories(
        IniConfigTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
            ${SIMPLEINI_INCLUDE_DIRS}
    )
    target_compile_definitions(
        IniConfigTests
        PRIVATE
            MERIDIAN_INICONFIG_NO_RUNTIME=1
    )
    set_compile_options(IniConfigTests)
    add_test(NAME IniConfigTests COMMAND IniConfigTests)

    add_executable(
        CursorTextureCacheTests
            tests/UIPlatform/CursorTextureCacheTests.cpp
    )
    target_include_directories(
        CursorTextureCacheTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(CursorTextureCacheTests)
    add_test(NAME CursorTextureCacheTests COMMAND CursorTextureCacheTests)

    add_executable(
        FrameTransportTests
            tests/UIPlatform/FrameTransportTests.cpp
            src/UIPlatform/Render/RenderDevice.cpp
            src/UIPlatform/Render/FrameTransport.cpp
    )
    target_include_directories(
        FrameTransportTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    target_compile_definitions(
        FrameTransportTests
        PRIVATE
            MERIDIAN_FRAME_TRANSPORT_NO_PCH=1
    )
    target_link_libraries(
        FrameTransportTests
        PRIVATE
            d3d11.lib
            dxgi.lib
    )
    set_compile_options(FrameTransportTests)
    add_test(NAME FrameTransportTests COMMAND FrameTransportTests)

    add_executable(
        InputChordTests
            tests/UIPlatform/InputChordTests.cpp
    )
    target_include_directories(
        InputChordTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(InputChordTests)
    add_test(NAME InputChordTests COMMAND InputChordTests)

    add_executable(
        InputSinkPriorityTests
            tests/UIPlatform/InputSinkPriorityTests.cpp
    )
    target_include_directories(
        InputSinkPriorityTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(InputSinkPriorityTests)
    add_test(NAME InputSinkPriorityTests COMMAND InputSinkPriorityTests)

    add_executable(
        PressedKeyStateTests
            tests/UIPlatform/PressedKeyStateTests.cpp
    )
    target_include_directories(
        PressedKeyStateTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(PressedKeyStateTests)
    add_test(NAME PressedKeyStateTests COMMAND PressedKeyStateTests)

    add_executable(
        CallSiteValidationTests
            tests/UIPlatform/CallSiteValidationTests.cpp
    )
    target_include_directories(
        CallSiteValidationTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(CallSiteValidationTests)
    add_test(NAME CallSiteValidationTests COMMAND CallSiteValidationTests)

    add_executable(
        ModSchemePathTests
            tests/UIPlatform/ModSchemePathTests.cpp
            src/UIPlatform/Scheme/ModSchemePath.cpp
    )
    target_include_directories(
        ModSchemePathTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(ModSchemePathTests)
    add_test(NAME ModSchemePathTests COMMAND ModSchemePathTests)

    add_executable(
        WeakFocusOwnerTests
            tests/UIPlatform/WeakFocusOwnerTests.cpp
    )
    target_include_directories(
        WeakFocusOwnerTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(WeakFocusOwnerTests)
    add_test(NAME WeakFocusOwnerTests COMMAND WeakFocusOwnerTests)

    add_executable(
        TextInputLeaseStateTests
            tests/UIPlatform/TextInputLeaseStateTests.cpp
    )
    target_include_directories(
        TextInputLeaseStateTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(TextInputLeaseStateTests)
    add_test(NAME TextInputLeaseStateTests COMMAND TextInputLeaseStateTests)

    add_executable(
        BrowserContentPolicyTests
            tests/UIPlatform/BrowserContentPolicyTests.cpp
            src/UIPlatform/CEF/BrowserContentPolicy.cpp
            src/UIPlatform/Scheme/ModSchemePath.cpp
    )
    target_include_directories(
        BrowserContentPolicyTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(BrowserContentPolicyTests)
    add_test(NAME BrowserContentPolicyTests COMMAND BrowserContentPolicyTests)

    add_executable(
        PublicAPIValidationTests
            tests/UIPlatform/PublicAPIValidationTests.cpp
    )
    target_include_directories(
        PublicAPIValidationTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(PublicAPIValidationTests)
    add_test(NAME PublicAPIValidationTests COMMAND PublicAPIValidationTests)

    add_executable(
        RuntimePathsTests
            tests/UIPlatform/RuntimePathsTests.cpp
    )
    target_include_directories(
        RuntimePathsTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    set_compile_options(RuntimePathsTests)
    add_test(NAME RuntimePathsTests COMMAND RuntimePathsTests)

    add_executable(
        CursorPolicyTests
            tests/UIPlatform/CursorPolicyTests.cpp
    )
    target_include_directories(
        CursorPolicyTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(CursorPolicyTests)
    add_test(NAME CursorPolicyTests COMMAND CursorPolicyTests)

    add_executable(
        RendererSelectionTests
            tests/UIPlatform/RendererSelectionTests.cpp
    )
    target_include_directories(
        RendererSelectionTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(RendererSelectionTests)
    add_test(NAME RendererSelectionTests COMMAND RendererSelectionTests)

    add_executable(
        FocusSessionStateTests
            tests/UIPlatform/FocusSessionStateTests.cpp
    )
    target_include_directories(
        FocusSessionStateTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/UIPlatform
    )
    set_compile_options(FocusSessionStateTests)
    add_test(NAME FocusSessionStateTests COMMAND FocusSessionStateTests)
endif()

# ============================ UIPlatform ============================
set(UIPlatformProjectName "MeridianUI")

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
        ${SIMPLEINI_INCLUDE_DIRS}
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

# ============================ UIPlugin ============================
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

# ============================ UIPlatformTest ============================
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

    add_library(${UIPlatformTestName} SHARED ${UIPlatformTestName_src})
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
            #ARCHIVE_OUTPUT_DIRECTORY ${SKSE_PLUGIN_PATH}/archive
            #PDB_OUTPUT_DIRECTORY ${SKSE_PLUGIN_PATH}
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

    target_link_libraries(
        ${UIPlatformTestName}
        PUBLIC
            CommonLibSSE::CommonLibSSE
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

# ============================ Standalone NIF Test ============================
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

# Stage the public SDK, current guides, and complete direct/transitive license
# texts as part of the ordinary production Data tree.
if(BUILD_AS_SHARED)
    add_custom_target(
        StageReleaseDocumentation ALL
        COMMAND ${CMAKE_COMMAND}
                -DROOT=${NL_UI_PATH}
                -DPROJECT_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
                -DAPI_SOURCE=${API_FOLDER_PATH}
                -DGENERATED_VERSION=${CMAKE_CURRENT_BINARY_DIR}/include/Version.h
                -DVCPKG_LICENSE_ROOT=${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/share
                -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/StageReleaseDocumentation.cmake
        DEPENDS
            ${CMAKE_CURRENT_SOURCE_DIR}/README.md
            ${CMAKE_CURRENT_SOURCE_DIR}/CHANGELOG.md
            ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE
            ${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md
            ${CMAKE_CURRENT_SOURCE_DIR}/docs/MeridianUI-AuthorGuide.md
            ${CMAKE_CURRENT_SOURCE_DIR}/docs/MeridianView-AuthorGuide.md
            ${CMAKE_CURRENT_SOURCE_DIR}/docs/testing/SE_AE_1.1.1_RUNTIME_GATE.md
            ${CMAKE_CURRENT_SOURCE_DIR}/docs/testing/AE_1.7.104_RUNTIME_GATE.md
            ${CMAKE_CURRENT_SOURCE_DIR}/docs/testing/MERIDIAN_FOCUSED_INPUT_RUNTIME_GATE.md
            ${CMAKE_CURRENT_SOURCE_DIR}/docs/testing/MERIDIAN_SE_RINGBUFFER_RUNTIME_GATE.md
            ${CMAKE_CURRENT_SOURCE_DIR}/docs/testing/MERIDIAN_DLSS5_RUNTIME_GATE.md
            ${CMAKE_CURRENT_SOURCE_DIR}/docs/testing/MERIDIAN_VIEW_1_HORDE_RUNTIME_GATE.md
            ${CMAKE_CURRENT_SOURCE_DIR}/docs/testing/MERIDIAN_NIF_ACTOR_APPEARANCE_RUNTIME_GATE.md
            ${CMAKE_CURRENT_BINARY_DIR}/include/Version.h
        COMMENT "Staging Meridian SDK, guides, and license notices..."
        VERBATIM
    )
endif()

# Validate the staged production tree before signing or deployment, rather
# than relying on a later optional CTest invocation.
if(BUILD_AS_SHARED AND NOT MERIDIAN_BUILD_FIXTURE)
    add_custom_target(
        VerifyReleaseManifestTarget ALL
        COMMAND ${CMAKE_COMMAND}
                -DROOT=${OUTPUT_PATH}/Data
                -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyReleaseManifest.cmake
        DEPENDS
            CefLibraryFiles
            ${CEFSubprocessProjectName}
            ${UIPlatformProjectName}
            ${UIPluginProjectName}
            StageReleaseDocumentation
        COMMENT "Verifying production release manifest..."
        VERBATIM
    )
endif()

# Sign and verify every PE binary in the staged Release Data tree, including
# CEF/graphics runtime DLLs. Valid vendor signatures are preserved and the
# Meridian Azure signature is appended. This target always runs after staging
# so an incremental CEF copy cannot silently restore unsigned files.
if(MERIDIAN_ENABLE_SIGNING)
    if(TARGET VerifyReleaseManifestTarget)
        set(MeridianReleaseSigningDependencies VerifyReleaseManifestTarget)
    else()
        set(
            MeridianReleaseSigningDependencies
                CefLibraryFiles
                ${CEFSubprocessProjectName}
                ${UIPlatformProjectName}
        )
    endif()
    if(BUILD_AS_SHARED AND NOT TARGET VerifyReleaseManifestTarget)
        list(APPEND MeridianReleaseSigningDependencies ${UIPluginProjectName} StageReleaseDocumentation)
        if(MERIDIAN_BUILD_FIXTURE)
            list(APPEND MeridianReleaseSigningDependencies ${UIPlatformTestName})
        endif()
    endif()

    add_custom_target(
        SignReleaseArtifacts ALL
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass
                -File "${CMAKE_SOURCE_DIR}/signing/sign_release_artifacts.ps1"
                -Config "$<CONFIG>"
                -Root "${OUTPUT_PATH}/Data"
        DEPENDS ${MeridianReleaseSigningDependencies}
        COMMENT "Signing and verifying all staged Meridian DLL/EXE artifacts..."
        VERBATIM
    )
endif()

# ============================ Deploy ============================
# Mirrors the verified built Data tree into a mod folder after every build.
# Configure with -DDEPLOY_MOD_PATH="D:/path/to/mods/MeridianUI" to enable.
if(DEFINED DEPLOY_MOD_PATH AND BUILD_AS_SHARED)
    add_custom_target(
        DeployToMods ALL
        COMMAND ${CMAKE_COMMAND}
                -DSOURCE=${OUTPUT_PATH}/Data
                -DDESTINATION=${DEPLOY_MOD_PATH}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/SyncReleaseTree.cmake
        COMMENT "Deploying to mod folder: ${DEPLOY_MOD_PATH}" VERBATIM
    )
    add_dependencies(DeployToMods CefLibraryFiles ${CEFSubprocessProjectName} ${UIPlatformProjectName} ${UIPluginProjectName} StageReleaseDocumentation)
    if(TARGET VerifyReleaseManifestTarget)
        add_dependencies(DeployToMods VerifyReleaseManifestTarget)
    endif()
    if(MERIDIAN_ENABLE_SIGNING)
        add_dependencies(DeployToMods SignReleaseArtifacts)
    endif()
    if(MERIDIAN_BUILD_FIXTURE)
        add_dependencies(DeployToMods ${UIPlatformTestName})
    endif()
endif()

if(BUILD_TESTING AND BUILD_AS_SHARED AND NOT MERIDIAN_BUILD_FIXTURE)
    add_test(
        NAME ReleaseManifestTests
        COMMAND ${CMAKE_COMMAND}
                -DROOT=${CMAKE_BINARY_DIR}/dist/$<CONFIG>/Data
                -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyReleaseManifest.cmake
    )
endif()

if(BUILD_TESTING AND BUILD_AS_SHARED AND MERIDIAN_ENABLE_SIGNING)
    add_test(
        NAME ReleaseSignatureTests
        COMMAND powershell -NoProfile -ExecutionPolicy Bypass
                -File ${CMAKE_CURRENT_SOURCE_DIR}/signing/verify_release_artifacts.ps1
                -Config $<CONFIG>
                -Root ${CMAKE_BINARY_DIR}/dist/$<CONFIG>/Data
    )
endif()
