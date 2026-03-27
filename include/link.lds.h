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

#define LDS_PAGE_SIZE 4096
#define LOAD_OFFSET   0

// clang-format off

/**
 * PERCPU_INPUT - the percpu input sections
 * @cacheline: cacheline size
 *
 * The core percpu section names and core symbols which do not rely
 * directly upon load addresses.
 *
 * @cacheline is used to align subsections to avoid false cacheline
 * sharing between subsections for different purposes.
 */
#define PERCPU_INPUT(cacheline)         \
    __per_cpu_start = .;                \
    *(.data..percpu..first)             \
    . = ALIGN(LDS_PAGE_SIZE);           \
    *(.data..percpu)                    \
    . = ALIGN(cacheline);               \
    __per_cpu_end = .;


/**
 * PERCPU_SECTION - define output section for percpu area, simple version
 * @cacheline: cacheline size
 *
 * Align to LDS_PAGE_SIZE and outputs output section for percpu area.  This
 * macro doesn't manipulate @vaddr or @phdr and __per_cpu_load and
 * __per_cpu_start will be identical.
 *
 * This macro is equivalent to ALIGN(LDS_PAGE_SIZE); PERCPU_VADDR(@cacheline,,)
 * except that __per_cpu_load is defined as a relative symbol against
 * .data..percpu which is required for relocatable x86_32 configuration.
 */
#define PERCPU_SECTION(cacheline)                               \
    . = ALIGN(LDS_PAGE_SIZE);                                   \
    .data..percpu    : AT(ADDR(.data..percpu) - LOAD_OFFSET) {  \
        __per_cpu_load = .;                                     \
        PERCPU_INPUT(cacheline)                                 \
    }


#ifdef CONFIG_ROOTFS_CPIO
#define INIT_RAM_FS							\
	. = ALIGN(4);							\
	__initramfs_start = .;						\
	KEEP(*(.init.ramfs))						\
	. = ALIGN(8);							\
	KEEP(*(.init.ramfs.info))
#else
#define INIT_RAM_FS
#endif

// clang-format on
