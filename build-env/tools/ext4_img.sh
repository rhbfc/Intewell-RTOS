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

dtb_file=$2
map_file=$3
conf_file=$4
kernel_file=$5
output_file=$6

echo "dtb_file     = ${dtb_file}"
echo "map_file     = ${map_file}"
echo "conf_file    = ${conf_file}"
echo "kernel_file  = ${kernel_file}"
echo "output_file  = ${output_file}"

img_size=$1

if [ "$#" -ne 6 ]; then
    echo "使用方法: $0 dtb_file map_file conf_file kernel_file output_file"
    exit 1
fi

rm -fr $output_file

dd if=/dev/zero of=$output_file bs=1M count=$img_size

mkdir -p tmp_disk_mnt/extlinux

echo "label kernel.dtb intewell" > tmp_disk_mnt/extlinux/extlinux.conf
echo "	kernel /intewell" >> tmp_disk_mnt/extlinux/extlinux.conf
echo "	fdt /rk-kernel.dtb" >> tmp_disk_mnt/extlinux/extlinux.conf
#echo "	initrd /initrd" >> tmp_disk_mnt/extlinux/extlinux.conf

cp $dtb_file tmp_disk_mnt/rk-kernel.dtb
cp $kernel_file tmp_disk_mnt/intewell
cp $conf_file tmp_disk_mnt/config
cp $map_file tmp_disk_mnt/System.map
mkfs.ext4 -d tmp_disk_mnt $output_file

rm -fr tmp_disk_mnt

