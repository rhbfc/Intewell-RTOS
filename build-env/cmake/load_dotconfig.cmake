# Copyright (c) 2026 Kyland Inc.
# Intewell-RTOS is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

# Support assigning Kconfig symbols on the command-line with CMake
# cache variables prefixed with 'CONFIG_'. This feature is
# experimental and undocumented until it has undergone more
# user-testing.
unset(EXTRA_KCONFIG_OPTIONS)
get_cmake_property(cache_variable_names CACHE_VARIABLES)
foreach (name ${cache_variable_names})
    # message(${cache_variable_names})
  if("${name}" MATCHES "^CONFIG_")
    # message(${name})
    # When a cache variable starts with 'CONFIG_', it is assumed to be
    # a Kconfig symbol assignment from the CMake command line.
    set(EXTRA_KCONFIG_OPTIONS
      "${EXTRA_KCONFIG_OPTIONS}\n${name}=${${name}}"
      )
  endif()
endforeach()

if(NOT EXISTS ${CONFIG_DOT})
    message(STATUS "[Kconfig] .config not found, generating from ${CONFIG_FILE}")

    execute_process(
        COMMAND ${CMAKE_COMMAND} -E env
            SRCTREE=${CMAKE_SOURCE_DIR}
            EXTRA_KCONFIG_OPTIONS=${EXTRA_KCONFIG_OPTIONS}
            BOARD_PATH=${CONFIG_BOARD_PATH}
            python3 ${TOOLS_PATH}/run_kconfig.py
                ${CMAKE_SOURCE_DIR}/Kconfig
                ${CMAKE_SOURCE_DIR}/${CONFIG_FILE}
                ${AUTOCONFIG_H}
                ${CMAKE_BINARY_DIR}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )

endif()

# Remove the CLI Kconfig symbols from the namespace and
# CMakeCache.txt. If the symbols end up in CONFIG_DOT they will be
# re-introduced to the namespace through 'import_kconfig'.
foreach (name ${cache_variable_names})
  if("${name}" MATCHES "^CONFIG_")
    unset(${name})
    unset(${name} CACHE)
  endif()
endforeach()

# Parse the lines prefixed with CONFIG_ in the .config file from Kconfig
import_kconfig(CONFIG_ ${CONFIG_DOT})

# Re-introduce the CLI Kconfig symbols that survived
foreach (name ${cache_variable_names})
  if("${name}" MATCHES "^CONFIG_")
    if(DEFINED ${name})
      set(${name} ${${name}} CACHE STRING "")
      message(STATUS "[Kconfig]  ${name} = ${${name}}")
    endif()
  endif()
endforeach()


# ---- Import CONFIG_* from autoconfig_yaml.h into CMake cache ----
# 你需要先有一个变量指向这个头文件
# 比如：set(AUTOCONFIG_YAML_H "${CMAKE_BINARY_DIR}/autoconfig_yaml.h")

set(AUTOCONFIG_YAML_H "${CMAKE_BINARY_DIR}/autoconfig_yaml.h")
if(DEFINED AUTOCONFIG_YAML_H AND EXISTS "${AUTOCONFIG_YAML_H}")
  file(READ "${AUTOCONFIG_YAML_H}" _yaml_ext_h)

  # 匹配：#define CONFIG_XXX <value>
  # <value> 可能是 1 / 123 / "abc"
  string(REGEX MATCHALL "[\r\n]#define[ \t]+(CONFIG_[A-Za-z0-9_]+)[ \t]+([^ \t\r\n][^\r\n]*)" _def_lines "\n${_yaml_ext_h}")

  foreach(_m ${_def_lines})
    # 拆出 name 和 value
    string(REGEX REPLACE "^[\r\n]#define[ \t]+(CONFIG_[A-Za-z0-9_]+)[ \t]+([^ \t\r\n][^\r\n]*).*$" "\\1" _name "${_m}")
    string(REGEX REPLACE "^[\r\n]#define[ \t]+(CONFIG_[A-Za-z0-9_]+)[ \t]+([^ \t\r\n][^\r\n]*).*$" "\\2" _val  "${_m}")

    # 去掉行尾注释（可选，防止 '#define X 1 // comment'）
    string(REGEX REPLACE "[ \t]+//.*$" "" _val "${_val}")
    string(REGEX REPLACE "[ \t]+/\\*.*\\*/[ \t]*$" "" _val "${_val}")
    string(STRIP "${_val}" _val)

    # 写回 Cache（用 STRING，和你现有逻辑一致）
    # 去掉外层双引号，避免变成 '"aarch64"'
    if(_val MATCHES "^\"(.*)\"$")
      set(_val "${CMAKE_MATCH_1}")
    endif()

    set(${_name} "${_val}" CACHE STRING "" FORCE)
    # message(STATUS "[YAML-EXT] ${_name} = ${_val}")
  endforeach()

  # 处理：/* #undef CONFIG_XXX */
  # 你可以选择：遇到 undef 就从 cache 清掉，避免误用
  string(REGEX MATCHALL "/\\*[ \t]*#undef[ \t]+(CONFIG_[A-Za-z0-9_]+)[ \t]*\\*/" _undef_lines "${_yaml_ext_h}")
  foreach(_u ${_undef_lines})
    string(REGEX REPLACE "^/\\*[ \t]*#undef[ \t]+(CONFIG_[A-Za-z0-9_]+)[ \t]*\\*/$" "\\1" _uname "${_u}")
    if(DEFINED ${_uname})
      unset(${_uname} CACHE)
      # message(STATUS "[YAML-EXT] ${_uname} undef -> unset cache")
    endif()
  endforeach()

else()
  message(STATUS "[YAML-EXT] autoconfig_yaml.h not found: ${AUTOCONFIG_YAML_H}")
endif()
