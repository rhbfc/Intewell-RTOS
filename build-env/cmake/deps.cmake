# Copyright (c) 2026 Kyland Inc.
# Intewell-RTOS is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

# 调用示例: board_fetch_deps(${BOARD_CONFIG_FILE})
function(board_fetch_deps board_config_file)
    if(NOT EXISTS "${board_config_file}")
        message(FATAL_ERROR "Board config file not found: ${board_config_file}")
    endif()

    set(deps_args ${board_config_file})
    if("$ENV{UPBOARD_DEPS_SYNC_REMOTE}" STREQUAL "1")
        list(APPEND deps_args --force)
    endif()

    # 调用 Python 脚本处理 deps
    execute_process(
        COMMAND python3 ${TOOLS_PATH}/deps.py ${deps_args}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        RESULT_VARIABLE result
        COMMAND_ECHO STDOUT
        ERROR_VARIABLE err_output
    )

    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Failed to fetch deps for ${board_config_file}\n${err_output}")
    endif()
endfunction()


add_custom_target(force_up_deps
    COMMAND ${CMAKE_COMMAND} -E echo "[deps] force updating dependencies"
    COMMAND python3 ${TOOLS_PATH}/deps.py ${CMAKE_SOURCE_DIR}/${BOARD_YAML} --force
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    USES_TERMINAL
    VERBATIM
)
