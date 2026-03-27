# Copyright (c) 2026 Kyland Inc.
# Intewell-RTOS is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

# driver/xxx/yyy/CMakeLists.txt

set(KMODULE_SRC_FILES "")

# 设置宏控制是否启用
driver_add_subdir(. CONFIG_ARCH_ARM)

file(GLOB KMODULE_SRC_FILES cpu_arm.c)

ADD_SRC_IF(CONFIG_ARCH_ARMv7    KMODULE_SRC_FILES cpu_armv7.c)
if(CONFIG_ARCH_AARCH32)
    ADD_SRC_IF(TRUE KMODULE_SRC_FILES cpu_armv8.c)
    ADD_SRC_IF(TRUE KMODULE_SRC_FILES cpu_armv8_a55.c)
elseif(CONFIG_ARCH_AARCH64)
    ADD_SRC_IF(TRUE KMODULE_SRC_FILES cpu_armv8.c)
    ADD_SRC_IF(TRUE KMODULE_SRC_FILES cpu_armv8_a55.c)
endif()

# 引入统一模板
os_driver_submodule_scan()
