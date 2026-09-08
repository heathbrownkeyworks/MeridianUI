set(CEF_URL "https://cef-builds.spotifycdn.com/cef_binary_152.0.6%2Bg708dc14%2Bchromium-152.0.7977.83_windows64.tar.bz2")

vcpkg_download_distfile(ARCHIVE
    URLS ${CEF_URL}
    FILENAME "cef152.tar.bz2"
    SHA512 abe1dba41af55705577a915457ef942ee8b8413e9647ce94308938fabb0b0334523f1af3810450f5852ae0e94a563812f711ac61a2cf6fbcc837f954b315214d
)

vcpkg_extract_source_archive_ex(
    OUT_SOURCE_PATH SOURCE_PATH
    ARCHIVE ${ARCHIVE}
)

file(COPY ${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt DESTINATION ${SOURCE_PATH})

vcpkg_configure_cmake(
    SOURCE_PATH ${SOURCE_PATH}
)

vcpkg_install_cmake()
vcpkg_copy_pdbs()

file(INSTALL "${SOURCE_PATH}/include" DESTINATION "${CURRENT_PACKAGES_DIR}")
file(INSTALL "${SOURCE_PATH}/Debug" "${SOURCE_PATH}/Release" "${SOURCE_PATH}/Resources" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
file(INSTALL "${SOURCE_PATH}/LICENSE.txt" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}" RENAME copyright)
