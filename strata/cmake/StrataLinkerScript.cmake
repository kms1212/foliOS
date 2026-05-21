function(strata_collect_linker_script_dependencies output_var)
    set(_dependencies)

    foreach(_path IN LISTS ARGN)
        if(EXISTS "${_path}")
            list(APPEND _dependencies "${_path}")
        endif()
    endforeach()

    set("${output_var}" "${_dependencies}" PARENT_SCOPE)
endfunction()

function(strata_preprocess_linker_script output_var source_file)
    get_filename_component(_source_name "${source_file}" NAME)
    set(_output_file "${CMAKE_CURRENT_BINARY_DIR}/${_source_name}.pp")

    strata_collect_linker_script_dependencies(
        _layout_dependencies
        "${CMAKE_SOURCE_DIR}/arch/${TARGET_ARCH}/${TARGET_PLATFORM}/include/strata/plat/image_layout.h"
        "${CMAKE_SOURCE_DIR}/arch/${TARGET_ARCH}/${TARGET_PLATFORM}/include/strata/plat/memmap.h"
        "${CMAKE_SOURCE_DIR}/arch/${TARGET_ARCH}/include/strata/arch/mmu_constants.h"
    )

    add_custom_command(
        OUTPUT "${_output_file}"
        COMMAND "${CMAKE_C_COMPILER}"
                -E
                -P
                -x assembler-with-cpp
                -I "${CMAKE_SOURCE_DIR}/arch/${TARGET_ARCH}/${TARGET_PLATFORM}/include"
                -I "${CMAKE_SOURCE_DIR}/arch/${TARGET_ARCH}/include"
                -I "${CMAKE_SOURCE_DIR}/include"
                -I "${ROOT_SOURCE_DIR}/common/include"
                "${source_file}"
                -o "${_output_file}"
        DEPENDS "${source_file}" ${_layout_dependencies}
        VERBATIM
    )

    set("${output_var}" "${_output_file}" PARENT_SCOPE)
endfunction()

function(strata_target_linker_script target source_file)
    get_filename_component(_source_name "${source_file}" NAME_WE)
    string(MAKE_C_IDENTIFIER "${target}_${_source_name}_ldscript" _linker_script_target)

    strata_preprocess_linker_script(_linker_script "${source_file}")

    add_custom_target("${_linker_script_target}" DEPENDS "${_linker_script}")
    add_dependencies("${target}" "${_linker_script_target}")
    target_link_options("${target}" PRIVATE -T "${_linker_script}")
    set_property(TARGET "${target}" APPEND PROPERTY LINK_DEPENDS "${_linker_script}")
endfunction()
