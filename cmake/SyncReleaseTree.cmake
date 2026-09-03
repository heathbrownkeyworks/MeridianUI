foreach(required SOURCE DESTINATION)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

cmake_path(ABSOLUTE_PATH SOURCE NORMALIZE OUTPUT_VARIABLE source_abs)
cmake_path(ABSOLUTE_PATH DESTINATION NORMALIZE OUTPUT_VARIABLE destination_abs)
cmake_path(GET destination_abs ROOT_PATH destination_root)
cmake_path(GET destination_abs FILENAME destination_name)
string(TOLOWER "${destination_name}" destination_name_lower)
cmake_path(IS_PREFIX destination_abs "${source_abs}" NORMALIZE destination_contains_source)

if(destination_abs STREQUAL destination_root OR
   destination_abs STREQUAL source_abs OR
   destination_contains_source OR
   NOT destination_name_lower STREQUAL "meridianui")
    message(FATAL_ERROR "Unsafe release-sync destination: ${destination_abs}")
endif()
if(NOT EXISTS "${source_abs}/MeridianUI/MeridianUI.dll" OR
   NOT EXISTS "${source_abs}/SKSE/Plugins/MeridianUIPlugin.dll")
    message(FATAL_ERROR "Release source is missing Meridian production markers: ${source_abs}")
endif()

file(MAKE_DIRECTORY "${destination_abs}")
file(GLOB_RECURSE source_files RELATIVE "${source_abs}" LIST_DIRECTORIES false "${source_abs}/*")
file(GLOB_RECURSE destination_files RELATIVE "${destination_abs}" LIST_DIRECTORIES false "${destination_abs}/*")

foreach(relative IN LISTS destination_files)
    if(NOT relative IN_LIST source_files)
        file(REMOVE "${destination_abs}/${relative}")
    endif()
endforeach()

file(GLOB_RECURSE destination_entries RELATIVE "${destination_abs}" LIST_DIRECTORIES true "${destination_abs}/*")
list(REVERSE destination_entries)
foreach(relative IN LISTS destination_entries)
    if(IS_DIRECTORY "${destination_abs}/${relative}")
        file(GLOB children "${destination_abs}/${relative}/*")
        if(NOT children)
            file(REMOVE_RECURSE "${destination_abs}/${relative}")
        endif()
    endif()
endforeach()

file(COPY "${source_abs}/" DESTINATION "${destination_abs}")

file(GLOB_RECURSE deployed_files RELATIVE "${destination_abs}" LIST_DIRECTORIES false "${destination_abs}/*")
list(SORT source_files)
list(SORT deployed_files)
if(NOT source_files STREQUAL deployed_files)
    message(FATAL_ERROR "Deployed tree does not match staged release manifest")
endif()
message(STATUS "Synchronized ${destination_abs} to ${source_abs}")
