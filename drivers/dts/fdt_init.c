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

#include "fdt_util.h"
#include <cache.h>
#include <driver/of.h>
#include <libfdt.h>
#include <list.h>
#include <shell.h>
#include <stdbool.h>
#include <stdio.h>
#include <system/kconfig.h>
#include <ttosMM.h>
#include <ttos_init.h>

#define KLOG_TAG "FDT"
#include <klog.h>

#define FDT_SIZE_MAX (512 * 1024)
#define FDT_MAGIC_SIZE 4

static void *fdt_root;
static struct device_node *of_root;
__aligned(1024) static uint32_t dtb_file[FDT_SIZE_MAX];

void *fdt_root_get(void)
{
    return fdt_root;
}

struct device_node *of_get_root_node(void)
{
    return of_root;
}

static struct property *property_create(const char *name, struct device_node *parent,
                                        struct property *prev, const void *value, size_t value_len)
{
    struct property *property = malloc(sizeof(struct property));
    if (property == NULL)
        return NULL;
    if (prev)
    {
        prev->next = property;
    }
    property->name = strdup(name);
    property->parent = parent;
    property->value = malloc(value_len);
    memcpy(property->value, value, value_len);
    property->value_len = value_len;
    property->next = NULL;

    return property;
}

static int device_node_init(struct device_node *node)
{
    memset(node, 0, sizeof(*node));
    return 0;
}

static struct device_node *device_node_create(void)
{
    struct device_node *node = malloc(sizeof(struct device_node));
    device_node_init(node);
    return node;
}

static int parse_property(void *fdt, int node, struct device_node *parent)
{
    int property_off = 0;
    struct property *property = NULL;
    const void *property_value;
    int property_value_len = 0;
    const char *name;
    fdt_for_each_property_offset(property_off, fdt, node)
    {
        property_value = fdt_getprop_by_offset(fdt, property_off, &name, &property_value_len);
        if (property_value)
        {
            struct property *new_property;
            new_property =
                property_create(name, parent, property, property_value, property_value_len);
            if (new_property == NULL)
            {
                return -1;
            }
            if (parent->property == NULL)
            {
                parent->property = new_property;
            }
            else if (property == NULL)
            {
                free(new_property->value);
                free((void *)new_property->name);
                free(new_property);
                new_property = NULL;
            }
            property = new_property;

            if (!strcmp(name, "phandle") || !strcmp(name, "linux,phandle"))
            {
                if (!parent->phandle)
                    parent->phandle = fdt32_to_cpu(*(fdt32_t *)property_value);
            }
        }
    }
    return 0;
}

static int parse_subnode(void *fdt, int parentoffset, struct device_node *parent)
{
    int node;
    struct device_node *device_node = NULL;
    const char *name;
    char *fullpath;
    fdt_for_each_subnode(node, fdt, parentoffset)
    {
        name = fdt_get_name(fdt, node, NULL);
        if (parent != NULL && parent->child == NULL)
        {
            parent->child = device_node_create();
            device_node = parent->child;
        }
        else
        {
            if (device_node)
            {
                device_node->sibling = device_node_create();
                device_node = device_node->sibling;
            }
        }

        if (device_node)
        {
            device_node->parent = parent;
            device_node->name = strdup(name);
            fullpath =
                malloc(strlen(device_node->parent->full_name) + 1 + strlen(device_node->name) + 1);
            strcpy(fullpath, device_node->parent->full_name);
            strcat(fullpath, "/");
            strcat(fullpath, device_node->name);
            device_node->full_name = fullpath;

            parse_property(fdt, node, device_node);

            device_node->type = of_node_get_device_type(device_node);
        }

        if (fdt_first_subnode(fdt, node) != -FDT_ERR_NOTFOUND)
        {
            parse_subnode(fdt, node, device_node);
        }
    }
    return 0;
}

static int fdt_init_parse(void *fdt)
{
    int offset = 0;
    of_root = device_node_create();
    of_root->name = strdup("root");
    of_root->type = strdup("root");
    of_root->full_name = strdup("");

    parse_property(fdt, offset, of_root);
    parse_subnode(fdt, offset, of_root);

    return 0;
}
void dtb_copy(void *address, long pv_offset)
{
    register uint32_t size;
    register uint32_t *src_addr = address;
    register uint32_t *dtc_addr = dtb_file;

#ifndef CONFIG_ARCH_AARCH64
    dtc_addr = (void *)dtc_addr + pv_offset;
#endif

    if ((long)src_addr & 0x3)
    {
        return;
    }

    if (fdt_magic(src_addr) != FDT_MAGIC)
    {
        return;
    }
    size = fdt_totalsize(src_addr) / sizeof(uint32_t);
    size = size < FDT_SIZE_MAX ? size : FDT_SIZE_MAX;

    while (size--)
    {
        *dtc_addr = *src_addr;
        dtc_addr++;
        src_addr++;
    }
}

static void *board_get_dtb(size_t *size)
{
    extern int dtb_data, dtb_end;
    size_t sz = (uintptr_t)&dtb_end - (uintptr_t)&dtb_data;

    if (size && (sz > 0))
    {
        *size = sz;
        return (void *)&dtb_data;
    }

    return 0;
}

static void *dtb_get(size_t *size)
{
    volatile void *p = NULL;

    if (fdt_magic(dtb_file) == FDT_MAGIC)
    {
        p = dtb_file;
    }
    else
    {
        p = board_get_dtb(size);
        while (p == NULL)
            ;
    }
    if (size)
        *size = fdt_totalsize(p);

    return (void *)p;
}

void of_earlycon_uart_init(const void *fdt);
#include <commonUtils.h>

// 默认值
#define DEFAULT_ADDRESS_CELLS 2
#define DEFAULT_SIZE_CELLS 1

// 递归获取 #address-cells 属性
static int get_address_cells(const void *fdt, int node_offset)
{
    const int *prop;
    int len;

    while (node_offset >= 0)
    {
        prop = fdt_getprop(fdt, node_offset, "#address-cells", &len);
        if (prop && len == sizeof(int))
        {
            return fdt32_to_cpu(*prop); // 转换为主机字节序
        }
        node_offset = fdt_parent_offset(fdt, node_offset); // 向上查找父节点
    }

    return DEFAULT_ADDRESS_CELLS; // 使用默认值
}

// 递归获取 #size-cells 属性
static int get_size_cells(const void *fdt, int node_offset)
{
    const int *prop;
    int len;

    while (node_offset >= 0)
    {
        prop = fdt_getprop(fdt, node_offset, "#size-cells", &len);
        if (prop && len == sizeof(int))
        {
            return fdt32_to_cpu(*prop); // 转换为主机字节序
        }
        node_offset = fdt_parent_offset(fdt, node_offset); // 向上查找父节点
    }

    return DEFAULT_SIZE_CELLS; // 使用默认值
}

int fdt_foreach_memory(void (*func)(phys_addr_t addr, phys_addr_t size, void *ctx), void *ctx)
{
    size_t fdt_size;
    void *fdt = dtb_get(&fdt_size);
    int offset = 0;

    // 遍历所有节点
    while ((offset = fdt_next_node(fdt, offset, NULL)) >= 0)
    {
        // 获取 device_type 属性
        const char *device_type = fdt_getprop(fdt, offset, "device_type", NULL);
        if (device_type && strcmp(device_type, "memory") == 0)
        {
            int len = 0;
            const void *prop = fdt_getprop(fdt, offset, "reg", &len);
            int addr_cells = get_address_cells(fdt, offset);
            int size_cells = get_size_cells(fdt, offset);

            for (int i = 0; i < len; i += (addr_cells + size_cells) * 4)
            {
                phys_addr_t addr, size;
                if (addr_cells == 2)
                {
                    addr = fdt64_ld(prop + i);
                    if (size_cells == 2)
                    {
                        size = fdt64_ld(prop + i + 8);
                    }
                    else
                    {
                        size = fdt32_ld(prop + i + 8);
                    }
                }
                else
                {
                    addr = fdt32_ld(prop + i);
                    if (size_cells == 2)
                    {
                        size = fdt64_ld(prop + i + 4);
                    }
                    else
                    {
                        size = fdt32_ld(prop + i + 4);
                    }
                }
                if (size == 0)
                {
                    continue;
                }
                func(addr, size, ctx);
            }
        }
    }
    return 0;
}

int fdt_foreach_memory_reserved(void (*func)(phys_addr_t addr, phys_addr_t size, void *ctx),
                                void *ctx)
{
    size_t fdt_size;
    void *fdt = dtb_get(&fdt_size);
    int resv_num = fdt_num_mem_rsv(fdt);
    if (resv_num < 0)
    {
        return resv_num;
    }
    for (int i = 0; i < resv_num; i++)
    {
        uint64_t addr, size;
        if (fdt_get_mem_rsv(fdt, i, &addr, &size) == 0)
        {
            func(addr, size, ctx);
        }
    }
    return 0;
}

void earlycon_uart_init(const void *fdt);
void earlycon_init(void)
{
    void *fdt = NULL;

    /* arm架构下earlycon初始化通过设备树解析得到基地址，x86跳过这一步 */
    if (!IS_ENABLED(__x86_64__))
    {
        size_t fdt_size;
        fdt = dtb_get(&fdt_size);
    }

    earlycon_uart_init(fdt);
}

int device_system_init(void);
int fdt_init(void)
{
    unsigned char smagic[FDT_MAGIC_SIZE];
    size_t fdt_size;
    void *fdt = dtb_get(&fdt_size);
    char *p = fdt;
    char *endp = p + fdt_size;

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
            printk("skipping fdt magic at offset %#tx\n", (uintptr_t)p - (uintptr_t)fdt);
        }
        ++p;
    }
    if (!p || endp - p < sizeof(struct fdt_header))
    {
        printk("could not locate fdt magic\n");
        return -1;
    }
    fdt = p;
    fdt_root = fdt;
    fdt_init_parse(fdt);
    of_core_init();
    return 0;
}

#ifndef CONFIG_X86_64
INIT_EXPORT_PRE(fdt_init, "fdt_init");
#endif

#ifdef CONFIG_SHELL
static void _fdt_dump(void)
{
    size_t fdt_size;
    void *fdt = dtb_get(&fdt_size);

    fdt_dump(fdt, fdt_size);
}
SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_FUNC) |
                     SHELL_CMD_DISABLE_RETURN,
                 fdt_dump, _fdt_dump, dump fdt);
#endif
