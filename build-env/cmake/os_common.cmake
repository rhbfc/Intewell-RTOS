# Copyright (c) 2026 Kyland Inc.
# Intewell-RTOS is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

# 全局变量：保存所有组件库名
set(OS_COMPONENT_LIBS "" CACHE INTERNAL "OS component libs")

# ============================================================
# 注册组件：供每个组件调用
# ============================================================
# 用法：
#   os_register_component(scheduler)
#
function(os_register_component COMPONENT_NAME)
    list(APPEND OS_COMPONENT_LIBS ${COMPONENT_NAME})
    set(OS_COMPONENT_LIBS "${OS_COMPONENT_LIBS}" CACHE INTERNAL "OS component libs")
endfunction()

# ============================================================
# OS 公共接口库（唯一的全局注册点）
# ============================================================

add_library(os_common_interface INTERFACE)
add_library(os_module_sdk_interface INTERFACE)

generate_git_version(
    "os"
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}/git_version.h
)
set(OS_GIT_VERSION_TARGET ${GIT_VERSION_TARGET})

# ---- 公共编译选项 ----
target_compile_options(os_common_interface INTERFACE
    -Wall
    -Wextra
    -ffreestanding
    -fno-builtin
    -nostdinc
    -include ${CMAKE_BINARY_DIR}/autoconfig_yaml.h
    -w
    -g
    -O${CONFIG_BUILD_OPTIMIZE}
)

# ---- 公共宏 ----
target_compile_definitions(os_common_interface INTERFACE
    OS_USE_LOG=1
    OS_VERSION=\"1.0.0\"
)


# ---- 公共 include 路径 ----
target_include_directories(os_common_interface INTERFACE
    ${CMAKE_BINARY_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/arch
    ${CMAKE_SOURCE_DIR}/include/linux-comp
)


function(os_generate_linker_script TARGET OUT IN)
    get_filename_component(OUT_DIR ${OUT} DIRECTORY)

    add_custom_command(
        OUTPUT ${OUT}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${OUT_DIR}
        COMMAND ${CMAKE_C_COMPILER}
                -E -P -x c
                -include ${AUTOCONFIG_H}
                -include ${CMAKE_SOURCE_DIR}/include/link.lds.h
                ${IN}
                -o ${OUT}
        DEPENDS ${IN}
        DEPENDS ${AUTOCONFIG_H}
        COMMENT "[LDS] Generating linker script ${OUT}"
        VERBATIM
    )

    add_custom_target(${TARGET}
        DEPENDS ${OUT}
    )
endfunction()


function(show_build_cmd)
    if(CMAKE_GENERATOR MATCHES "Ninja")
        set(cmd "ninja -C ${CMAKE_BINARY_DIR}")
    else()
        set(cmd "make -C ${CMAKE_BINARY_DIR}")
    endif()

    message("\n==================================================")
    message("CMake Generator: ${CMAKE_GENERATOR}")
    message("To build the project, run:")
    message("  ${cmd}")
    message("==================================================\n")
endfunction()
