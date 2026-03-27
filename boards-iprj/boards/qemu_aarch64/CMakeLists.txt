# Copyright (c) 2026 Kyland Inc.
# Intewell-RTOS is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

set(LIB_NAME board_${OS_BOARD})

file(GLOB SRC_FILES *.c)

add_library(${LIB_NAME} STATIC ${SRC_FILES})

# 私有 include
target_include_directories(${LIB_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# 私有宏
target_compile_definitions(${LIB_NAME}
    PRIVATE
        OS_BOARD_NAME=\"${OS_BOARD}\"
)

target_link_libraries(${LIB_NAME} PRIVATE os_common_interface)

os_register_component(${LIB_NAME})

include(board.cmake)