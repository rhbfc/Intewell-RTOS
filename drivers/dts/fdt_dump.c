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

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <system/macros.h>

#include "fdt_util.h"
#include <libfdt.h>

#include <commonUtils.h>

#define FDT_MAGIC_SIZE 4

#define ALIGN(x, a) (((x) + ((a)-1)) & ~((a)-1))
#define PALIGN(p, a) ((void *)(ALIGN((unsigned long)(p), (a))))
#define GET_CELL(p) (p += 4, *((const fdt32_t *)(p - 4)))

static const char *tagname(uint32_t tag)
{
    static const char *const names[] = {
#define TN(t) [t] = #t
        TN(FDT_BEGIN_NODE), TN(FDT_END_NODE), TN(FDT_PROP), TN(FDT_NOP), TN(FDT_END),
#undef TN
    };
    if (tag < array_size(names))
        if (names[tag])
            return names[tag];
    return "FDT_???";
}

#define dumpf(fmt, args...)                                                                        \
    do                                                                                             \
    {                                                                                              \
        if (debug)                                                                                 \
            printk("// " fmt, ##args);                                                             \
    } while (0)

static bool util_is_printable_string(const void *data, int len)
{
    const char *s = data;
    const char *ss, *se;

    /* zero length is not */
    if (len == 0)
        return 0;

    /* must terminate with zero */
    if (s[len - 1] != '\0')
        return 0;

    se = s + len;

    while (s < se)
    {
        ss = s;
        while (s < se && *s && isprint((unsigned char)*s))
            s++;

        /* not zero, or not done yet */
        if (*s != '\0' || s == ss)
            return 0;

        s++;
    }

    return 1;
}

static void utilfdt_print_data(const char *data, int len)
{
    int i;
    const char *s;

    /* no data, don't print */
    if (len == 0)
        return;

    if (util_is_printable_string(data, len))
    {
        printk(" = ");

        s = data;
        do
        {
            printk("\"%s\"", s);
            s += strlen(s) + 1;
            if (s < data + len)
                printk(", ");
        } while (s < data + len);
    }
    else if ((len % 4) == 0)
    {
        const fdt32_t *cell = (const fdt32_t *)data;

        printk(" = <");
        for (i = 0, len /= 4; i < len; i++)
            printk("0x%08x%s", fdt32_to_cpu(cell[i]), i < (len - 1) ? " " : "");
        printk(">");
    }
    else
    {
        const unsigned char *p = (const unsigned char *)data;
        printk(" = [");
        for (i = 0; i < len; i++)
            printk("%02x%s", *p++, i < len - 1 ? " " : "");
        printk("]");
    }
}

static void dump_blob(void *blob, bool debug)
{
    uintptr_t blob_off = (uintptr_t)blob;
    struct fdt_header *bph = blob;
    uint32_t off_mem_rsvmap = fdt32_to_cpu(bph->off_mem_rsvmap);
    uint32_t off_dt = fdt32_to_cpu(bph->off_dt_struct);
    uint32_t off_str = fdt32_to_cpu(bph->off_dt_strings);
    struct fdt_reserve_entry *p_rsvmap =
        (struct fdt_reserve_entry *)((char *)blob + off_mem_rsvmap);
    const char *p_struct = (const char *)blob + off_dt;
    const char *p_strings = (const char *)blob + off_str;
    uint32_t version = fdt32_to_cpu(bph->version);
    uint32_t totalsize = fdt32_to_cpu(bph->totalsize);
    uint32_t tag;
    const char *p, *s, *t;
    int depth, sz, shift;
    int i;
    uint64_t addr, size;

    depth = 0;
    shift = 4;

    printk("/dts-v1/;\n");
    printk("// magic:\t\t0x%" PRIx32 "\n", fdt32_to_cpu(bph->magic));
    printk("// totalsize:\t\t0x%" PRIx32 " (%" PRIu32 ")\n", totalsize, totalsize);
    printk("// off_dt_struct:\t0x%" PRIx32 "\n", off_dt);
    printk("// off_dt_strings:\t0x%" PRIx32 "\n", off_str);
    printk("// off_mem_rsvmap:\t0x%" PRIx32 "\n", off_mem_rsvmap);
    printk("// version:\t\t%" PRIu32 "\n", version);
    printk("// last_comp_version:\t%" PRIu32 "\n", fdt32_to_cpu(bph->last_comp_version));
    if (version >= 2)
        printk("// boot_cpuid_phys:\t0x%" PRIx32 "\n", fdt32_to_cpu(bph->boot_cpuid_phys));

    if (version >= 3)
        printk("// size_dt_strings:\t0x%" PRIx32 "\n", fdt32_to_cpu(bph->size_dt_strings));
    if (version >= 17)
        printk("// size_dt_struct:\t0x%" PRIx32 "\n", fdt32_to_cpu(bph->size_dt_struct));
    printk("\n");

    for (i = 0;; i++)
    {
        addr = fdt64_to_cpu(p_rsvmap[i].address);
        size = fdt64_to_cpu(p_rsvmap[i].size);
        if (addr == 0 && size == 0)
            break;

        printk("/memreserve/ %" PRIx64 " %" PRIx64 ";\n", addr, size);
    }

    p = p_struct;
    while ((tag = fdt32_to_cpu(GET_CELL(p))) != FDT_END)
    {
        dumpf("%04" PRIxPTR ": tag: 0x%08" PRIxPTR " (%s)\n", (uintptr_t)p - blob_off - 4,
              (uintptr_t)tag, tagname(tag));

        if (tag == FDT_BEGIN_NODE)
        {
            s = p;
            p = PALIGN(p + strlen(s) + 1, 4);

            if (*s == '\0')
                s = "/";

            printk("%*s%s {\n", depth * shift, "", s);

            depth++;
            continue;
        }

        if (tag == FDT_END_NODE)
        {
            depth--;

            printk("%*s};\n", depth * shift, "");
            continue;
        }

        if (tag == FDT_NOP)
        {
            printk("%*s// [NOP]\n", depth * shift, "");
            continue;
        }

        if (tag != FDT_PROP)
        {
            printk("%*s ** Unknown tag 0x%08x\n", depth * shift, "", tag);
            break;
        }
        sz = fdt32_to_cpu(GET_CELL(p));
        s = p_strings + fdt32_to_cpu(GET_CELL(p));
        if (version < 16 && sz >= 8)
            p = PALIGN(p, 8);
        t = p;

        p = PALIGN(p + sz, 4);

        dumpf("%04" PRIxPTR ": string: %s\n", (uintptr_t)s - blob_off, s);
        dumpf("%04" PRIxPTR ": value\n", (uintptr_t)t - blob_off);
        printk("%*s%s", depth * shift, "", s);
        utilfdt_print_data(t, sz);
        printk(";\n");
    }
}

int fdt_dump(char *buf, size_t len)
{
    bool debug = false;
    bool scan = true;

    /* try and locate an embedded fdt in a bigger blob */
    if (scan)
    {
        unsigned char smagic[FDT_MAGIC_SIZE];
        char *p = buf;
        char *endp = buf + len;

        fdt32_st(smagic, FDT_MAGIC);

        /* poor man's memmem */
        while ((endp - p) >= FDT_MAGIC_SIZE)
        {
            p = memchr(p, smagic[0], endp - p - FDT_MAGIC_SIZE);
            if (!p)
                break;
            if (fdt_magic(p) == FDT_MAGIC)
            {
                /* try and validate the main struct */
                size_t this_len = endp - p;
                if (valid_header(p, this_len))
                    break;
                if (debug)
                    printk("skipping fdt magic at offset %" PRIxPTR "\n",
                           (uintptr_t)p - (uintptr_t)buf);
            }
            ++p;
        }
        if (!p || endp - p < sizeof(struct fdt_header))
        {
            printk("could not locate fdt magic\n");
            while (1)
                ;
        }
        buf = p;
    }
    else if (!valid_header(buf, len))
    {
        printk("header is not valid\n");
        while (1)
            ;
    }

    dump_blob(buf, debug);

    return 0;
}
