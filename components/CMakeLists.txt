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

include(${CMAKE_ENV_PATH}/component.cmake)

# ============================================
# Component 主库（最终聚合所有子模块）
# ============================================
set(COMPONENT_MODULES "" CACHE INTERNAL "Component module list")


# ============================================
# 获取 component 下面所有一级子模块目录
# ============================================
file(GLOB COMPONENT_SUBDIRS RELATIVE 
     ${CMAKE_CURRENT_LIST_DIR} 
     ${CMAKE_CURRENT_LIST_DIR}/*)

foreach(sub ${COMPONENT_SUBDIRS})
    if(IS_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/${sub}" AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/${sub}/CMakeLists.txt")
        add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/${sub})  # 每个子模块 CMakeLists.txt 内再调用 component_add_subdir
    endif()
endforeach()

component_create_aggregate()

