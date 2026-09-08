cmake_minimum_required(VERSION 3.23)
add_definitions(-D_WIN32_WINNT=0x0A00)

option(BUILD_AS_SHARED "Build as dll skse plugin else build as static lib" ON)
option(MERIDIAN_ENABLE_SIGNING "Code-sign release binaries with Azure Artifact Signing" OFF)
option(MERIDIAN_BUILD_FIXTURE "Build and stage the in-game MeridianUITest fixture" OFF)
option(MERIDIAN_BUILD_NIF_TEST "Build the standalone MeridianNifTest SKSE consumer" OFF)

# Version
set(LIB_MAJOR_VERSION 1)
set(LIB_MINOR_VERSION 2)
set(LIB_PATCH_VERSION 1)
set(API_MAJOR_VERSION 1)
set(API_MINOR_VERSION 0)

# VCPKG config
string(REPLACE "\\" "/" ENV_VCPKG_ROOT "$ENV{VCPKG_ROOT}")
set(ENV{VCPKG_ROOT} ${ENV_VCPKG_ROOT})


# Includes pdb files in release mode
#set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")
#set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")

# Options
# Limit build parallelism (configurable via -DCMAKE_COMPILE_JOBS and -DCMAKE_LINK_JOBS)
if(NOT DEFINED CMAKE_COMPILE_JOBS)
    set(CMAKE_COMPILE_JOBS 8)
endif()

if(NOT DEFINED CMAKE_LINK_JOBS)
    set(CMAKE_LINK_JOBS 2)
endif()

set_property(GLOBAL PROPERTY JOB_POOLS "compile=${CMAKE_COMPILE_JOBS}" "link=${CMAKE_LINK_JOBS}")
set(CMAKE_JOB_POOL_COMPILE compile)
set(CMAKE_JOB_POOL_LINK link)
set(CMAKE_INSTALL_MESSAGE LAZY)
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
find_package(simpleini CONFIG REQUIRED)

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
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CEF.cmake)

# ============================ CEF Subprocess ============================
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CEFSubprocess.cmake)

# ============================ Tests ============================
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Tests.cmake)

# ============================ UIPlatform ============================
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/NifMaterialShaders.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/UIPlatform.cmake)

# ============================ UIPlugin ============================
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/UIPlugin.cmake)

# ============================ UIPlatformTest ============================
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/UIPlatformTest.cmake)

# ============================ Standalone NIF Test ============================
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/NifTest.cmake)

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
                -DVCPKG_LICENSE_ROOT=${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share
                -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/StageReleaseDocumentation.cmake
        DEPENDS
            ${CMAKE_CURRENT_SOURCE_DIR}/README.md
            ${CMAKE_CURRENT_SOURCE_DIR}/CHANGELOG.md
            ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE
            ${CMAKE_CURRENT_SOURCE_DIR}/EXCEPTIONS.md
            ${CMAKE_CURRENT_SOURCE_DIR}/LICENSING.md
            ${CMAKE_CURRENT_SOURCE_DIR}/LICENSES/MIT.txt
            ${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md
            ${API_FOLDER_PATH}/LICENSE-MIT
            ${CMAKE_CURRENT_SOURCE_DIR}/docs/MeridianUI-AuthorGuide.md
            ${CMAKE_CURRENT_SOURCE_DIR}/docs/MeridianView-AuthorGuide.md
            ${CMAKE_CURRENT_SOURCE_DIR}/docs/testing/SE_AE_1.1.1_RUNTIME_GATE.md
            ${CMAKE_CURRENT_SOURCE_DIR}/docs/testing/AE_1.7.104_RUNTIME_GATE.md
            ${CMAKE_CURRENT_SOURCE_DIR}/docs/testing/MERIDIAN_FOCUSED_INPUT_RUNTIME_GATE.md
            ${CMAKE_CURRENT_SOURCE_DIR}/docs/testing/MERIDIAN_ELEVATED_STARTUP_RUNTIME_GATE.md
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
