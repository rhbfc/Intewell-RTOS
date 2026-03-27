# Copyright (c) 2026 Kyland Inc.
# Intewell-RTOS is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

# cmake/GitVersion.cmake
# 用法：
#   generate_git_version(<repo_path> <out_header_path>)
#
# 示例：
#   generate_git_version(${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR}/git_version.h)

function(generate_git_version _key repo_path out_header_path)
    message(STATUS "[GIT] version regeneration request: repo='${repo_path}' out='${out_header_path}'")
    if(NOT IS_ABSOLUTE "${repo_path}")
        get_filename_component(repo_path "${repo_path}" ABSOLUTE)
    endif()

    if(NOT IS_ABSOLUTE "${out_header_path}")
        get_filename_component(out_header_path "${out_header_path}" ABSOLUTE)
    endif()

    set(_py "${CMAKE_SOURCE_DIR}/build-env/tools/gen_git_version.py")
    if(NOT EXISTS "${_py}")
        message(FATAL_ERROR "gen_git_version.py not found: ${_py}")
    endif()

    # 生成一个唯一的 target 名，避免重复定义
    set(_tgt "git_version_${_key}")

    set(_tmp "${out_header_path}.tmp")

    if(NOT TARGET ${_tgt})
        add_custom_command(
            OUTPUT "${out_header_path}"
            COMMAND ${CMAKE_COMMAND} -E env
                python3 "${_py}"
                    --repo "${repo_path}"
                    --out  "${_tmp}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_tmp}" "${out_header_path}"
            COMMAND ${CMAKE_COMMAND} -E rm -f "${_tmp}"
            WORKING_DIRECTORY "${repo_path}"
            DEPENDS "${_py}"
            # COMMENT "[git] Generating ${out_header_path}"
            VERBATIM
        )

        add_custom_target(${_tgt} DEPENDS "${out_header_path}")
    endif()

    set(GIT_VERSION_TARGET ${_tgt} PARENT_SCOPE)
    set(GIT_VERSION_HEADER ${out_header_path} PARENT_SCOPE)
endfunction()
