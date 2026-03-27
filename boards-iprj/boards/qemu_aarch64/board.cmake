# Copyright (c) 2026 Kyland Inc.
# Intewell-RTOS is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

set(QEMU_BIN qemu-system-aarch64)

if(NOT DEFINED KERNEL_BIN)
    set(KERNEL_BIN "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.bin")
endif()

set(QEMU_ARGS -cpu cortex-a57
                -m 3G -smp cpus=2,cores=2,threads=1 
                -machine virt,gic-version=3 
                -nographic 
                -serial mon:stdio -global virtio-mmio.force-legacy=false 
                -netdev tap,id=tapnet,ifname=tap0,script=no,downscript=no
                -device virtio-net-device,netdev=tapnet,mq=on,packed=on,mac=52:54:10:11:12:14)


if(DEFINED CONFIG_ROOTFS_EXT4)
    list(APPEND QEMU_ARGS
                -drive file=${ROOTFS_BIN},if=none,format=raw,id=hd 
                -device virtio-blk-device,drive=hd,bus=virtio-mmio-bus.0)
endif()

if(CONFIG_VIRTIO_CONSOLE)
    list(APPEND QEMU_ARGS
                -device virtio-serial-device
                -chardev socket,id=vcsock,path=/tmp/serial1.sock,server=on,wait=off
                -device virtconsole,chardev=vcsock)
endif()

if(${CMAKE_HOST_SYSTEM_NAME} STREQUAL "Windows")
    set(SUDO "")
else()
    set(SUDO "sudo")
endif()


set(RUN_CMD ${SUDO} ${QEMU_BIN} ${QEMU_ARGS} -kernel ${KERNEL_BIN})

add_custom_target(run
    COMMAND ${RUN_CMD}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    USES_TERMINAL
)

add_custom_target(debug
    COMMAND ${RUN_CMD} -S -gdb tcp::${CONFIG_DEBUG_PORT}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    USES_TERMINAL
)

# Private board artifact: include idepack zip only for tag pipelines.
os_out_add_tag_artifacts(
    TARGETS idepack
    FILES ${IDEPACK_ZIP_PATH}
)
