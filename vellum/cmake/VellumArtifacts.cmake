function(vellum_add_binary_artifact artifact_target source_target output_file description)
    add_custom_command(
        OUTPUT "${output_file}"
        COMMAND ${CMAKE_OBJCOPY} -O binary "$<TARGET_FILE:${source_target}>" "${output_file}"
        DEPENDS "${source_target}"
        COMMENT "${description}"
        VERBATIM
    )
    add_custom_target("${artifact_target}" ALL DEPENDS "${output_file}")
endfunction()

function(vellum_add_module_artifact module_name)
    set(_module_target "vellum_module_${module_name}")
    set(_module_elf "${CMAKE_CURRENT_BINARY_DIR}/${module_name}.elf")
    set(_module_mod "${CMAKE_CURRENT_BINARY_DIR}/${module_name}.mod")

    add_custom_command(
        OUTPUT "${_module_elf}" "${_module_mod}"
        COMMAND
            ${CMAKE_LD}
                -shared -fno-plt -z now -Bsymbolic
                --emit-relocs --export-dynamic --allow-shlib-undefined
                $<TARGET_OBJECTS:${_module_target}>
                -o "${_module_elf}"
        COMMAND
            ${CMAKE_OBJCOPY}
                --add-section .note.vellum=${CMAKE_CURRENT_SOURCE_DIR}/module_info.bin
                --set-section-flags .note.vellum=contents,noload,readonly
                "${_module_elf}"
                "${_module_mod}"
        COMMAND_EXPAND_LISTS
        DEPENDS
            "${_module_target}"
            $<TARGET_OBJECTS:${_module_target}>
            "${CMAKE_CURRENT_SOURCE_DIR}/module_info.bin"
        COMMENT "Linking target ${_module_target}"
        VERBATIM
    )

    add_custom_target("${_module_target}_mod" ALL DEPENDS "${_module_mod}")
endfunction()
