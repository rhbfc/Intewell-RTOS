# Copyright (c) 2026 Kyland Inc.
# Intewell-RTOS is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

function(os_generate_asm_offsets TARGET SRC PATH INCLUDES DEFINES)
    # ------------------------------------------------------------
    # 1. 路径与文件名处理
    # ------------------------------------------------------------
    get_filename_component(SRC_NAME ${SRC} NAME)
    set(GEN_C ${PATH}/${SRC_NAME}.c)
    set(GEN_S ${PATH}/${SRC_NAME}.s)
    set(GEN_H ${PATH}/${SRC_NAME}.h)

    # 确保输出目录存在
    file(MAKE_DIRECTORY ${PATH})

    # ------------------------------------------------------------
    # 2. include / define 参数
    # ------------------------------------------------------------
    set(INC_FLAGS "")
    foreach(inc ${INCLUDES})
        list(APPEND INC_FLAGS -I${inc})
    endforeach()

    set(DEF_FLAGS "")
    foreach(def ${DEFINES})
        list(APPEND DEF_FLAGS -D${def})
    endforeach()

    # ------------------------------------------------------------
    # 3. 生成 asm-offsets.h
    # ------------------------------------------------------------
    add_custom_command(
        OUTPUT ${GEN_H}

        # (1) 复制源文件并加 .c
        COMMAND ${CMAKE_COMMAND}
            -E copy_if_different
            ${SRC}
            ${GEN_C}

        # (2) 编译成汇编
        COMMAND ${CMAKE_C_COMPILER}
            -S
            -ffreestanding
            ${DEF_FLAGS}
            ${INC_FLAGS}
            ${GEN_C}
            -o ${GEN_S}

        # # (3) 提取
        COMMAND ${TOOLS_PATH}/asm_offset.sh ${GEN_S} ${GEN_H}

        DEPENDS ${SRC}
        COMMENT "[ASM-OFFSETS] generate ${GEN_H}  ${CMAKE_C_COMPILER}"
        VERBATIM
        )
        
    # ------------------------------------------------------------
    # 4. custom target
    # ------------------------------------------------------------
    add_custom_target(${TARGET} DEPENDS ${GEN_H})
endfunction()
