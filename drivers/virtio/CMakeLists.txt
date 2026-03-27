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

ADD_SRC_IF(CONFIG_DRIVERS_VIRTIO    KMODULE_SRC_FILES virtio.c)
ADD_SRC_IF(CONFIG_DRIVERS_VIRTIO    KMODULE_SRC_FILES virtio_bus.c)
ADD_SRC_IF(CONFIG_VIRTIO_MMIO       KMODULE_SRC_FILES virtio-mmio.c)
ADD_SRC_IF(CONFIG_VIRTIO_MMIO       KMODULE_SRC_FILES virtqueue.c)
ADD_SRC_IF(CONFIG_VIRTIO_MMIO_BLK   KMODULE_SRC_FILES virtio-blk.c)
ADD_SRC_IF(CONFIG_VIRTIO_MMIO_NET   KMODULE_SRC_FILES virtio-net.c)

if(CONFIG_PCI)
ADD_SRC_IF(CONFIG_VIRTIO_PCI        KMODULE_SRC_FILES virtio-pci.c)
ADD_SRC_IF(CONFIG_VIRTIO_PCI        KMODULE_SRC_FILES virtio-pci-modern.c)
ADD_SRC_IF(CONFIG_VIRTIO_PCI        KMODULE_SRC_FILES virtio-pci-legacy.c)
endif(CONFIG_PCI)

# 引入统一模板
os_driver_submodule_scan()
