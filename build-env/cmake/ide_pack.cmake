# Copyright (c) 2026 Kyland Inc.
# Intewell-RTOS is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

# Add SDK target to export headers
set(IDEPACK_ZIP_NAME "idepack_${CMAKE_PRESET_NAME}.zip")
set(IDEPACK_ZIP_PATH "${CMAKE_BINARY_DIR}/${IDEPACK_ZIP_NAME}")

add_custom_target(mainfast
    COMMAND ${CMAKE_C_COMPILER} 
            -E -P -x c
            -imacros ${AUTOCONFIG_H}
            -imacros ${CMAKE_SOURCE_DIR}/include/version.h 
            ${CMAKE_CURRENT_LIST_DIR}/ide/mainfast.json.template
            -o ${CMAKE_BINARY_DIR}/mainfast.json

    COMMAND awk 'NF{p=1} p' ${CMAKE_BINARY_DIR}/mainfast.json > ${CMAKE_BINARY_DIR}/mainfast.json.tmp
    COMMAND ${CMAKE_COMMAND} -E rename ${CMAKE_BINARY_DIR}/mainfast.json.tmp ${CMAKE_BINARY_DIR}/mainfast.json
)


add_custom_target(idepack

    COMMAND zip -j ${IDEPACK_ZIP_NAME}
            ${CMAKE_BINARY_DIR}/rtos.bin 
            ${CMAKE_BINARY_DIR}/rtos.elf
            ${CMAKE_BINARY_DIR}/rootfs.bin 
            ${CMAKE_BINARY_DIR}/mainfast.json

    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}

    DEPENDS rootfs ${PROJECT_NAME}.elf mainfast
    VERBATIM
)

os_out_add_target(mainfast)
os_out_add_file(${CMAKE_CURRENT_BINARY_DIR}/mainfast.json)
