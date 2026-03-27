# Copyright (c) 2026 Kyland Inc.
# Intewell-RTOS is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

set(LIB_NAME arch_${CPU_ARCH})

file(GLOB SRC_FILES CONFIGURE_DEPENDS *.c *.S)

file(RELATIVE_PATH ARCH_REL_PATH
     ${CMAKE_SOURCE_DIR}
     ${CMAKE_CURRENT_SOURCE_DIR})

# ============================================================
# ========== 1) 生成 asm-offsets.h ===========================
# ============================================================

include(${CMAKE_ENV_PATH}/arch_asm_offsets.cmake)

set(ASM_OFFSETS_INCLUDES
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/arch/arm_common
    ${CMAKE_SOURCE_DIR}/include/${ARCH_REL_PATH}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set(ASM_OFFSETS_DEFINES "")
if(CONFIG_HARD_FLOAT)
    list(APPEND ASM_OFFSETS_DEFINES _HARD_FLOAT_)
endif()

# 创建 asm-offsets
os_generate_asm_offsets(
    asm_offsets_${CPU_ARCH}
    ${CMAKE_CURRENT_SOURCE_DIR}/asm-offsets
    ${CMAKE_CURRENT_BINARY_DIR}
    "${ASM_OFFSETS_INCLUDES}"
    "${ASM_OFFSETS_DEFINES}"
)

# ============================================================
# ========== 2) 架构实际源码库 ===============================
# ============================================================

add_library(${LIB_NAME} STATIC ${SRC_FILES})

# 依赖 offsets 生成
add_dependencies(${LIB_NAME} asm_offsets_${CPU_ARCH})

# 让汇编能 include asm-offsets.h
target_include_directories(${LIB_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_BINARY_DIR})

# 架构私有 flags
target_compile_options(${LIB_NAME}
    PRIVATE
        -fno-omit-frame-pointer
        -fno-builtin
)

# # 架构私有宏
# target_compile_definitions(${LIB_NAME}
#     PRIVATE
#         OS_ARCH_${CPU_ARCH}=1
# )

if(CONFIG_HARD_FLOAT)
    message(STATUS "CONFIG_HARD_FLOAT enable")
    target_compile_definitions(${LIB_NAME} PRIVATE _HARD_FLOAT_)
endif() 

# 继承 OS 的公共 flags/定义/头文件
target_link_libraries(${LIB_NAME} PUBLIC os_common_interface arch_common)

# 向 OS 注册
os_register_component(${LIB_NAME})


# ============================================================
# ==== 3) 链接脚本给顶层使用 =================================
# ============================================================

set(ARCH_LINKER_SCRIPT
    "${CMAKE_CURRENT_SOURCE_DIR}/link.lds"
    CACHE STRING "Linker script for selected architecture"
)


# ============================================================
# ==== 4) 架构公共 INTERFACE 库 ===============================
# ============================================================

set(ARCH_INTERFACE ${LIB_NAME}_interface)

add_library(${ARCH_INTERFACE} INTERFACE)

target_include_directories(${ARCH_INTERFACE} INTERFACE 
    ${CMAKE_SOURCE_DIR}/include/${ARCH_REL_PATH})

target_compile_definitions(os_module_sdk_interface INTERFACE CPU_BIT=64)


# ---- 公共宏：CPU_BIT ----
target_compile_definitions(${ARCH_INTERFACE} INTERFACE CPU_BIT=64)

target_compile_options(${ARCH_INTERFACE} INTERFACE -fno-pic -fno-pie)
target_link_options(${ARCH_INTERFACE} INTERFACE -no-pie)

# ---- 公共 CPU Flags ----
if(CONFIG_TARGET_CPU_CORTEX_A55)
    target_compile_options(${ARCH_INTERFACE} INTERFACE -march=armv8.2-a)
    target_compile_options(os_module_sdk_interface INTERFACE -march=armv8.2-a)
else()
    target_compile_options(${ARCH_INTERFACE} INTERFACE -march=armv8-a)
    target_compile_options(os_module_sdk_interface INTERFACE -march=armv8-a)
endif()

# ---- 浮点功能限制 ----
if(NOT CONFIG_HARD_FLOAT)
    target_compile_options(${ARCH_INTERFACE} INTERFACE -mgeneral-regs-only)
    target_compile_options(os_module_sdk_interface INTERFACE -mgeneral-regs-only)
else()
    target_compile_definitions(${ARCH_INTERFACE} INTERFACE _HARD_FLOAT_)
    target_compile_definitions(os_module_sdk_interface INTERFACE _HARD_FLOAT_)
endif()

target_link_libraries(os_common_interface INTERFACE ${ARCH_INTERFACE})

# 注册 interface （让 OS 层也能拿到公共宏与 flags）
os_register_component(${ARCH_INTERFACE})

# ============================================================
# ==== 5) vDSO image build ===================================
# ============================================================

add_subdirectory(vdso)
