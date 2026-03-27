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
# Kconfig integration (final, conflict-free)
#
# 设计原则：
# 1. .config 只有一个 producer（defconfig）
# 2. menuconfig 只是修改者，不声明 OUTPUT
# 3. autoconfig.h 只依赖 .config
# ============================================================

# ------------------------------------------------------------
# 路径定义
# ------------------------------------------------------------
set(CONFIG_DOT   ${CMAKE_BINARY_DIR}/.config)
set(AUTOCONFIG_H ${CMAKE_BINARY_DIR}/autoconfig.h)

# ------------------------------------------------------------
# defconfig
#   - 唯一声明 .config 产物
#   - 用于初始化 / 重置配置
# ------------------------------------------------------------
add_custom_target(defconfig
    COMMAND ${CMAKE_COMMAND} -E env
        KCONFIG_BASE=${CMAKE_BINARY_DIR}
        srctree=${CMAKE_SOURCE_DIR}
        KCONFIG_CONFIG=${CONFIG_DOT}
        BOARD_PATH=${CONFIG_BOARD_PATH}
        python3 ${TOOLS_PATH}/Kconfiglib/defconfig.py
                --kconfig   ${CMAKE_SOURCE_DIR}/Kconfig
                 ${CMAKE_SOURCE_DIR}/boards/configs/${CONFIG_FILE}

    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "[Kconfig] Applying defconfig"
)

# ------------------------------------------------------------
# menuconfig
#   - 交互式修改 .config
#   - 不声明 OUTPUT（否则会和 defconfig 冲突）
# ------------------------------------------------------------
add_custom_target(ui_menuconfig
    COMMAND ${CMAKE_COMMAND} -E env
        KCONFIG_BASE=${CMAKE_BINARY_DIR}
        srctree=${CMAKE_SOURCE_DIR}
        KCONFIG_CONFIG=${CONFIG_DOT}
        BOARD_PATH=${CONFIG_BOARD_PATH}
        python3 ${TOOLS_PATH}/Kconfiglib/menuconfig.py
                ${CMAKE_SOURCE_DIR}/Kconfig

    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    USES_TERMINAL
    COMMENT "[Kconfig] Menu configuration"
)

# ------------------------------------------------------------
# autoconfig.h
#   - 从 .config 生成 C 头文件
# ------------------------------------------------------------
add_custom_command(
    OUTPUT ${AUTOCONFIG_H}
    COMMAND ${CMAKE_COMMAND} -E env
        KCONFIG_BASE=${CMAKE_BINARY_DIR}
        srctree=${CMAKE_SOURCE_DIR}
        KCONFIG_CONFIG=${CONFIG_DOT}
        BOARD_PATH=${CONFIG_BOARD_PATH}
        python3 ${TOOLS_PATH}/Kconfiglib/genconfig.py
                ${CMAKE_SOURCE_DIR}/Kconfig
                --header-path ${AUTOCONFIG_H}

    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    DEPENDS ${CONFIG_DOT}
    COMMENT "[Kconfig] Generating autoconfig.h"
)

add_custom_target(gen-autoconfig-header
    DEPENDS ${AUTOCONFIG_H}
)


add_custom_target(reconfigure
    COMMAND ${CMAKE_COMMAND}
        -S ${CMAKE_SOURCE_DIR}
        -B ${CMAKE_BINARY_DIR}
    COMMENT "[CMake] Re-configuring due to config change"
)


# ------------------------------------------------------------
# menuconfig
#   - 交互式修改 .config
#   - 完成后自动生成 autoconfig.h
# ------------------------------------------------------------
add_custom_target(menuconfig
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target ui_menuconfig
    
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target gen-autoconfig-header

    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target reconfigure

    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    USES_TERMINAL
    COMMENT "[Kconfig] Menu configuration with auto-update"
)

# ------------------------------------------------------------
# savedefconfig
#   - 从当前 .config 生成最小 defconfig
#   - 用于保存配置结果
# ------------------------------------------------------------
add_custom_target(savedefconfig
    COMMAND ${CMAKE_COMMAND} -E env
        KCONFIG_BASE=${CMAKE_SOURCE_DIR}
        srctree=${CMAKE_SOURCE_DIR}
        python3 ${TOOLS_PATH}/Kconfiglib/savedefconfig.py
                ${CMAKE_SOURCE_DIR}/Kconfig
                ${CONFIG_DOT}
                ${CMAKE_SOURCE_DIR}/boards/configs/${CONFIG_FILE}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    DEPENDS ${CONFIG_DOT}
    COMMENT "[Kconfig] Saving minimal defconfig to boards/configs/${CONFIG_FILE}"
)

install(
    FILES ${CMAKE_BINARY_DIR}/autoconfig.h
    DESTINATION kconfig
    COMPONENT sdk
)

# ------------------------------------------------------------
# 提示信息
# ------------------------------------------------------------
message(STATUS "[Kconfig] .config path      = ${CONFIG_DOT}")
message(STATUS "[Kconfig] autoconfig.h path = ${AUTOCONFIG_H}")
