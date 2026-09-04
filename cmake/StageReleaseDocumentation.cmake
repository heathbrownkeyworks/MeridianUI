foreach(required ROOT PROJECT_ROOT API_SOURCE GENERATED_VERSION VCPKG_LICENSE_ROOT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

file(TO_CMAKE_PATH "${ROOT}" root_normalized)
if(NOT root_normalized MATCHES "/Data/MeridianUI$")
    message(FATAL_ERROR "Refusing to stage release documentation outside Data/MeridianUI: ${ROOT}")
endif()

file(MAKE_DIRECTORY "${ROOT}")
file(REMOVE_RECURSE "${ROOT}/SDK" "${ROOT}/docs" "${ROOT}/licenses")
file(MAKE_DIRECTORY "${ROOT}/SDK/MeridianUIAPI" "${ROOT}/docs/testing" "${ROOT}/licenses")

file(COPY_FILE "${PROJECT_ROOT}/README.md" "${ROOT}/README.md" ONLY_IF_DIFFERENT)
file(COPY_FILE "${PROJECT_ROOT}/CHANGELOG.md" "${ROOT}/CHANGELOG.md" ONLY_IF_DIFFERENT)
file(COPY_FILE "${PROJECT_ROOT}/LICENSE" "${ROOT}/LICENSE" ONLY_IF_DIFFERENT)
file(COPY_FILE "${PROJECT_ROOT}/THIRD_PARTY_NOTICES.md" "${ROOT}/THIRD_PARTY_NOTICES.md" ONLY_IF_DIFFERENT)
file(COPY_FILE "${PROJECT_ROOT}/docs/MeridianUI-AuthorGuide.md" "${ROOT}/docs/MeridianUI-AuthorGuide.md" ONLY_IF_DIFFERENT)
file(COPY_FILE "${PROJECT_ROOT}/docs/MeridianView-AuthorGuide.md" "${ROOT}/docs/MeridianView-AuthorGuide.md" ONLY_IF_DIFFERENT)
file(COPY_FILE "${PROJECT_ROOT}/docs/testing/SE_AE_1.1.1_RUNTIME_GATE.md" "${ROOT}/docs/testing/SE_AE_1.1.1_RUNTIME_GATE.md" ONLY_IF_DIFFERENT)
file(COPY_FILE "${PROJECT_ROOT}/docs/testing/AE_1.7.104_RUNTIME_GATE.md" "${ROOT}/docs/testing/AE_1.7.104_RUNTIME_GATE.md" ONLY_IF_DIFFERENT)
file(COPY_FILE "${PROJECT_ROOT}/docs/testing/MERIDIAN_FOCUSED_INPUT_RUNTIME_GATE.md" "${ROOT}/docs/testing/MERIDIAN_FOCUSED_INPUT_RUNTIME_GATE.md" ONLY_IF_DIFFERENT)
file(COPY_FILE "${PROJECT_ROOT}/docs/testing/MERIDIAN_SE_RINGBUFFER_RUNTIME_GATE.md" "${ROOT}/docs/testing/MERIDIAN_SE_RINGBUFFER_RUNTIME_GATE.md" ONLY_IF_DIFFERENT)
file(COPY_FILE "${PROJECT_ROOT}/docs/testing/MERIDIAN_VIEW_1_HORDE_RUNTIME_GATE.md" "${ROOT}/docs/testing/MERIDIAN_VIEW_1_HORDE_RUNTIME_GATE.md" ONLY_IF_DIFFERENT)
file(COPY_FILE "${PROJECT_ROOT}/docs/testing/MERIDIAN_NIF_ACTOR_APPEARANCE_RUNTIME_GATE.md" "${ROOT}/docs/testing/MERIDIAN_NIF_ACTOR_APPEARANCE_RUNTIME_GATE.md" ONLY_IF_DIFFERENT)

file(GLOB api_headers LIST_DIRECTORIES false "${API_SOURCE}/*.h")
foreach(header IN LISTS api_headers)
    get_filename_component(header_name "${header}" NAME)
    file(COPY_FILE "${header}" "${ROOT}/SDK/MeridianUIAPI/${header_name}" ONLY_IF_DIFFERENT)
endforeach()
file(COPY_FILE "${GENERATED_VERSION}" "${ROOT}/SDK/MeridianUIAPI/Version.h" ONLY_IF_DIFFERENT)

set(packages
    cef-prebuilt
    commonlibsse-ng
    directxmath
    directxtk
    fmt
    nlohmann-json
    PalSigslot
    rapidcsv
    simpleini
    spdlog
    toml11
    xbyak
)
foreach(package IN LISTS packages)
    set(copyright_file "${VCPKG_LICENSE_ROOT}/${package}/copyright")
    if(NOT EXISTS "${copyright_file}")
        message(FATAL_ERROR "Required third-party license is missing: ${copyright_file}")
    endif()
    string(TOLOWER "${package}" output_name)
    file(COPY_FILE "${copyright_file}" "${ROOT}/licenses/${output_name}.txt" ONLY_IF_DIFFERENT)
endforeach()

message(STATUS "Staged Meridian SDK, guides, and license notices: ${ROOT}")
