# Copyright (c) 2026 Kyland Inc.
# Intewell-RTOS is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

function(os_setup_sdk_targets)
    if(TARGET sdk)
        return()
    endif()

    set(SDK_OUTPUT_DIR      ${CMAKE_BINARY_DIR}/sdk)
    set(SDK_INC_PATH        ${SDK_OUTPUT_DIR}/include)
    set(GLOBA_DESC_TEMPLATE ${CMAKE_SOURCE_DIR}/build-env/desc_template/desc_template)

    install(
        DIRECTORY ${CMAKE_SOURCE_DIR}/include/
        DESTINATION kernel
        COMPONENT sdk
        FILES_MATCHING
        PATTERN "*.h"
        PATTERN "linux-comp" EXCLUDE
    )

    install(
        DIRECTORY ${CMAKE_SOURCE_DIR}/include/linux-comp/
        DESTINATION linux-comp
        COMPONENT sdk
        FILES_MATCHING
        PATTERN "*.h"
    )

    add_custom_target(generate_desc
        COMMAND ${CMAKE_COMMAND} -E remove_directory ${SDK_OUTPUT_DIR}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SDK_OUTPUT_DIR}
        COMMAND ${CMAKE_C_COMPILER}
                -E -P -x c
                -imacros ${AUTOCONFIG_H}
                -imacros ${CMAKE_BINARY_DIR}/git_version.h
                -imacros ${CMAKE_SOURCE_DIR}/include/version.h
                "-DBOARD_NAME=\\\"${CMAKE_PRESET_NAME}\\\""
                ${GLOBA_DESC_TEMPLATE} -o ${SDK_OUTPUT_DIR}/.desc
        COMMAND awk 'NF{p=1} p' ${SDK_OUTPUT_DIR}/.desc > ${SDK_OUTPUT_DIR}/.desc.tmp
        COMMAND ${CMAKE_COMMAND} -E rename ${SDK_OUTPUT_DIR}/.desc.tmp ${SDK_OUTPUT_DIR}/.desc
        BYPRODUCTS ${SDK_OUTPUT_DIR}/.desc
        DEPENDS ${OS_GIT_VERSION_TARGET}
    )

    add_custom_target(sdk
        COMMAND ${CMAKE_COMMAND} --install ${CMAKE_BINARY_DIR} --prefix ${SDK_INC_PATH}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SDK_OUTPUT_DIR}/cmake
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SDK_OUTPUT_DIR}/include/kernel
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_ENV_PATH}/load_dotconfig.cmake        ${SDK_OUTPUT_DIR}/cmake/
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_ENV_PATH}/tools.cmake                 ${SDK_OUTPUT_DIR}/cmake/
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/.config                   ${SDK_OUTPUT_DIR}/
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/include/ttosInterTask.inl ${SDK_OUTPUT_DIR}/include/kernel/
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/include/ttosUtils.inl     ${SDK_OUTPUT_DIR}/include/kernel/
        DEPENDS generate_desc
        COMMENT "Exporting SDK headers and build helpers to ${SDK_OUTPUT_DIR}"
        VERBATIM
    )
endfunction()
