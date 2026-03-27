# Copyright (c) 2026 Kyland Inc.
# Intewell-RTOS is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

# string(REGEX REPLACE "/$" "" CURRENT_FOLDER_ABSOLUTE ${CMAKE_CURRENT_SOURCE_DIR})

# file(GLOB SOURCE_FILE_LIST ${CURRENT_FOLDER_ABSOLUTE}/*.c ${CURRENT_FOLDER_ABSOLUTE}/*.S ${CURRENT_FOLDER_ABSOLUTE}/*.cpp ${CURRENT_FOLDER_ABSOLUTE}/*.cxx ${CURRENT_FOLDER_ABSOLUTE}/*.c++)

# if(NOT EXISTS "${CONFIG_INITRAMFS_SOURCE}")
#     message(FATAL_ERROR "------------ cpio initramfs source not found: ${CONFIG_INITRAMFS_SOURCE} ------------ ")
# endif()

# set(CPIO_DATA ${CMAKE_BINARY_DIR}/${CONFIG_INITRAMFS_SOURCE})
# set(CPIO_INC_DATA ${CMAKE_BINARY_DIR}/initramfs_inc_data)

# # 生成initramfs_inc_data 文件，在汇编阶段将其放入.init.ramfs段
# add_custom_target(copy_initramfs_inc_data
#     COMMAND ${CMAKE_COMMAND} -E copy ${CPIO_DATA} ${CPIO_INC_DATA}
# )

# set_property(TARGET copy_initramfs_inc_data APPEND PROPERTY ADDITIONAL_CLEAN_FILES ${CPIO_INC_DATA})
# add_dependencies(${PROJECT_NAME}  copy_initramfs_inc_data)

# foreach(file ${SOURCE_FILE_LIST})
#     list(APPEND GLOB_SOURCE_LIST ${file})
# endforeach()

# # 包含子文件夹
# SUBDIRLIST(SUBDIRS)

# foreach(SUB_FOLDER ${SUBDIRS})
#     ADD_SUBDIR(${SUB_FOLDER})
# endforeach()

# set(GLOB_INC_PATH_LIST ${GLOB_INC_PATH_LIST} PARENT_SCOPE)
# set(GLOB_SOURCE_LIST ${GLOB_SOURCE_LIST} PARENT_SCOPE)

# kernel/xxx/CMakeLists.txt

set(KMODULE_SRC_FILES "")

# 设置宏控制是否启用
kernel_add_subdir(. CONFIG_ROOTFS_CPIO)

file(GLOB KMODULE_SRC_FILES *.c *.S)

# 引入统一模板
os_kernel_submodule_scan()
