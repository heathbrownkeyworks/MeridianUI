cmake_minimum_required(VERSION 3.23)
project(cef-prebuilt)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")
set(CEF_ROOT "${CMAKE_SOURCE_DIR}")
find_package(CEF REQUIRED)

# CEF's libcef_dll/CMakeLists.txt defines libcef_dll_wrapper as a *static*
# library by default and expects the consuming target to link libcef.lib
set(BUILD_SHARED_LIBS OFF)
add_subdirectory(${CEF_LIBCEF_DLL_WRAPPER_PATH} libcef_dll_wrapper)

install(TARGETS libcef_dll_wrapper)
