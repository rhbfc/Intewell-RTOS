/*
 * Copyright (c) 2026 Kyland Inc.
 * Intewell-RTOS is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */

#ifndef __INITRD_H__
#define __INITRD_H__

#include "system/types.h"

/* 内核启动参数中 initrd 的开始和结束地址（虚拟地址） */
extern virt_addr_t initrd_start, initrd_end;

/* initrd 的物理地址和大小 */
extern phys_addr_t phys_initrd_start;
extern unsigned long phys_initrd_size;

extern char __initramfs_start[];
extern unsigned long __initramfs_size;

/* newc 格式的cpio头 */
struct cpio_header {
    char magic[6];    // "070701" 或 "070702"
    char ino[8];
    char mode[8];     // 文件类型和权限（16进制）
    char uid[8];
    char gid[8];
    char nlink[8];
    char mtime[8];
    char filesize[8]; // 数据段大小（16进制）
    char devmajor[8];
    char devminor[8];
    char rdevmajor[8];
    char rdevminor[8];
    char namesize[8]; // 文件名长度（16进制）
    char check[8];
}__attribute__((packed));


#endif /* __INITRD_H__ */
