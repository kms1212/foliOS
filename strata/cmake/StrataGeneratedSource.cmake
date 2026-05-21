function(strata_target_dependent_source target output_var output_file source_file)
    get_filename_component(_output_name "${output_file}" NAME_WE)
    string(MAKE_C_IDENTIFIER "${target}_${_output_name}_source" _source_target)

    add_custom_command(
        OUTPUT "${output_file}"
        COMMAND ${CMAKE_COMMAND} -E copy "${source_file}" "${output_file}"
        COMMAND ${CMAKE_COMMAND} -E touch "${output_file}"
        DEPENDS "${source_file}" ${ARGN}
        VERBATIM
    )
    add_custom_target("${_source_target}" DEPENDS "${output_file}")
    add_dependencies("${target}" "${_source_target}")

    set_source_files_properties("${output_file}" PROPERTIES GENERATED TRUE)
    set("${output_var}" "${output_file}" PARENT_SCOPE)
endfunction()
