# Copyright (c) 2026 Kyland Inc.
# Intewell-RTOS is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

if(NOT DEFINED OS_BOARD)
    message(FATAL_ERROR "[OS] OS_BOARD not define")
endif()

if(NOT DEFINED CONFIG_BOARD_PATH OR CONFIG_BOARD_PATH STREQUAL "")
    message(FATAL_ERROR "CONFIG_BOARD_PATH is not set")
endif()

cmake_path(SET CUR_DIR   NORMALIZE "${CMAKE_CURRENT_SOURCE_DIR}")
cmake_path(SET BOARD_DIR NORMALIZE "${CMAKE_SOURCE_DIR}/${CONFIG_BOARD_PATH}")

cmake_path(IS_PREFIX CUR_DIR "${BOARD_DIR}" NORMALIZE IS_SUBDIR)


if(IS_SUBDIR OR CUR_DIR STREQUAL BOARD_DIR)
    if(NOT EXISTS "${BOARD_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "[OS] board '${BOARD_DIR}' not supported (missing CMakeLists.txt)")
    endif()

    # in-tree
    message(STATUS "[OS] Board: ${OS_BOARD}")
    message(STATUS "[OS] CUR_DIR: ${CUR_DIR}")
    message(STATUS "[OS] BOARD_DIR: ${BOARD_DIR}")
    add_subdirectory("${BOARD_DIR}")
endif()
