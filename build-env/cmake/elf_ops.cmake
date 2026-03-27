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

set_property(GLOBAL PROPERTY OS_ELF_OP_COUNT 0)

function(os_register_elf_op)
    set(options)
    set(one_value_args NAME COMMENT PRIORITY)
    set(multi_value_args COMMAND)
    cmake_parse_arguments(OS_ELF_OP "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

    if(NOT OS_ELF_OP_NAME)
        message(FATAL_ERROR "[elf_ops] NAME is required")
    endif()

    if(NOT OS_ELF_OP_COMMAND)
        message(FATAL_ERROR "[elf_ops] COMMAND is required for '${OS_ELF_OP_NAME}'")
    endif()

    if(NOT DEFINED OS_ELF_OP_PRIORITY OR OS_ELF_OP_PRIORITY STREQUAL "")
        set(OS_ELF_OP_PRIORITY 100)
    endif()

    if(NOT OS_ELF_OP_PRIORITY MATCHES "^[0-9]+$")
        message(FATAL_ERROR "[elf_ops] PRIORITY for '${OS_ELF_OP_NAME}' must be a non-negative integer")
    endif()

    get_property(_count GLOBAL PROPERTY OS_ELF_OP_COUNT)
    if(NOT _count)
        set(_count 0)
    endif()

    set_property(GLOBAL PROPERTY OS_ELF_OP_${_count}_NAME "${OS_ELF_OP_NAME}")
    set_property(GLOBAL PROPERTY OS_ELF_OP_${_count}_COMMENT "${OS_ELF_OP_COMMENT}")
    set_property(GLOBAL PROPERTY OS_ELF_OP_${_count}_PRIORITY "${OS_ELF_OP_PRIORITY}")
    set_property(GLOBAL PROPERTY OS_ELF_OP_${_count}_SORT_KEY "${OS_ELF_OP_PRIORITY}.${_count}")
    set_property(GLOBAL PROPERTY OS_ELF_OP_${_count}_COMMAND "${OS_ELF_OP_COMMAND}")

    math(EXPR _next_count "${_count} + 1")
    set_property(GLOBAL PROPERTY OS_ELF_OP_COUNT ${_next_count})
endfunction()

function(os_finalize_elf_ops TARGET_NAME)
    if(NOT TARGET ${TARGET_NAME})
        message(FATAL_ERROR "[elf_ops] target not found: ${TARGET_NAME}")
    endif()

    get_property(_count GLOBAL PROPERTY OS_ELF_OP_COUNT)
    if(NOT _count)
        set(_count 0)
    endif()

    set(_final_elf "$<TARGET_FILE:${TARGET_NAME}>")
    set(_current_input "${_final_elf}")
    set(_temp_dir "${CMAKE_CURRENT_BINARY_DIR}/elf_ops")
    set(_byproducts "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.bin")
    set(_commands "")
    set(_comment "[OS] Generating ${PROJECT_NAME}.bin from ${PROJECT_NAME}.elf")

    if(_count GREATER 0)
        set(_commands
            COMMAND ${CMAKE_COMMAND} -E make_directory ${_temp_dir}
            COMMAND /bin/date "+[elf_ops] build date: %F %T %Z"
        )
        set(_comment "[OS] Running registered ELF post-processors and generating ${PROJECT_NAME}.bin")
        math(EXPR _last_index "${_count} - 1")
        set(_ordered_ops "")
        foreach(_idx RANGE ${_last_index})
            get_property(_sort_key GLOBAL PROPERTY OS_ELF_OP_${_idx}_SORT_KEY)
            list(APPEND _ordered_ops "${_sort_key}|${_idx}")
        endforeach()

        list(SORT _ordered_ops COMPARE NATURAL ORDER ASCENDING)

        set(_exec_idx 0)
        foreach(_entry IN LISTS _ordered_ops)
            string(REGEX REPLACE "^[^|]*\\|" "" _idx "${_entry}")
            get_property(_name GLOBAL PROPERTY OS_ELF_OP_${_idx}_NAME)
            get_property(_comment GLOBAL PROPERTY OS_ELF_OP_${_idx}_COMMENT)
            get_property(_priority GLOBAL PROPERTY OS_ELF_OP_${_idx}_PRIORITY)
            get_property(_raw_command GLOBAL PROPERTY OS_ELF_OP_${_idx}_COMMAND)

            set(_output "${_temp_dir}/${PROJECT_NAME}.${_exec_idx}.${_name}.elf")
            list(APPEND _byproducts "${_output}")

            list(APPEND _commands
                COMMAND ${CMAKE_COMMAND} -E echo "[elf_ops] priority=${_priority} name=${_name}"
            )

            set(_expanded_command "")
            foreach(_arg IN LISTS _raw_command)
                set(_value "${_arg}")
                string(REPLACE "@INPUT@" "${_current_input}" _value "${_value}")
                string(REPLACE "@OUTPUT@" "${_output}" _value "${_value}")
                string(REPLACE "@TARGET@" "${_final_elf}" _value "${_value}")
                list(APPEND _expanded_command "${_value}")
            endforeach()

            list(APPEND _commands COMMAND ${_expanded_command})
            set(_current_input "${_output}")
            math(EXPR _exec_idx "${_exec_idx} + 1")
        endforeach()
    endif()

    if(_count GREATER 0)
        list(APPEND _commands
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_current_input} ${_final_elf}
        )
    endif()

    list(APPEND _commands
        COMMAND ${CMAKE_OBJCOPY} -O binary
                ${_final_elf}
                ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.bin
    )

    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        ${_commands}
        BYPRODUCTS ${_byproducts}
        COMMENT "${_comment}"
        VERBATIM
    )
endfunction()
