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
# component.cmake - STATIC-only component framework
# ============================================================

# ------------------------------------------------------------
# 打印模块信息（保留）
# ------------------------------------------------------------
macro(print_module_info)
    get_filename_component(FOLDER_NAME ${CMAKE_CURRENT_LIST_DIR} NAME)
    message(STATUS "    [component-${FOLDER_NAME}] module: ${CMAKE_CURRENT_SOURCE_DIR}")
endmacro()

# ------------------------------------------------------------
# 根据目录生成 component 子模块名字
# component/xxx/yyy    -> component_xxx_yyy
# ------------------------------------------------------------
function(get_component_submodule_name OUTPUT_VAR)
    set(COMPONENT_BASE_DIR ${CMAKE_SOURCE_DIR}/components)
    file(RELATIVE_PATH REL_PATH ${COMPONENT_BASE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})

    if("${REL_PATH}" STREQUAL "" OR "${REL_PATH}" STREQUAL ".")
        set(NAME_PART "component")
    else()
        string(REPLACE "/" "_" NAME_PART ${REL_PATH})
    endif()

    set(${OUTPUT_VAR} component_${NAME_PART} PARENT_SCOPE)
endfunction()

# ------------------------------------------------------------
# component 子模块注册（注册的是 STATIC target）
# ------------------------------------------------------------
function(component_register_module module_name)
    list(APPEND COMPONENT_MODULES ${module_name})
    list(REMOVE_DUPLICATES COMPONENT_MODULES)
    set(COMPONENT_MODULES "${COMPONENT_MODULES}" CACHE INTERNAL "Component module list")
endfunction()

# ------------------------------------------------------------
# component_add_subdir - 控制子模块是否构建（保留你原来的语义）
# ------------------------------------------------------------
function(component_add_subdir SUBDIR CONFIG_OR_VALUE)
    set(SUBMODULE_ENABLED OFF)
    set(VALUE "")

    string(TOUPPER "${CONFIG_OR_VALUE}" TMP_VALUE)
    if(TMP_VALUE STREQUAL "Y" OR TMP_VALUE STREQUAL "ON")
        set(SUBMODULE_ENABLED ON)
        set(VALUE ${CONFIG_OR_VALUE})
    else()
        if(DEFINED ${CONFIG_OR_VALUE})
            set(VALUE ${${CONFIG_OR_VALUE}})
            string(TOUPPER "${VALUE}" TMP_VALUE2)
            if(TMP_VALUE2 STREQUAL "Y" OR TMP_VALUE2 STREQUAL "ON")
                set(SUBMODULE_ENABLED ON)
            endif()
        endif()
    endif()

    set(COMPONENT_SUBMODULE_ENABLED ${SUBMODULE_ENABLED} PARENT_SCOPE)

    get_filename_component(FOLDER_NAME ${CMAKE_CURRENT_LIST_DIR} NAME)
    if(SUBMODULE_ENABLED)
        message(STATUS "[component] Build module (${FOLDER_NAME}) ==> (CONFIG: ${CONFIG_OR_VALUE}=${VALUE})")
    else()
        message(STATUS "[component] Skip  module (${FOLDER_NAME}) ==> (CONFIG: ${CONFIG_OR_VALUE}=${VALUE})")
    endif()
endfunction()

# ============================================================
# os_component_submodule_scan
# 每个 component 子目录统一调用的模板
# ============================================================
macro(os_component_submodule_scan)

    # --------------------------------------------------------
    # 1. 模块名
    # --------------------------------------------------------
    get_component_submodule_name(LIB_NAME)

    # --------------------------------------------------------
    # 2. 是否启用
    # --------------------------------------------------------
    if(NOT DEFINED COMPONENT_SUBMODULE_ENABLED)
        set(COMPONENT_SUBMODULE_ENABLED ON)
    endif()

    if(NOT COMPONENT_SUBMODULE_ENABLED)
        message(STATUS "[component] Skip module: ${LIB_NAME}")
        return()
    endif()

    message(STATUS "[component] Build module: ${LIB_NAME}")

    # --------------------------------------------------------
    # 3. 源文件（由子模块自己提前 set）
    #   子模块 CMakeLists.txt 中：
    #     set(KMODULE_SRC_FILES a.c b.c)
    # --------------------------------------------------------
    set(_KMODULE_HAS_SRC FALSE)

    if(KMODULE_SRC_FILES)
        foreach(src ${KMODULE_SRC_FILES})
            if(EXISTS ${src})
                set(_KMODULE_HAS_SRC TRUE)
                break()
            endif()
        endforeach()
    endif()

    if(NOT _KMODULE_HAS_SRC)
        message(STATUS "==========[component] ${LIB_NAME} has no source files")
    endif()

    # --------------------------------------------------------
    # 4. STATIC 子模块库（关键变化点）
    # --------------------------------------------------------
    add_library(${LIB_NAME} STATIC ${KMODULE_SRC_FILES} ${CMAKE_ENV_PATH}/empty.c)

    # --------------------------------------------------------
    # 5. 私有编译选项
    # --------------------------------------------------------
    if(DEFINED KMODULE_COMPILE_OPTIONS)
        target_compile_options(${LIB_NAME} PRIVATE ${KMODULE_COMPILE_OPTIONS})
    endif()

    # --------------------------------------------------------
    # 6. 私有宏
    # --------------------------------------------------------
    if(DEFINED KMODULE_COMPILE_DEFINITIONS)
        target_compile_definitions(${LIB_NAME} PRIVATE ${KMODULE_COMPILE_DEFINITIONS})
    endif()

    # --------------------------------------------------------
    # 7. 私有头文件（include 目录）
    # --------------------------------------------------------
    if(DEFINED KMODULE_INCLUDE_DIRS)
        target_include_directories(${LIB_NAME} PRIVATE ${KMODULE_INCLUDE_DIRS})
    endif()

    # --------------------------------------------------------
    # 8. 公共 OS 配置
    # --------------------------------------------------------
    target_link_libraries(${LIB_NAME} PUBLIC os_common_interface)

    if(DEFINED OS_GIT_VERSION_TARGET)
        add_dependencies(${LIB_NAME} ${OS_GIT_VERSION_TARGET})
    endif()

    # --------------------------------------------------------
    # 9. 注册到 component 聚合库
    # --------------------------------------------------------
    component_register_module(${LIB_NAME})

    # --------------------------------------------------------
    # 10. 递归子目录
    # --------------------------------------------------------
    file(GLOB SUBDIRS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/*)
    foreach(sub ${SUBDIRS})
        if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${sub}"
           AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${sub}/CMakeLists.txt")
            add_subdirectory(${sub})
        endif()
    endforeach()

endmacro()

# ============================================================
# component 聚合库（STATIC）
# 由 component/CMakeLists.txt 在最后调用
# ============================================================
function(component_create_aggregate)
    add_library(component STATIC ${CMAKE_ENV_PATH}/empty.c)

    if(COMPONENT_MODULES)
        message(STATUS "[component] Aggregate modules: ${COMPONENT_MODULES}")
        target_link_libraries(component PRIVATE os_common_interface ${COMPONENT_MODULES})
    else()
        message(WARNING "[component] No component modules registered")
    endif()

    os_register_component(component)

    message(STATUS "[component] modules = ${COMPONENT_MODULES}")

endfunction()
