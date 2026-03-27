# Copyright (c) 2026 Kyland Inc.
# Intewell-RTOS is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

set(KMODULE_SRC_FILES "")

# 设置宏控制是否启用
component_add_subdir(. CONFIG_ACPI)

file(GLOB KMODULE_SRC_FILES *.c *.S)

file(GLOB RESOURCE_FILES acpica/source/components/utilities/*.c)
list(FILTER RESOURCE_FILES EXCLUDE REGEX "utprint\\.c$")
list(APPEND KMODULE_SRC_FILES ${RESOURCE_FILES})


# Tables - needed to load and manage ACPI tables
ADD_SRC(KMODULE_SRC_FILES acpica/source/components/tables/*.c)

# Namespace - needed for ACPI namespace management
ADD_SRC(KMODULE_SRC_FILES acpica/source/components/namespace/*.c)

# Parser - needed to parse AML bytecode
ADD_SRC(KMODULE_SRC_FILES acpica/source/components/parser/*.c)

# Dispatcher - needed to dispatch AML opcodes
ADD_SRC(KMODULE_SRC_FILES acpica/source/components/dispatcher/*.c)

# Executer - needed to execute AML code
ADD_SRC(KMODULE_SRC_FILES acpica/source/components/executer/*.c)

# Hardware - needed for hardware access
ADD_SRC(KMODULE_SRC_FILES acpica/source/components/hardware/*.c)

# Events - needed for event handling
ADD_SRC(KMODULE_SRC_FILES acpica/source/components/events/*.c)

ADD_SRC(KMODULE_SRC_FILES acpica/source/common/ahids.c)

# Resources - needed for resource management
# Exclude rsdump.c as it's only for debugger
file(GLOB RESOURCE_FILES acpica/source/components/resources/*.c)
list(FILTER RESOURCE_FILES EXCLUDE REGEX "rsdump\\.c$")
list(APPEND KMODULE_SRC_FILES ${RESOURCE_FILES})


set(COMPONENT_INTERFACE acpi_interface)
add_library(${COMPONENT_INTERFACE} INTERFACE)

target_include_directories(${COMPONENT_INTERFACE} INTERFACE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/acpica/source/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/platform
)

target_link_libraries(os_common_interface INTERFACE ${COMPONENT_INTERFACE})

# 注册 interface （让 OS 层也能拿到公共宏与 flags）
os_register_component(${COMPONENT_INTERFACE})

# 引入统一模板
os_component_submodule_scan()

install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/acpica/source/include/
    DESTINATION acpica
    COMPONENT sdk
    FILES_MATCHING
    PATTERN "*.h"
)

install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/platform/
    DESTINATION acpica/platform
    COMPONENT sdk
)
