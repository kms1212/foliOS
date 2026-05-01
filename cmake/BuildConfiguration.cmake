function(_folios_apply_build_config_var name value type doc)
    get_property(_initial_cache_vars GLOBAL PROPERTY FOLIOS_BUILD_CONFIG_INITIAL_CACHE_VARS)
    get_property(_previous_managed_vars GLOBAL PROPERTY FOLIOS_BUILD_CONFIG_PREVIOUS_MANAGED_VARS)

    list(FIND _initial_cache_vars "${name}" _initial_cache_index)
    list(FIND _previous_managed_vars "${name}" _previous_managed_index)

    if(_initial_cache_index GREATER -1 AND _previous_managed_index EQUAL -1)
        return()
    endif()

    if("${type}" STREQUAL "")
        set(type STRING)
    endif()

    if("${doc}" STREQUAL "")
        set(doc "Build configuration value")
    endif()

    set(${name} "${value}" CACHE ${type} "${doc}" FORCE)

    get_property(_new_managed_vars GLOBAL PROPERTY FOLIOS_BUILD_CONFIG_NEW_MANAGED_VARS)
    list(APPEND _new_managed_vars "${name}")
    list(REMOVE_DUPLICATES _new_managed_vars)
    set_property(GLOBAL PROPERTY FOLIOS_BUILD_CONFIG_NEW_MANAGED_VARS "${_new_managed_vars}")
endfunction()

function(folios_build_config_set name value regex doc)
    _folios_apply_build_config_var("${name}" "${value}" STRING "${doc}")

    if(NOT DEFINED ${name} OR "${${name}}" STREQUAL "")
        if("${doc}" STREQUAL "")
            message(FATAL_ERROR "${name} must be configured")
        else()
            message(FATAL_ERROR "${name} must be configured (${doc})")
        endif()
    endif()

    if(NOT "${regex}" STREQUAL "" AND NOT "${${name}}" MATCHES "${regex}")
        if("${doc}" STREQUAL "")
            message(FATAL_ERROR "${name} must match regex '${regex}'")
        else()
            message(FATAL_ERROR "${name} must match regex '${regex}' (${doc})")
        endif()
    endif()
endfunction()

function(_folios_parse_build_config_file config_file)
    file(STRINGS "${config_file}" _config_lines)

    set(_line_number 0)
    foreach(_raw_line IN LISTS _config_lines)
        math(EXPR _line_number "${_line_number} + 1")

        string(STRIP "${_raw_line}" _line)
        if("${_line}" STREQUAL "" OR "${_line}" MATCHES "^#")
            continue()
        endif()

        if(NOT "${_line}" MATCHES "^set\\((.*)\\)$")
            message(
                FATAL_ERROR
                "Unsupported statement in build configuration file '${config_file}:${_line_number}': ${_line}"
            )
        endif()

        string(REGEX REPLACE "^set\\((.*)\\)$" "\\1" _args "${_line}")
        separate_arguments(_tokens UNIX_COMMAND "${_args}")

        list(LENGTH _tokens _token_count)
        if(_token_count LESS 2)
            message(
                FATAL_ERROR
                "Invalid set() statement in build configuration file '${config_file}:${_line_number}'"
            )
        endif()

        list(GET _tokens 0 _name)
        list(FIND _tokens CACHE _cache_index)

        if(_cache_index EQUAL -1)
            math(EXPR _value_count "${_token_count} - 1")
            list(SUBLIST _tokens 1 ${_value_count} _value_tokens)
            list(JOIN _value_tokens ";" _value)
            _folios_apply_build_config_var("${_name}" "${_value}" STRING "Build configuration value")
            continue()
        endif()

        math(EXPR _value_count "${_cache_index} - 1")
        if(_value_count LESS 1)
            message(
                FATAL_ERROR
                "Invalid CACHE set() statement in build configuration file '${config_file}:${_line_number}'"
            )
        endif()

        math(EXPR _type_index "${_cache_index} + 1")
        math(EXPR _doc_index "${_cache_index} + 2")
        if(_doc_index GREATER_EQUAL _token_count)
            message(
                FATAL_ERROR
                "CACHE set() statement missing type or doc string in '${config_file}:${_line_number}'"
            )
        endif()

        list(SUBLIST _tokens 1 ${_value_count} _value_tokens)
        list(JOIN _value_tokens ";" _value)

        list(GET _tokens ${_type_index} _type)

        math(EXPR _doc_count "${_token_count} - ${_doc_index}")
        list(SUBLIST _tokens ${_doc_index} ${_doc_count} _doc_tokens)
        string(JOIN " " _doc ${_doc_tokens})

        _folios_apply_build_config_var("${_name}" "${_value}" "${_type}" "${_doc}")
    endforeach()
endfunction()

function(_folios_append_build_config_candidate out_var directory preset_name)
    set(_files "${${out_var}}")
    set(_default_file "${directory}/default.cmake")

    if(EXISTS "${_default_file}")
        list(APPEND _files "${_default_file}")
    endif()

    if(NOT "${preset_name}" STREQUAL "default")
        set(_preset_file "${directory}/${preset_name}.cmake")
        if(EXISTS "${_preset_file}")
            list(APPEND _files "${_preset_file}")
        endif()
    endif()

    set(${out_var} "${_files}" PARENT_SCOPE)
endfunction()

function(load_build_configuration target_config_arch target_platform target_firmware config_preset)
    get_cmake_property(_initial_cache_vars CACHE_VARIABLES)
    set(_previous_managed_vars "${FOLIOS_CONFIG_MANAGED_CACHE_VARS}")
    set(_config_files)

    set_property(GLOBAL PROPERTY FOLIOS_BUILD_CONFIG_INITIAL_CACHE_VARS "${_initial_cache_vars}")
    set_property(GLOBAL PROPERTY FOLIOS_BUILD_CONFIG_PREVIOUS_MANAGED_VARS "${_previous_managed_vars}")
    set_property(GLOBAL PROPERTY FOLIOS_BUILD_CONFIG_NEW_MANAGED_VARS "")

    _folios_append_build_config_candidate(
        _config_files
        "${CMAKE_SOURCE_DIR}/config/${target_config_arch}"
        "${config_preset}"
    )
    _folios_append_build_config_candidate(
        _config_files
        "${CMAKE_SOURCE_DIR}/config/${target_config_arch}/${target_platform}"
        "${config_preset}"
    )
    _folios_append_build_config_candidate(
        _config_files
        "${CMAKE_SOURCE_DIR}/config/${target_config_arch}/${target_platform}/${target_firmware}"
        "${config_preset}"
    )

    foreach(_config_file IN LISTS _config_files)
        message(STATUS "Loading build configuration: ${_config_file}")
        _folios_parse_build_config_file("${_config_file}")
    endforeach()

    get_property(_new_managed_vars GLOBAL PROPERTY FOLIOS_BUILD_CONFIG_NEW_MANAGED_VARS)

    foreach(_old_var IN LISTS _previous_managed_vars)
        list(FIND _new_managed_vars "${_old_var}" _still_managed_index)
        if(_still_managed_index EQUAL -1)
            unset(${_old_var} CACHE)
        endif()
    endforeach()

    set(
        FOLIOS_CONFIG_MANAGED_CACHE_VARS
        "${_new_managed_vars}"
        CACHE INTERNAL
        "Cache variables managed by build configuration files"
        FORCE
    )
endfunction()

function(load_component_build_configuration component_name)
    set(_config_file "${CMAKE_SOURCE_DIR}/config/components/${component_name}.cmake")

    if(NOT EXISTS "${_config_file}")
        message(FATAL_ERROR "Missing component build configuration file: ${_config_file}")
    endif()

    message(STATUS "Loading component build configuration: ${_config_file}")
    include("${_config_file}")

    get_property(_new_managed_vars GLOBAL PROPERTY FOLIOS_BUILD_CONFIG_NEW_MANAGED_VARS)
    set(
        FOLIOS_CONFIG_MANAGED_CACHE_VARS
        "${_new_managed_vars}"
        CACHE INTERNAL
        "Cache variables managed by build configuration files"
        FORCE
    )
endfunction()

function(get_build_configuration_cache_args out_var)
    set(_cache_args)

    foreach(_var IN LISTS FOLIOS_CONFIG_MANAGED_CACHE_VARS)
        get_property(_type CACHE "${_var}" PROPERTY TYPE)
        if(NOT _type)
            set(_type STRING)
        endif()

        list(APPEND _cache_args "-D${_var}:${_type}=${${_var}}")
    endforeach()

    set(${out_var} "${_cache_args}" PARENT_SCOPE)
endfunction()
