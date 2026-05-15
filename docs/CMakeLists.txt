find_package(Doxygen QUIET)

if(DOXYGEN_FOUND)
    include(FetchContent)

    set(FOLIOS_DOXYFILE_IN "${CMAKE_CURRENT_SOURCE_DIR}/doxygen/Doxyfile.in")

    option(
        FOLIOS_DOXYGEN_AWESOME
        "Use doxygen-awesome-css for generated Doxygen HTML"
        ON
    )
    set(
        FOLIOS_DOXYGEN_AWESOME_VERSION
        "v2.4.2"
        CACHE STRING
        "doxygen-awesome-css release tag to download"
    )
    set(
        FOLIOS_DOXYGEN_AWESOME_URL_HASH
        "SHA256=6f527e5733723bfd7243c28f24588dca4714cf977fea6147fdb4ae2050244409"
        CACHE STRING
        "Expected hash for the doxygen-awesome-css release archive"
    )

    set(
        FOLIOS_DOXYGEN_EXTRA_INPUTS
        ""
        CACHE STRING
        "Additional input paths for the main Doxygen target"
    )
    set(
        FOLIOS_DOXYGEN_STRATA_EXTRA_INPUTS
        ""
        CACHE STRING
        "Additional input paths for the Strata Doxygen target"
    )
    set(
        FOLIOS_DOXYGEN_VELLUM_EXTRA_INPUTS
        ""
        CACHE STRING
        "Additional input paths for the Vellum Doxygen target"
    )
    set(
        FOLIOS_DOXYGEN_COMMON_EXTRA_INPUTS
        ""
        CACHE STRING
        "Additional input paths for the Common Doxygen target"
    )
    set(
        FOLIOS_DOXYGEN_EXTRA_PREDEFINED
        ""
        CACHE STRING
        "Additional Doxygen PREDEFINED entries"
    )

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

    set(FOLIOS_DOXYGEN_HTML_EXTRA_STYLESHEET "HTML_EXTRA_STYLESHEET =")

    if(FOLIOS_DOXYGEN_AWESOME)
        set(_doxygen_awesome_css_dir "${CMAKE_CURRENT_BINARY_DIR}/doxygen-awesome-css")

        FetchContent_Populate(
            doxygen_awesome_css
            QUIET
            URL "https://github.com/jothepro/doxygen-awesome-css/archive/refs/tags/${FOLIOS_DOXYGEN_AWESOME_VERSION}.tar.gz"
            URL_HASH "${FOLIOS_DOXYGEN_AWESOME_URL_HASH}"
            SOURCE_DIR "${_doxygen_awesome_css_dir}"
            BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/doxygen-awesome-css-build"
            SUBBUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/doxygen-awesome-css-subbuild"
        )

        folios_doxygen_collect_existing(
            _doxygen_awesome_stylesheets
            "${_doxygen_awesome_css_dir}/doxygen-awesome.css"
            "${_doxygen_awesome_css_dir}/doxygen-awesome-sidebar-only.css"
        )
        list(LENGTH _doxygen_awesome_stylesheets _doxygen_awesome_stylesheet_count)

        if(NOT _doxygen_awesome_stylesheet_count EQUAL 2)
            message(FATAL_ERROR "Failed to locate downloaded doxygen-awesome-css stylesheets")
        endif()

        folios_doxygen_format_setting(
            FOLIOS_DOXYGEN_HTML_EXTRA_STYLESHEET
            HTML_EXTRA_STYLESHEET
            ${_doxygen_awesome_stylesheets}
        )
    endif()

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

    set(FOLIOS_DOXYGEN_PREDEFINED_ENTRIES
        "__in="
        "__out="
        "__inout="
        "__out_optional="
        "__buf="
        "__kernel="
        "__percpu="
        "__iomem="
        "__module="
        "__user="
        "__nocast="
        "__bitwise="
        "__ref_strong="
        "__ref_weak="
        "__ref_borrowed="
        "__ref_internal="
        "__ref_locked="
        "__capability(name)="
        "__guarded_by(x)="
        "__pt_guarded_by(x)="
        "__requires_capability(x)="
        "__acquire_capability(x)="
        "__try_acquire_capability(x)="
        "__release_capability(x)="
        "__no_thread_safety_analysis="
        "__must_hold(x)="
        "__acquires(x)="
        "__cond_acquires(x)="
        "__releases(x)="
        "__always_inline=inline"
        "__always_unused="
        "__unused="
        "__noreturn="
        "__naked="
        "__packed="
        "__format_printf(fmt,chk)="
        "__format_scanf(fmt,chk)="
        "__malloc_like(free_func)="
        "__aligned(n)="
        "__constructor="
        "__destructor="
        "__section(s)="
        "__optimize(s)="
        "__target(s)="
        "__weak="
        "__hot="
        "__cold="
        "__pure="
        "__deprecated="
        "__externally_visible="
        "__sentinel="
        "__warn_unused_result="
        "__nonnull(...)="
        "__arg_nonnull="
        "__returns_nonnull="
        "__alloc_size(...)="
        "__alloc_align(arg_index)="
        "__assume_aligned(align)="
        "__access_read_only(ptr_index,size_index)="
        "__access_write_only(ptr_index,size_index)="
        "__access_read_write(ptr_index,size_index)="
    )
    list(APPEND FOLIOS_DOXYGEN_PREDEFINED_ENTRIES ${FOLIOS_DOXYGEN_EXTRA_PREDEFINED})

    folios_doxygen_collect_existing(
        FOLIOS_DOXYGEN_MAIN_INPUTS
        "${CMAKE_SOURCE_DIR}/docs"
    )
    list(APPEND FOLIOS_DOXYGEN_MAIN_INPUTS ${FOLIOS_DOXYGEN_EXTRA_INPUTS})

    folios_doxygen_collect_component_includes(
        FOLIOS_DOXYGEN_STRATA_INCLUDES
        strata
        "${TARGET_ARCH}"
    )
    folios_doxygen_collect_component_includes(
        FOLIOS_DOXYGEN_VELLUM_INCLUDES
        vellum
        "${VELLUM_TARGET_ARCH}"
    )
    folios_doxygen_collect_existing(
        FOLIOS_DOXYGEN_COMMON_INCLUDES
        "${CMAKE_SOURCE_DIR}/common/include"
    )

    set(FOLIOS_DOXYGEN_STRATA_INPUTS
        "${CMAKE_SOURCE_DIR}/docs/strata"
        ${FOLIOS_DOXYGEN_STRATA_INCLUDES}
    )
    list(APPEND FOLIOS_DOXYGEN_STRATA_INPUTS ${FOLIOS_DOXYGEN_STRATA_EXTRA_INPUTS})

    set(FOLIOS_DOXYGEN_VELLUM_INPUTS
        "${CMAKE_SOURCE_DIR}/docs/vellum"
        ${FOLIOS_DOXYGEN_VELLUM_INCLUDES}
    )
    list(APPEND FOLIOS_DOXYGEN_VELLUM_INPUTS ${FOLIOS_DOXYGEN_VELLUM_EXTRA_INPUTS})

    set(FOLIOS_DOXYGEN_COMMON_INPUTS
        "${CMAKE_SOURCE_DIR}/docs/common"
        ${FOLIOS_DOXYGEN_COMMON_INCLUDES}
    )
    list(APPEND FOLIOS_DOXYGEN_COMMON_INPUTS ${FOLIOS_DOXYGEN_COMMON_EXTRA_INPUTS})

    set(FOLIOS_DOXYGEN_STRATA_INCLUDE_PATHS
        ${FOLIOS_DOXYGEN_STRATA_INCLUDES}
        ${FOLIOS_DOXYGEN_COMMON_INCLUDES}
    )
    set(FOLIOS_DOXYGEN_VELLUM_INCLUDE_PATHS
        ${FOLIOS_DOXYGEN_VELLUM_INCLUDES}
        ${FOLIOS_DOXYGEN_COMMON_INCLUDES}
    )

    folios_doxygen_add_target(
        docs-doxygen
        PROJECT_NAME "foliOS"
        PROJECT_BRIEF "foliOS architecture and design documentation"
        OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/docs/doxygen"
        MAINPAGE "${CMAKE_SOURCE_DIR}/docs/index.md"
        INPUTS ${FOLIOS_DOXYGEN_MAIN_INPUTS}
        PREDEFINED ${FOLIOS_DOXYGEN_PREDEFINED_ENTRIES}
    )

    folios_doxygen_add_target(
        docs-doxygen-strata
        PROJECT_NAME "foliOS Strata"
        PROJECT_BRIEF "Strata kernel API reference"
        OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/docs/doxygen/strata"
        MAINPAGE "${CMAKE_SOURCE_DIR}/docs/strata/index.md"
        INPUTS ${FOLIOS_DOXYGEN_STRATA_INPUTS}
        INCLUDE_PATHS ${FOLIOS_DOXYGEN_STRATA_INCLUDE_PATHS}
        PREDEFINED ${FOLIOS_DOXYGEN_PREDEFINED_ENTRIES}
    )

    folios_doxygen_add_target(
        docs-doxygen-vellum
        PROJECT_NAME "foliOS Vellum"
        PROJECT_BRIEF "Vellum bootloader API reference"
        OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/docs/doxygen/vellum"
        MAINPAGE "${CMAKE_SOURCE_DIR}/docs/vellum/index.md"
        INPUTS ${FOLIOS_DOXYGEN_VELLUM_INPUTS}
        INCLUDE_PATHS ${FOLIOS_DOXYGEN_VELLUM_INCLUDE_PATHS}
        PREDEFINED ${FOLIOS_DOXYGEN_PREDEFINED_ENTRIES}
    )

    folios_doxygen_add_target(
        docs-doxygen-common
        PROJECT_NAME "foliOS Common"
        PROJECT_BRIEF "Common boot protocol and shared API reference"
        OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/docs/doxygen/common"
        MAINPAGE "${CMAKE_SOURCE_DIR}/docs/common/index.md"
        INPUTS ${FOLIOS_DOXYGEN_COMMON_INPUTS}
        INCLUDE_PATHS ${FOLIOS_DOXYGEN_COMMON_INCLUDES}
        PREDEFINED ${FOLIOS_DOXYGEN_PREDEFINED_ENTRIES}
    )

    add_custom_target(
        docs-doxygen-all
        DEPENDS
            docs-doxygen
            docs-doxygen-strata
            docs-doxygen-vellum
            docs-doxygen-common
    )
endif()
