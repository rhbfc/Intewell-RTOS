#!/bin/bash
# Copyright (c) 2026 Kyland Inc.
# Intewell-RTOS is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.

set -x

# Script usage function
usage() {
    echo "Usage: $0 <rtos.bin path> <output ISO filename> <rootfs.bin path>"
    echo "Example: $0 ./rtos.bin intewell.iso ./rootfs/x86_64/rootfs.bin"
    exit 1
}

# Check number of arguments
if [ $# -ne 2 ]; then
    echo "Error: Three arguments required"
    usage
fi

RTOS_BIN="$1"
OUTPUT_ISO="$2"
# ROOTFS_BIN="$3"

# Check if rtos.bin file exists
if [ ! -f "$RTOS_BIN" ]; then
    echo "Error: RTOS binary file '$RTOS_BIN' does not exist"
    exit 1
fi

# Check if rootfs.bin file exists
# if [ ! -f "$ROOTFS_BIN" ]; then
#     echo "Error: Rootfs binary file '$ROOTFS_BIN' does not exist"
#     exit 1
# fi

echo "Using RTOS binary file: $RTOS_BIN"
# echo "Using Rootfs binary file: $ROOTFS_BIN"
echo "Output ISO file: $OUTPUT_ISO"

# Clean and create temporary directory
rm -fr /tmp/iso
mkdir -p /tmp/iso/boot/grub

# Copy RTOS binary file
cp "$RTOS_BIN" /tmp/iso/boot/kernel.bin

# Copy Rootfs binary file
# cp "$ROOTFS_BIN" /tmp/iso/boot/rootfs.bin

# Detect if kernel supports Multiboot2 by checking for Multiboot2 header
# We'll create both Multiboot and Multiboot2 menu entries for compatibility
# module2 /boot/rootfs.bin rootfs

if hexdump -Cv "$RTOS_BIN" | head -c $((32*1024*4)) | grep -q "d6 50 52 e8"; then
    echo "Multiboot2 header found, creating Multiboot2 menu entry"
cat <<'EOF' | tee /tmp/iso/boot/grub/grub.cfg > /dev/null
insmod efi_gop
insmod gfxterm

set timeout=5
set default=0
set gfxpayload=keep
loadfont /boot/grub/fonts/unicode.pf2
terminal_output gfxterm

menuentry "Intewell RTOS" {
    multiboot2 /boot/kernel.bin
    boot
}
EOF
else
# module /boot/rootfs.bin rootfs
    echo "Multiboot2 header not found, creating Multiboot menu entry only"
cat <<'EOF' | tee /tmp/iso/boot/grub/grub.cfg > /dev/null
set timeout=5
set default=0

menuentry "Intewell RTOS" {
    multiboot /boot/kernel.bin
    boot
}
EOF
fi

mkdir -p /tmp/iso/EFI/BOOT
# 生成 EFI 可执行文件
grub-mkstandalone \
    -O x86_64-efi \
    -o /tmp/iso/EFI/BOOT/BOOTx64.EFI \
    "boot/grub/grub.cfg=/tmp/iso/boot/grub/grub.cfg"

mkdir -p /tmp/iso/boot/grub/fonts

cp /usr/share/grub/unicode.pf2 /tmp/iso/boot/grub/fonts/unicode.pf2

# Create ISO file
grub-mkrescue  -o "$OUTPUT_ISO" /tmp/iso --efi-boot /EFI/BOOT/BOOTx64.EFI #--modules="multiboot2 gfxterm efi_gop all_video part_gpt part_msdos fat iso9660 ext2 normal chain boot linux configfile"

# Check if ISO creation was successful
if [ $? -eq 0 ]; then
    echo "ISO file created successfully: $OUTPUT_ISO"
else
    echo "Error: ISO file creation failed"
    exit 1
fi

# Clean up temporary files
rm -fr /tmp/iso