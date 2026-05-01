function(process_target target_var)
    if(NOT DEFINED ${target_var})
        message(FATAL_ERROR "Target variable '${target_var}' is not defined")
    endif()

    set(_target "${${target_var}}")

    string(REPLACE "-" ";" _target_parts "${_target}")
    list(LENGTH _target_parts _target_parts_len)
    if(NOT _target_parts_len EQUAL 3)
        message(FATAL_ERROR "Invalid target format: '${_target}'")
    endif()

    list(GET _target_parts 0 _target_arch)
    list(GET _target_parts 1 _target_platform)
    list(GET _target_parts 2 _target_firmware)

    set(_target_config_arch "${_target_arch}")

    set(TARGET_ARCH "${_target_arch}" PARENT_SCOPE)
    set(TARGET_PLATFORM "${_target_platform}" PARENT_SCOPE)
    set(TARGET_FIRMWARE "${_target_firmware}" PARENT_SCOPE)
    set(TARGET_CONFIG_ARCH "${_target_config_arch}" PARENT_SCOPE)
endfunction()
