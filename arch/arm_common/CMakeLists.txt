# Copyright (c) 2026 Kyland Inc.
# Intewell-RTOS is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

set(LIB_NAME arch_common)

file(GLOB SRC_FILES *.c *.S)

add_library(${LIB_NAME} STATIC ${SRC_FILES})

# 把公共设置加入 OS 体系
target_link_libraries(${LIB_NAME} PRIVATE os_common_interface)

os_register_component(${LIB_NAME})

add_library(${LIB_NAME}_interface INTERFACE)

# 架构私有 flags
target_compile_options(${LIB_NAME}
    PRIVATE
    -fno-omit-frame-pointer
    -fno-builtin
)

file(RELATIVE_PATH ARCH_REL_PATH
     ${CMAKE_SOURCE_DIR}
     ${CMAKE_CURRENT_SOURCE_DIR})

target_include_directories(${LIB_NAME}_interface INTERFACE 
    ${CMAKE_SOURCE_DIR}/include/${ARCH_REL_PATH})

target_link_libraries(os_common_interface INTERFACE ${LIB_NAME}_interface)