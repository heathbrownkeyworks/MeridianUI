function(set_compile_options target)
    target_compile_definitions(${target} PRIVATE WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
    target_compile_features(
        ${target}
        PRIVATE
            cxx_std_23
    )

    target_compile_options(
        ${target}
        PRIVATE
            "/EHsc"     # C++ exception handling model
            "/W4"       # Warning level 4
            "/sdl"      # Enable Additional Security Checks
            "/utf-8"    # Set Source and Executable character sets to UTF-8
            "/Zi"       # Debug Information Format

            "/permissive-"          # Standards conformance
            "/MP"

            "/Zc:alignedNew"        # C++17 over-aligned allocation
            #"/Zc:auto"              # Enforce 'auto' type deduction
            "/Zc:__cplusplus"       # Correct __cplusplus macro value
            #"/Zc:externC"           # Enforce extern "C" linkage rules
            "/Zc:externConstexpr"   # Enforce extern constexpr rules
            #"/Zc:forScope"          # Enforce for-loop scope rules
            #"/Zc:hiddenFriend"      # Enforce hidden friend functions
            #"/Zc:implicitNoexcept"  # Enforce implicit noexcept
            #"/Zc:lambda"            # Enforce lambda rules
            #"/Zc:noexceptTypes"     # Enforce noexcept type rules
            "/Zc:preprocessor"      # Enforce preprocessor rules
            #"/Zc:referenceBinding"  # Enforce reference binding rules
            #"/Zc:rvalueCast"        # Enforce rvalue cast rules
            #"/Zc:sizedDealloc"      # Enforce sized deallocation
            #"/Zc:strictStrings"     # Enforce strict string rules
            #"/Zc:ternary"           # Enforce ternary operator rules
            "/Zc:threadSafeInit"    # Enforce thread-safe initialization
            #"/Zc:trigraphs"         # Enforce trigraph rules
            "/Zc:wchar_t" # Enforce wchar_t rules

            "/wd4200" # nonstandard extension used : zero-sized array in struct/union
            "/wd4100" # unreferenced formal parameter
            #"/WX-" # treats warning as error OFF

            "$<$<CONFIG:DEBUG>:>"
            "$<$<CONFIG:RELEASE>:/Zc:inline;/JMC-;/Ob3;>"
    )

    if(MSVC)
        set_target_properties(
            ${target}
            PROPERTIES
            PDB_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/symbols/$<CONFIG>"
        )

        target_link_options(
            ${target}
            PRIVATE
                "/ignore:4099" # libcef_dll_wrapper.lib ships no PDB; harmless with our own /DEBUG builds
                "$<$<CONFIG:DEBUG>:/INCREMENTAL;/OPT:NOREF;/OPT:NOICF;/DEBUG:FULL;>"
                "$<$<CONFIG:RELEASE>:/INCREMENTAL:NO;/OPT:REF;/OPT:ICF;/DEBUG:FULL;>"
        )

        target_compile_definitions(
	        ${target}
	        PUBLIC
                SKYRIM_SUPPORT_AE
        )
    endif()
endfunction()

function(set_external_project_options target)
    set_target_properties(
        ${target}
        PROPERTIES FOLDER
            ${PROJECT_FOLDER_EXTERNAL}
    )

    if(MSVC)
        set_target_properties(
            ${target}
            PROPERTIES MSVC_RUNTIME_LIBRARY 
                "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        )
    endif()

    target_compile_options(
        ${target}
        PRIVATE
            "/EHsc"
            "/MP"
            "/W4"
            "/WX"
            "/external:W0"
    )

    if (WIN32)
        target_compile_definitions(
          ${target}
          PRIVATE
            WIN32
        )
    endif()
endfunction()

# Apply LTO flags (/GL compile + /LTCG link) to a specific target for Release builds.
# Call this after defining each target that should use link-time optimization.
function(enable_lto_for_target target)
    if(MSVC AND ENABLE_LTO)
        target_compile_options(${target} PRIVATE $<$<CONFIG:Release>:/GL>)
        target_link_options(${target} PRIVATE $<$<CONFIG:Release>:/LTCG>)
    endif()
endfunction()
