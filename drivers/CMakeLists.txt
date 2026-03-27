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

include(${CMAKE_ENV_PATH}/driver.cmake)

# ============================================
# Driver 主库（最终聚合所有子模块）
# ============================================
set(DRIVER_MODULES "" CACHE INTERNAL "Driver module list")


# ============================================
# 获取 driver 下面所有一级子模块目录
# ============================================
file(GLOB DRIVER_SUBDIRS RELATIVE 
     ${CMAKE_CURRENT_LIST_DIR}
     ${CMAKE_CURRENT_LIST_DIR}/*)

foreach(sub ${DRIVER_SUBDIRS})
    if(IS_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/${sub}" AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/${sub}/CMakeLists.txt")
        add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/${sub})  # 每个子模块 CMakeLists.txt 内再调用 driver_add_subdir
    endif()
endforeach()

driver_create_aggregate()

# 你已有的内核公共处理
os_register_component(driver)

message(STATUS "[driver] modules = ${DRIVER_MODULES}")
