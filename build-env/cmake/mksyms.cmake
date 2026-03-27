# Copyright (c) 2026 Kyland Inc.
# Intewell-RTOS is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

# ======================================================
# mksyms.cmake - 符号表生成模块
# ======================================================
# 顶层安全变量，保证无论 CONFIG_ALLSYMS 是否定义都有
set(MKSYMS_ALL_TARGETS "" CACHE INTERNAL "All INTERFACE targets from mksyms.cmake")

# ------------------------------------------------------------------
# 如果 CONFIG_ALLSYMS 开启，则生成符号表相关步骤
# ------------------------------------------------------------------
if(CONFIG_ALLSYMS)

    # ------------------------------------------------------------------
    # 1 定义路径
    # ------------------------------------------------------------------
    set(ALLSYMS_EMPTY ${CMAKE_BINARY_DIR}/allsyms_empty.c)
    set(ALLSYMS_REAL  ${CMAKE_BINARY_DIR}/allsyms.c)
    set(NOSYMS_ELF    ${PROJECT_NAME}_nosyms.elf)

    # ------------------------------------------------------------------
    # 2 生成空 allsyms.c
    # ------------------------------------------------------------------
    add_custom_command(
        OUTPUT ${ALLSYMS_EMPTY}
        COMMAND python3 ${TOOLS_PATH}/gen_allsyms.py ${ALLSYMS_EMPTY}
        COMMENT "Generate empty allsyms.c"
    )

    # ------------------------------------------------------------------
    # 3 编译 nosyms.elf
    # ------------------------------------------------------------------
    add_executable(${NOSYMS_ELF}
        ${ALLSYMS_EMPTY}
    )

    target_link_libraries(${NOSYMS_ELF}
        PRIVATE kernel_elf_iface
    )

    # ------------------------------------------------------------------
    # 4 从 nosyms.elf 生成真实 allsyms.c
    # ------------------------------------------------------------------
    add_custom_command(
        OUTPUT ${ALLSYMS_REAL}
        COMMAND python3 ${TOOLS_PATH}/gen_allsyms.py ${ALLSYMS_REAL} $<TARGET_FILE:${NOSYMS_ELF}>
        DEPENDS ${NOSYMS_ELF}
        COMMENT "Generate allsyms.c from nosyms ELF"
    )

    add_custom_target(allsyms_gen DEPENDS ${ALLSYMS_REAL})
    set_source_files_properties(${ALLSYMS_REAL} PROPERTIES GENERATED TRUE)

    # ------------------------------------------------------------------
    # 5 创建 allsyms INTERFACE target，导出给顶层使用
    # ------------------------------------------------------------------
    add_library(allsyms_iface INTERFACE)
    target_sources(allsyms_iface INTERFACE ${ALLSYMS_REAL})
    add_dependencies(allsyms_iface allsyms_gen)

    # 6 统一添加到顶层可用列表
    list(APPEND MKSYMS_ALL_TARGETS allsyms_iface)
    set(MKSYMS_ALL_TARGETS ${MKSYMS_ALL_TARGETS} CACHE INTERNAL "All INTERFACE targets from mksyms.cmake")

endif()

