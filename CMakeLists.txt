# Copyright (c) 2026 Kyland Inc.
# Intewell-RTOS is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

cmake_minimum_required(VERSION 3.19)

set(CMAKE_ENV_PATH  ${CMAKE_SOURCE_DIR}/build-env/cmake)
set(TOOLS_PATH      ${CMAKE_SOURCE_DIR}/build-env/tools)

# Enable generation of compile_commands.json by default for tooling (clangd, etc.).
if(NOT DEFINED CMAKE_EXPORT_COMPILE_COMMANDS)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Export compile commands" FORCE)
endif()

get_property(_harch  CACHE CONFIG_CPU_ARCH   PROPERTY VALUE)
get_property(_sarch  CACHE CONFIG_SOFT_ARCH   PROPERTY VALUE)
get_property(_board CACHE CONFIG_BOARD  PROPERTY VALUE)
set(CPU_ARCH ${_harch}  CACHE INTERNAL "OS hard architecture")
set(OS_ARCH  ${_sarch}  CACHE INTERNAL "OS soft architecture")
set(OS_BOARD ${_board} CACHE INTERNAL "OS board")

# ======================================================
# 处理依赖库
# ======================================================
if(DEFINED BOARD_YAML)
    message(STATUS "Using board config: ${BOARD_YAML}")
    include(${CMAKE_ENV_PATH}/deps.cmake)
    board_fetch_deps(${CMAKE_SOURCE_DIR}/${BOARD_YAML})
endif()


# ======================================================
# 环境初始化
# ======================================================
message(STATUS "CPU_ARCH  = ${CPU_ARCH}")
message(STATUS "OS_ARCH   = ${OS_ARCH}")
message(STATUS "OS_BOARD  = ${OS_BOARD}")
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

if(DEFINED CONFIG_PROJECT_NAME)
    message(STATUS "PROJECT_NAME = ${CONFIG_PROJECT_NAME}")
    set(PROJECT_NAME "${CONFIG_PROJECT_NAME}")
else()
    set(PROJECT_NAME "rtos")
endif()

project(${PROJECT_NAME} C ASM)

include(${CMAKE_ENV_PATH}/top.cmake)

# ======================================================
# 公共配置
# ======================================================

include(${CMAKE_ENV_PATH}/os_common.cmake)
# ======================================================
# 加载 OS 各子系统
# （子目录内部通过 os_register_component 注册 STATIC 库）
# ======================================================
add_subdirectory(dependencies)
add_subdirectory(arch)
add_subdirectory(boards-iprj)
add_subdirectory(boards-prj)
add_subdirectory(kernel)
add_subdirectory(drivers)
add_subdirectory(components)

os_setup_sdk_targets()


# ======================================================
# 最终 OS 静态库（聚合）
# ======================================================
add_library(os STATIC ${CMAKE_ENV_PATH}/empty.c)

# 聚合所有组件 STATIC 库
target_link_libraries(os 
    PUBLIC 
        -Wl,--whole-archive
        fs_interface
        os_common_interface 
        ${OS_COMPONENT_LIBS}
        -Wl,--no-whole-archive
        gcc
        m
        c
)


# ======================================================
# ARCH LDS 
# ======================================================
message(STATUS "[LDS]  ${ARCH_LINKER_SCRIPT}")

os_generate_linker_script(arch_linker_script
    ${CMAKE_BINARY_DIR}/link.lds
    ${ARCH_LINKER_SCRIPT}
)


# ======================================================
# ELF 链接接口（不生成任何文件）
# ======================================================
add_library(kernel_elf_iface INTERFACE)

target_link_libraries(kernel_elf_iface
    INTERFACE
        os
)

target_link_options(kernel_elf_iface
    INTERFACE
        -nostdlib
        -nostartfiles
        -nodefaultlibs
        -Wl,-T${CMAKE_BINARY_DIR}/link.lds
)

add_dependencies(kernel_elf_iface arch_linker_script)


# ======================================================
# 符号表相关
# ======================================================
include(${CMAKE_ENV_PATH}/mksyms.cmake)


# ======================================================
# 目标 ELF
# ======================================================
add_executable(${PROJECT_NAME}.elf
    ${CMAKE_ENV_PATH}/empty.c
)

target_link_libraries(${PROJECT_NAME}.elf
    PRIVATE
        kernel_elf_iface
)

# 顶层依赖所有 mksyms 生成的 INTERFACE target
foreach(iface ${MKSYMS_ALL_TARGETS})
    target_link_libraries(${PROJECT_NAME}.elf PRIVATE ${iface})
endforeach()

target_link_options(${PROJECT_NAME}.elf PRIVATE
    "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.map"
)

# ======================================================
# 顶层统一执行已注册的 ELF 后处理，并生成最终产物
# ======================================================
os_finalize_elf_ops(${PROJECT_NAME}.elf)


# ======================================================
# 产物打包
# ======================================================
os_out_add_target(${PROJECT_NAME}.elf)
os_out_add_file(${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.bin)
os_out_add_file(${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.map)
os_out_setup_target()

# ======================================================
# 调试信息
# ======================================================
message(STATUS "[OS] Components loaded:")
foreach(lib ${OS_COMPONENT_LIBS})
    message(STATUS "  ${lib}")
endforeach()


show_build_cmd()
