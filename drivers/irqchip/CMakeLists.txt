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

# 设置宏控制是否启用
driver_add_subdir(. ON)

ADD_SRC_IF(CONFIG_GIC_VERSION_V3    KMODULE_SRC_FILES gicv3.c)
ADD_SRC_IF(CONFIG_GIC_VERSION_V3    KMODULE_SRC_FILES gicv3_its.c)
ADD_SRC_IF(CONFIG_GIC_VERSION_V2    KMODULE_SRC_FILES gicv2.c)

# 引入统一模板
os_driver_submodule_scan()
