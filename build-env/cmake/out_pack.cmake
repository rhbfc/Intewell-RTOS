# Copyright (c) 2026 Kyland Inc.
# Intewell-RTOS is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

include_guard(GLOBAL)

set(OS_OUT_ROOT_DIR "${CMAKE_SOURCE_DIR}/out" CACHE PATH "Root directory for packaged outputs")

if(DEFINED CMAKE_PRESET_NAME AND NOT "${CMAKE_PRESET_NAME}" STREQUAL "")
    set(OS_OUT_PRESET "${CMAKE_PRESET_NAME}")
elseif(DEFINED CONFIG_BOARD AND NOT "${CONFIG_BOARD}" STREQUAL "")
    set(OS_OUT_PRESET "${CONFIG_BOARD}")
else()
    set(OS_OUT_PRESET "default")
endif()

set(OS_OUT_DIR "${OS_OUT_ROOT_DIR}/${OS_OUT_PRESET}")
set(OS_OUT_ARCHIVE "${OS_OUT_ROOT_DIR}/${OS_OUT_PRESET}.tar.gz")

set_property(GLOBAL PROPERTY OS_OUT_COPY_TARGETS "")
set_property(GLOBAL PROPERTY OS_OUT_DEP_TARGETS "")
set_property(GLOBAL PROPERTY OS_OUT_FILES "")

# Register a built target as packaging artifact.
function(os_out_add_target TARGET_NAME)
    if(NOT TARGET ${TARGET_NAME})
        message(FATAL_ERROR "[mkout] target not found: ${TARGET_NAME}")
    endif()

    get_property(_dep_targets GLOBAL PROPERTY OS_OUT_DEP_TARGETS)
    list(APPEND _dep_targets ${TARGET_NAME})
    list(REMOVE_DUPLICATES _dep_targets)
    set_property(GLOBAL PROPERTY OS_OUT_DEP_TARGETS "${_dep_targets}")

    get_target_property(_target_type ${TARGET_NAME} TYPE)
    if(_target_type STREQUAL "EXECUTABLE"
       OR _target_type STREQUAL "STATIC_LIBRARY"
       OR _target_type STREQUAL "SHARED_LIBRARY"
       OR _target_type STREQUAL "MODULE_LIBRARY")
        get_property(_copy_targets GLOBAL PROPERTY OS_OUT_COPY_TARGETS)
        list(APPEND _copy_targets ${TARGET_NAME})
        list(REMOVE_DUPLICATES _copy_targets)
        set_property(GLOBAL PROPERTY OS_OUT_COPY_TARGETS "${_copy_targets}")
    endif()
endfunction()

# Register an arbitrary file path as packaging artifact.
function(os_out_add_file FILE_PATH)
    if(IS_ABSOLUTE "${FILE_PATH}")
        set(_file "${FILE_PATH}")
    else()
        set(_file "${CMAKE_CURRENT_BINARY_DIR}/${FILE_PATH}")
    endif()

    get_property(_files GLOBAL PROPERTY OS_OUT_FILES)
    list(APPEND _files "${_file}")
    list(REMOVE_DUPLICATES _files)
    set_property(GLOBAL PROPERTY OS_OUT_FILES "${_files}")
endfunction()

# Register artifacts only when running in a tag pipeline.
# Usage:
#   os_out_add_tag_artifacts(TARGETS t1 t2 FILES f1 f2)
function(os_out_add_tag_artifacts)
    set(options)
    set(one_value_args)
    set(multi_value_args TARGETS FILES)
    cmake_parse_arguments(OS_OUT_TAG "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

    if(NOT (DEFINED ENV{CI_COMMIT_TAG} AND NOT "$ENV{CI_COMMIT_TAG}" STREQUAL ""))
        return()
    endif()

    foreach(_target IN LISTS OS_OUT_TAG_TARGETS)
        os_out_add_target(${_target})
    endforeach()

    foreach(_file IN LISTS OS_OUT_TAG_FILES)
        os_out_add_file(${_file})
    endforeach()
endfunction()

# Create mkout target to collect and archive registered artifacts.
function(os_out_setup_target)
    if(TARGET mkout)
        return()
    endif()

    get_property(_copy_targets GLOBAL PROPERTY OS_OUT_COPY_TARGETS)
    get_property(_dep_targets GLOBAL PROPERTY OS_OUT_DEP_TARGETS)
    get_property(_files GLOBAL PROPERTY OS_OUT_FILES)

    if(NOT _copy_targets AND NOT _files)
        message(WARNING "[mkout] no artifacts registered. mkout will only create an empty package.")
    endif()

    set(_mkout_commands
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${OS_OUT_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${OS_OUT_DIR}"
    )

    foreach(_target IN LISTS _copy_targets)
        list(APPEND _mkout_commands
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${_target}>" "${OS_OUT_DIR}/"
        )
    endforeach()

    foreach(_file IN LISTS _files)
        list(APPEND _mkout_commands
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_file}" "${OS_OUT_DIR}/"
        )
    endforeach()

    list(APPEND _mkout_commands
        COMMAND ${CMAKE_COMMAND} -E chdir "${OS_OUT_ROOT_DIR}"
                ${CMAKE_COMMAND} -E tar "cfz" "${OS_OUT_ARCHIVE}" --format=gnutar -- "${OS_OUT_PRESET}"
    )

    add_custom_target(mkout
        ${_mkout_commands}
        COMMENT "[mkout] package outputs to ${OS_OUT_DIR} and ${OS_OUT_ARCHIVE}"
        VERBATIM
    )

    if(_dep_targets)
        add_dependencies(mkout ${_dep_targets})
    endif()
endfunction()
