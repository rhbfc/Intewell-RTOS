# Copyright (c) 2026 Kyland Inc.
# Intewell-RTOS is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

# driver CMakeLists.txt

set(KMODULE_SRC_FILES "")

# 默认不加
set(_enable_mod ON)

# # 兼容 CONFIG_X86_64=1 / y / ON / TRUE
# if(DEFINED CONFIG_X86_64)
#   set(_enable_mod OFF)
# endif()

# 设置宏控制是否启用
driver_add_subdir(. _enable_mod)

file(GLOB KMODULE_SRC_FILES *.c *.S)

# 引入统一模板
os_driver_submodule_scan()
