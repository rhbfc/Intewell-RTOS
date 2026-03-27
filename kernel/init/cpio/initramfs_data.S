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

/* 将initramfs_inc_data添加到内核的.init.ramfs段 */

.section .init.ramfs,"a"
__irf_start:
.incbin "../build/initramfs_inc_data"
__irf_end:
.section .init.ramfs.info,"a"
.globl __initramfs_size
__initramfs_size:
#ifdef CONFIG_64BIT
	.quad __irf_end - __irf_start
#else
	.long __irf_end - __irf_start
#endif