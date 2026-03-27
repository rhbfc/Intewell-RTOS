# Copyright (c) 2026 Kyland Inc.
# Intewell-RTOS is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

# 2.2 Misc
#
# import_kconfig(<prefix> <kconfig_fragment> [<keys>])
#
# Parse a KConfig fragment (typically with extension .config) and
# introduce all the symbols that are prefixed with 'prefix' into the
# CMake namespace. List all created variable names in the 'keys'
# output variable if present.
function(import_kconfig prefix kconfig_fragment)
  # Parse the lines prefixed with 'prefix' in ${kconfig_fragment}
  file(
    STRINGS
    ${kconfig_fragment}
    DOT_CONFIG_LIST
    REGEX "^${prefix}"
    ENCODING "UTF-8"
  )

  foreach (CONFIG ${DOT_CONFIG_LIST})

    # CONFIG could look like: CONFIG_NET_BUF=y

    # Match the first part, the variable name
    string(REGEX MATCH "[^=]+" CONF_VARIABLE_NAME ${CONFIG})

    # Match the second part, variable value
    string(REGEX MATCH "=(.+$)" CONF_VARIABLE_VALUE ${CONFIG})
    # The variable name match we just did included the '=' symbol. To just get the
    # part on the RHS we use match group 1
    set(CONF_VARIABLE_VALUE ${CMAKE_MATCH_1})

    if("${CONF_VARIABLE_VALUE}" MATCHES "^\"(.*)\"$") # Is surrounded by quotes
      set(CONF_VARIABLE_VALUE ${CMAKE_MATCH_1})
    endif()

    set("${CONF_VARIABLE_NAME}" "${CONF_VARIABLE_VALUE}" PARENT_SCOPE)
    list(APPEND keys "${CONF_VARIABLE_NAME}")
  endforeach()

  foreach(outvar ${ARGN})
    set(${outvar} "${keys}" PARENT_SCOPE)
  endforeach()
endfunction()


# 2.2 Misc
#
# SUBDIRLIST(<var>)
#
macro(SUBDIRLIST result)
    string(REGEX REPLACE "/$" "" CURRENT_FOLDER_ABSOLUTE ${CMAKE_CURRENT_SOURCE_DIR})
    file(GLOB children RELATIVE ${CURRENT_FOLDER_ABSOLUTE} ${CURRENT_FOLDER_ABSOLUTE}/*)
    set(dirlist "")
    foreach(child ${children})
        if(IS_DIRECTORY ${CURRENT_FOLDER_ABSOLUTE}/${child})
            LIST(APPEND dirlist ${child})
        endif()
    endforeach()
    set(${result} ${dirlist})
endmacro()

# 2.2 Misc
#
# ADD_SUBDIR(<subdir>)
#
macro(REMOVE_SRC list filename)
    foreach(file ${${list}})
      if( "${file}" MATCHES "${filename}$")
          list(REMOVE_ITEM ${list} ${file})
      endif()
    endforeach()
endmacro()

macro(REMOVE_SRC_IF config list filename)
if(${config})
    foreach(file ${${list}})
      if( "${file}" MATCHES "${filename}$")
          list(REMOVE_ITEM ${list} ${file})
      endif()
    endforeach()
endif()
endmacro()

macro(REMOVE_SRC_IFNOT config list filename)
if(NOT ${config})
    foreach(file ${${list}})
      if( "${file}" MATCHES "${filename}$")
          list(REMOVE_ITEM ${list} ${file})
      endif()
    endforeach()
endif()
endmacro()

macro(ADD_SRC_IF config list filename)
if(${config})
    list(APPEND ${list} ${CMAKE_CURRENT_SOURCE_DIR}/${filename})
endif()
endmacro()

macro(ADD_SRC list filename)
file(GLOB __SOURCE_FILE_LIST__ ${CMAKE_CURRENT_SOURCE_DIR}/${filename})

foreach(file ${__SOURCE_FILE_LIST__})
    list(APPEND ${list} ${file})
endforeach()
endmacro()

macro(ADD_SUBDIR subdir)
    string(REGEX REPLACE "/$" "" CURRENT_FOLDER_ABSOLUTE ${CMAKE_CURRENT_SOURCE_DIR})
    if(EXISTS ${CURRENT_FOLDER_ABSOLUTE}/${subdir}/CMakeLists.txt)
        add_subdirectory(${subdir})
    endif()
endmacro()

macro(ADD_SUBDIR_IF config subdir)
    if(${config})
      string(REGEX REPLACE "/$" "" CURRENT_FOLDER_ABSOLUTE ${CMAKE_CURRENT_SOURCE_DIR})
      if(EXISTS ${CURRENT_FOLDER_ABSOLUTE}/${subdir}/CMakeLists.txt)
          add_subdirectory(${subdir})
      endif()
    endif()
endmacro()

macro(ADD_SUBDIR_IFNOT config subdir)
    if(NOT ${config})
      string(REGEX REPLACE "/$" "" CURRENT_FOLDER_ABSOLUTE ${CMAKE_CURRENT_SOURCE_DIR})
      if(EXISTS ${CURRENT_FOLDER_ABSOLUTE}/${subdir}/CMakeLists.txt)
          add_subdirectory(${subdir})
      endif()
    endif()
endmacro()

