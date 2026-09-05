if(NOT DEFINED ROOT)
    message(FATAL_ERROR "ROOT is required")
endif()

if(NOT EXISTS "${ROOT}")
    message(STATUS "Release staging root does not exist yet: ${ROOT}")
    return()
endif()

file(GLOB_RECURSE staged_entries LIST_DIRECTORIES true "${ROOT}/*")
foreach(entry IN LISTS staged_entries)
    file(TO_CMAKE_PATH "${entry}" normalized)
    get_filename_component(name "${entry}" NAME)
    if(name MATCHES "^MeridianUITest(\\.dll|\\.pdb)?$" OR
       name MATCHES "\\.pdb$" OR
       name MATCHES "\\.lastcodeanalysissucceeded$" OR
       name STREQUAL "enable_fixture.txt" OR
       name STREQUAL "use_synccopy.txt" OR
       normalized MATCHES "/Data/MeridianUI/tests($|/)" OR
       normalized MATCHES "/Data/MeridianUI/MeridianUITest($|/)")
        message(FATAL_ERROR "Release staging contains fixture artifact: ${entry}")
    endif()
endforeach()

set(required_release_files
    "${ROOT}/MeridianUI/LICENSE"
    "${ROOT}/MeridianUI/EXCEPTIONS.md"
    "${ROOT}/MeridianUI/LICENSING.md"
    "${ROOT}/MeridianUI/CHANGELOG.md"
    "${ROOT}/MeridianUI/README.md"
    "${ROOT}/MeridianUI/THIRD_PARTY_NOTICES.md"
    "${ROOT}/MeridianUI/SDK/MeridianUIAPI/API.h"
    "${ROOT}/MeridianUI/SDK/MeridianUIAPI/ViewAPI.h"
    "${ROOT}/MeridianUI/SDK/MeridianUIAPI/RenderLayerAPI.h"
    "${ROOT}/MeridianUI/SDK/MeridianUIAPI/NifViewAPI.h"
    "${ROOT}/MeridianUI/SDK/MeridianUIAPI/NifSceneAPI.h"
    "${ROOT}/MeridianUI/SDK/MeridianUIAPI/LICENSE-MIT"
    "${ROOT}/MeridianUI/docs/testing/SE_AE_1.1.1_RUNTIME_GATE.md"
    "${ROOT}/MeridianUI/docs/testing/AE_1.7.104_RUNTIME_GATE.md"
    "${ROOT}/MeridianUI/docs/testing/MERIDIAN_FOCUSED_INPUT_RUNTIME_GATE.md"
    "${ROOT}/MeridianUI/docs/testing/MERIDIAN_ELEVATED_STARTUP_RUNTIME_GATE.md"
    "${ROOT}/MeridianUI/docs/testing/MERIDIAN_VIEW_1_HORDE_RUNTIME_GATE.md"
    "${ROOT}/MeridianUI/docs/testing/MERIDIAN_NIF_ACTOR_APPEARANCE_RUNTIME_GATE.md"
    "${ROOT}/MeridianUI/licenses/cef-prebuilt.txt"
    "${ROOT}/MeridianUI/licenses/commonlibsse-ng.txt"
    "${ROOT}/MeridianUI/licenses/meridianui-api-and-nirnlab-mit.txt"
    "${ROOT}/MeridianUI/licenses/toml11.txt"
)
foreach(required_file IN LISTS required_release_files)
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR "Release staging is missing required SDK/license material: ${required_file}")
    endif()
endforeach()

message(STATUS "Release manifest is fixture-free: ${ROOT}")
