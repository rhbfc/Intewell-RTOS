# Copyright (c) 2026 Kyland Inc.
# Intewell-RTOS is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

# ============================================================
# Architecture selection
# 用户可在顶层设置： -DCPU_ARCH=arm/aarch64/x86_64
# ============================================================
set(ARCH_DIR ${CMAKE_CURRENT_LIST_DIR}/${CPU_ARCH})

if(NOT EXISTS ${ARCH_DIR}/CMakeLists.txt)
    message(FATAL_ERROR "[OS] arch '${CPU_ARCH}' not supported")
endif()

# ============================================================
# 如果是 ARM（arm, armv7, armv8, aarch64…），则额外添加 arm_common
# ============================================================
if(CPU_ARCH MATCHES "^arm" OR CPU_ARCH MATCHES "aarch64")
    message(STATUS "[OS] ARCH '${CPU_ARCH}' is ARM family → enabling arm_common")

    # 使用 arch 目录下的 arm_common（相对路径）
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/arm_common")
endif()

# 添加选定架构目录
add_subdirectory(${ARCH_DIR})
