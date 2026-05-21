function(folios_doxygen_collect_existing output_var)
    set(_entries "")

    foreach(_path IN LISTS ARGN)
        if(EXISTS "${_path}")
            list(APPEND _entries "${_path}")
        endif()
    endforeach()

    set("${output_var}" "${_entries}" PARENT_SCOPE)
endfunction()

function(folios_doxygen_collect_component_includes output_var component arch)
    set(_base "${CMAKE_SOURCE_DIR}/${component}")

    folios_doxygen_collect_existing(
        _entries
        "${_base}/include"
        "${_base}/arch/generic/include"
        "${_base}/arch/${arch}/include"
        "${_base}/arch/${arch}/${TARGET_PLATFORM}/include"
        "${_base}/arch/${arch}/${TARGET_PLATFORM}/${TARGET_FIRMWARE}/include"
    )

    set("${output_var}" "${_entries}" PARENT_SCOPE)
endfunction()

function(folios_doxygen_format_setting output_var setting_name)
    set(_formatted "${setting_name} =")
    list(LENGTH ARGN _count)

    if(_count GREATER 0)
        string(APPEND _formatted " \\\n")
        math(EXPR _last "${_count} - 1")

        foreach(_index RANGE 0 ${_last})
            list(GET ARGN ${_index} _entry)

            if(_index EQUAL _last)
                string(APPEND _formatted "    \"${_entry}\"")
            else()
                string(APPEND _formatted "    \"${_entry}\" \\\n")
            endif()
        endforeach()
    endif()

    set("${output_var}" "${_formatted}" PARENT_SCOPE)
endfunction()

function(folios_doxygen_add_target target_name)
    cmake_parse_arguments(
        ARG
        ""
        "PROJECT_NAME;PROJECT_BRIEF;OUTPUT_DIRECTORY;MAINPAGE"
        "INPUTS;INCLUDE_PATHS;PREDEFINED"
        ${ARGN}
    )

    set(FOLIOS_DOXYGEN_PROJECT_NAME "${ARG_PROJECT_NAME}")
    set(FOLIOS_DOXYGEN_PROJECT_BRIEF "${ARG_PROJECT_BRIEF}")
    set(FOLIOS_DOXYGEN_OUTPUT_DIRECTORY "${ARG_OUTPUT_DIRECTORY}")
    set(FOLIOS_DOXYGEN_MAINPAGE "${ARG_MAINPAGE}")

    set(_inputs ${ARG_INPUTS})
    set(_include_paths ${ARG_INCLUDE_PATHS})
    set(_predefined ${ARG_PREDEFINED})

    list(REMOVE_DUPLICATES _inputs)
    list(REMOVE_DUPLICATES _include_paths)
    list(REMOVE_DUPLICATES _predefined)

    folios_doxygen_format_setting(FOLIOS_DOXYGEN_INPUT INPUT ${_inputs})
    folios_doxygen_format_setting(
        FOLIOS_DOXYGEN_INCLUDE_PATH
        INCLUDE_PATH
        ${_include_paths}
    )
    folios_doxygen_format_setting(FOLIOS_DOXYGEN_PREDEFINED PREDEFINED ${_predefined})

    set(_doxyfile "${CMAKE_CURRENT_BINARY_DIR}/${target_name}.Doxyfile")
    configure_file("${FOLIOS_DOXYFILE_IN}" "${_doxyfile}" @ONLY)

    add_custom_target(
        "${target_name}"
        COMMAND "${DOXYGEN_EXECUTABLE}" "${_doxyfile}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Generating ${ARG_PROJECT_NAME} Doxygen documentation"
        VERBATIM
    )
endfunction()
