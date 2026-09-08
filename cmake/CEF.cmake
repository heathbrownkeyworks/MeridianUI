# cef -- the CEF runtime: an INTERFACE library that links the prebuilt
# libcef wrapper + libcef.lib, and the CefLibraryFiles target that copies
# the CEF runtime DLLs/resources into the staged Data tree.
# Included from the top-level CMakeLists.txt. Requires the caller to have
# already defined NL_UI_PATH.

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
