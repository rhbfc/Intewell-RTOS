# Copyright (c) 2026 Kyland Inc.
# Intewell-RTOS is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

set(CMAKE_ENV_PATH "${CMAKE_SOURCE_DIR}/build-env/cmake")

include(${CMAKE_ENV_PATH}/kernel.cmake)

# 逻辑模块列表（由各子模块注册）
set(KERNEL_MODULES "" CACHE INTERNAL "Kernel module list")

# ============================================
# 扫描 kernel 下所有一级子模块
# ============================================
file(GLOB KERNEL_SUBDIRS RELATIVE
     ${CMAKE_CURRENT_LIST_DIR}
     ${CMAKE_CURRENT_LIST_DIR}/*
)

foreach(sub ${KERNEL_SUBDIRS})
    if(IS_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/${sub}"
       AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/${sub}/CMakeLists.txt")
        add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/${sub})
    endif()
endforeach()

kernel_create_aggregate()

# ============================================
# 你已有的内核公共处理
# ============================================
os_register_component(kernel)

message(STATUS "[kernel] modules = ${KERNEL_MODULES}")
