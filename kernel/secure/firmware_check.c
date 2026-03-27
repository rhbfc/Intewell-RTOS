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

#include <ttos_init.h>
#include <system/compiler.h>
#include <ttos.h>
#include <tiny_md5.h>

typedef struct {
    uint8_t text_md5[16];
    uint8_t rodata_md5[16];
} md5_kernel_t;

extern uint8_t _text_start[];
extern uint8_t _text_end[];
extern uint8_t _rodata_start[];
extern uint8_t _rodata_end[];

static __used md5_kernel_t kmd5 __section(".md5_section");

static void md5_dump(char *tag, uint8_t *buf)
{
    printk("md5.[%s]:", tag);
    for (size_t i = 0; i < 16; i++)
        printk("%02x", buf[i]);
    printk("\n");
}

static int md5_cmp(uint8_t *md5_1, uint8_t *md5_2)
{
    return memcmp(md5_1, md5_2, 16);
}

static int kernel_md5_check(void)
{
    uint8_t md5sum[16];

    tiny_md5(_text_start, _text_end - _text_start, md5sum);
    if (md5_cmp(md5sum, kmd5.text_md5))
    {
        printk("\x1b[7mthe 'text' segment is not secure and the md5 verification fails!\x1b[0m\n");
        md5_dump("text", md5sum);
        md5_dump("read text", kmd5.text_md5);
        while (1);
    }

    tiny_md5(_rodata_start, _rodata_end - _rodata_start, md5sum);
    if (md5_cmp(md5sum, kmd5.rodata_md5))
    {
        printk("\x1b[7mthe 'rodata' segment is not secure and the md5 verification fails!\x1b[0m\n");
        md5_dump("rodata", md5sum);
        md5_dump("read rodata", kmd5.rodata_md5);
        while (1);
    }
    return 0;
}
INIT_EXPORT_ARCH(kernel_md5_check, "kernel_md5_check");