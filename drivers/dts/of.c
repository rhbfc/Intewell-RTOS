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

#include <driver/device.h>
#include <driver/of.h>
#include <errno.h>
#include <hash.h>
#include <io.h>
#include <spinlock.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <ttosMM.h>
#include <ttos_pic.h>

#define KLOG_TAG "OF"
#include <klog.h>

static DEFINE_SPINLOCK(devtree_lock);

#define OF_PHANDLE_CACHE_BITS 7
#define OF_PHANDLE_CACHE_SZ (0x01 << OF_PHANDLE_CACHE_BITS)

static struct device_node *phandle_cache[OF_PHANDLE_CACHE_SZ];

static uint32_t of_phandle_cache_hash(phandle handle)
{
    return hash_32(handle, OF_PHANDLE_CACHE_BITS);
}

void of_core_init(void)
{
    struct device_node *np;

    /* Create the kset, and register existing nodes */
    // mutex_lock(&of_mutex);
    // of_kset = kset_create_and_add("devicetree", NULL, firmware_kobj);
    // if (!of_kset) {
    // 	mutex_unlock(&of_mutex);
    // 	pr_err("failed to register existing nodes\n");
    // 	return;
    // }
    for_each_of_allnodes(np)
    {
        // __of_attach_node_sysfs(np);
        if (np->phandle && !phandle_cache[of_phandle_cache_hash(np->phandle)])
            phandle_cache[of_phandle_cache_hash(np->phandle)] = np;
    }
    // mutex_unlock(&of_mutex);

    // /* Symlink in /proc as required by userspace ABI */
    // if (of_root)
    // 	proc_symlink("device-tree", NULL, "/sys/firmware/devicetree/base");
}

static struct device_node *__of_get_next_child(const struct device_node *node,
                                               struct device_node *prev)
{
    struct device_node *next;

    if (!node)
        return NULL;

    next = prev ? prev->sibling : node->child;
    of_node_get(next);
    of_node_put(prev);
    return next;
}

/**
 * of_get_next_child - Iterate a node childs
 * @node:	parent node
 * @prev:	previous child of the parent node, or NULL to get first
 *
 * Return: A node pointer with refcount incremented, use of_node_put() on
 * it when done. Returns NULL when prev is the last child. Decrements the
 * refcount of prev.
 */
struct device_node *of_get_next_child(const struct device_node *node, struct device_node *prev)
{
    struct device_node *next;
    unsigned long flags;

    spin_lock_irqsave(&devtree_lock, flags);
    next = __of_get_next_child(node, prev);
    spin_unlock_irqrestore(&devtree_lock, flags);
    return next;
}

static struct property *__of_find_property(const struct device_node *np, const char *name,
                                           int *lenp)
{
    struct property *pp;

    if (!np)
        return NULL;

    for (pp = np->property; pp; pp = pp->next)
    {
        if (strcmp(pp->name, name) == 0)
        {
            if (lenp)
                *lenp = pp->value_len;
            break;
        }
    }

    return pp;
}

struct property *of_find_property(const struct device_node *np, const char *name, int *lenp)
{
    struct property *pp;
    unsigned long flags;

    spin_lock_irqsave(&devtree_lock, flags);
    pp = __of_find_property(np, name, lenp);
    spin_unlock_irqrestore(&devtree_lock, flags);

    return pp;
}

const void *of_get_property(const struct device_node *np, const char *name, int *lenp)
{
    struct property *pp = of_find_property(np, name, lenp);

    return pp ? pp->value : NULL;
}

int of_property_count_elems_of_size(const struct device_node *np, const char *propname,
                                    int elem_size)
{
    struct property *prop = of_find_property(np, propname, NULL);

    if (!prop)
        return -EINVAL;
    if (!prop->value)
        return -ENOENT;

    if (prop->value_len % elem_size != 0)
    {
        KLOG_E("size of %s in node %pOF is not a multiple of %d", propname, np, elem_size);
        return -EINVAL;
    }

    return prop->value_len / elem_size;
}

int of_property_read_string_helper(const struct device_node *np, const char *propname,
                                   const char **out_strs, size_t sz, int skip)
{
    const struct property *prop = of_find_property(np, propname, NULL);
    int l = 0, i = 0;
    const char *p, *end;

    if (!prop)
        return -EINVAL;
    if (!prop->value)
        return -ENOENT;
    p = (const char *)prop->value;
    end = p + prop->value_len;

    for (i = 0; p < end && (!out_strs || i < skip + sz); i++, p += l)
    {
        l = strnlen(p, end - p) + 1;
        if (p + l > end)
            return -EILSEQ;
        if (out_strs && i >= skip)
            *out_strs++ = p;
    }
    i -= skip;
    return i <= 0 ? -ENOENT : i;
}

static void *of_find_property_value_of_size(const struct device_node *np, const char *propname,
                                            uint32_t min, uint32_t max, size_t *len)
{
    struct property *prop = of_find_property(np, propname, NULL);

    if (!prop)
    {
        errno = EINVAL;
        return NULL;
    }
    if (!prop->value)
    {
        errno = ENOENT;
        return NULL;
    }
    if (prop->value_len < min)
    {
        errno = EOVERFLOW;
        return NULL;
    }
    if (max && prop->value_len > max)
    {
        errno = EOVERFLOW;
        return NULL;
    }

    if (len)
        *len = prop->value_len;

    return prop->value;
}

int of_property_read_variable_u8_array(const struct device_node *np, const char *propname,
                                       uint8_t *out_values, size_t sz_min, size_t sz_max)
{
    size_t sz, count;
    const uint8_t *val = of_find_property_value_of_size(
        np, propname, (sz_min * sizeof(*out_values)), (sz_max * sizeof(*out_values)), &sz);

    if (val == NULL)
        return -errno;

    if (!sz_max)
        sz = sz_min;
    else
        sz /= sizeof(*out_values);

    count = sz;
    while (count--)
        *out_values++ = *val++;

    return sz;
}

int of_property_read_variable_u16_array(const struct device_node *np, const char *propname,
                                        uint16_t *out_values, size_t sz_min, size_t sz_max)
{
    size_t sz, count;
    const fdt16_t *val = of_find_property_value_of_size(
        np, propname, (sz_min * sizeof(*out_values)), (sz_max * sizeof(*out_values)), &sz);

    if (val == NULL)
        return -errno;

    if (!sz_max)
        sz = sz_min;
    else
        sz /= sizeof(*out_values);

    count = sz;
    while (count--)
        *out_values++ = fdt16_to_cpu(*val++);

    return sz;
}

int of_property_read_variable_u32_array(const struct device_node *np, const char *propname,
                                        uint32_t *out_values, size_t sz_min, size_t sz_max)
{
    size_t sz, count;
    const fdt32_t *val = of_find_property_value_of_size(
        np, propname, (sz_min * sizeof(*out_values)), (sz_max * sizeof(*out_values)), &sz);

    if (val == NULL)
        return -errno;

    if (!sz_max)
        sz = sz_min;
    else
        sz /= sizeof(*out_values);

    count = sz;
    while (count--)
        *out_values++ = fdt32_to_cpu(*val++);

    return sz;
}

int of_property_read_u64(const struct device_node *np, const char *propname, uint64_t *out_value)
{
    const fdt32_t *val = of_find_property_value_of_size(np, propname, sizeof(*out_value), 0, NULL);

    if (val == NULL)
        return -errno;

    *out_value = of_read_number(val, 2);
    return 0;
}

int of_property_read_variable_u64_array(const struct device_node *np, const char *propname,
                                        uint64_t *out_values, size_t sz_min, size_t sz_max)
{
    size_t sz, count;
    const fdt32_t *val = of_find_property_value_of_size(
        np, propname, (sz_min * sizeof(*out_values)), (sz_max * sizeof(*out_values)), &sz);

    if (val == NULL)
        return -errno;

    if (!sz_max)
        sz = sz_min;
    else
        sz /= sizeof(*out_values);

    count = sz;
    while (count--)
    {
        *out_values++ = of_read_number(val, 2);
        val += 2;
    }

    return sz;
}

int of_property_read_string(const struct device_node *np, const char *propname,
                            const char **out_string)
{
    const struct property *prop = of_find_property(np, propname, NULL);
    if (!prop)
        return -EINVAL;
    if (!prop->value_len)
        return -ENOENT;
    if (strnlen((const char *)prop->value, prop->value_len) >= prop->value_len)
        return -EILSEQ;
    *out_string = (const char *)prop->value;
    return 0;
}

int of_property_match_string(const struct device_node *np, const char *propname, const char *string)
{
    const struct property *prop = of_find_property(np, propname, NULL);
    size_t l;
    int i;
    const char *p, *end;

    if (!prop)
        return -EINVAL;
    if (!prop->value)
        return -ENOENT;

    p = (const char *)prop->value;
    end = p + prop->value_len;

    for (i = 0; p < end; i++, p += l)
    {
        l = strnlen(p, end - p) + 1;
        if (p + l > end)
            return -EILSEQ;
        if (strcmp(string, p) == 0)
            return i; /* Found it; return index */
    }
    return -ENOENT;
}

const fdt32_t *of_prop_next_u32(struct property *prop, const fdt32_t *cur, uint32_t *pu)
{
    const void *curv = cur;

    if (!prop)
        return NULL;

    if (!cur)
    {
        curv = prop->value;
        goto out_val;
    }

    curv += sizeof(*cur);
    if ((uintptr_t)curv >= (uintptr_t)(prop->value + prop->value_len))
        return NULL;

out_val:
    *pu = fdt32_to_cpu(*(fdt32_t *)curv);
    return curv;
}

const char *of_prop_next_string(struct property *prop, const char *cur)
{
    const void *curv = cur;

    if (!prop)
        return NULL;

    if (!cur)
        return (const char *)prop->value;

    curv += strlen(cur) + 1;
    if ((uintptr_t)curv >= (uintptr_t)(prop->value + prop->value_len))
        return NULL;

    return curv;
}

static bool __of_device_is_available(const struct device_node *device)
{
    const char *status;
    const struct property *prop;
    int statlen;

    if (!device)
        return false;

    /* Caller holds devtree_lock, so use internal helper to avoid lock recursion. */
    prop = __of_find_property(device, "status", &statlen);
    status = prop ? (const char *)prop->value : NULL;
    if (status == NULL)
        return true;

    if (statlen > 0)
    {
        if (!strcmp(status, "okay") || !strcmp(status, "ok"))
            return true;
    }

    return false;
}

bool of_device_is_available(const struct device_node *device)
{
    unsigned long flags;
    bool res;

    spin_lock_irqsave(&devtree_lock, flags);
    res = __of_device_is_available(device);
    spin_unlock_irqrestore(&devtree_lock, flags);
    return res;
}

static int __of_device_is_compatible(const struct device_node *device, const char *compat,
                                     const char *type, const char *name)
{
    struct property *prop;
    const char *cp;
    int index = 0, score = 0;

    /* Compatible match has highest priority */
    if (compat && compat[0])
    {
        prop = __of_find_property(device, "compatible", NULL);
        for (cp = of_prop_next_string(prop, NULL); cp; cp = of_prop_next_string(prop, cp), index++)
        {
            if (strcmp(cp, compat) == 0)
            {
                score = INT_MAX / 2 - (index << 2);
                break;
            }
        }
        if (!score)
            return 0;
    }

    /* Matching type is better than matching name */
    if (type && type[0] && device->type && device->type[0])
    {
        if (!strcmp(device->type, type))
            return 0;
        score += 2;
    }

    /* Matching name is a bit better than not */
    if (name && name[0] && device->name && device->name[0])
    {
        if (!strcmp(device->name, name))
            return 0;
        score++;
    }

    return score;
}

static const struct of_device_id *__of_match_node(const struct of_device_id *matches,
                                                  const struct device_node *node)
{
    const struct of_device_id *best_match = NULL;
    int score, best_score = 0;

    if (!matches)
        return NULL;

    for (; matches->name[0] || matches->type[0] || matches->compatible[0]; matches++)
    {
        score = __of_device_is_compatible(node, matches->compatible, matches->type, matches->name);
        if (score > best_score)
        {
            best_match = matches;
            best_score = score;
        }
    }

    return best_match;
}

/**
 * of_match_node - Tell if a device_node has a matching of_match structure
 * @matches:	array of of device match structures to search in
 * @node:	the of device structure to match against
 *
 * Low level utility function used by device matching.
 */
const struct of_device_id *of_match_node(const struct of_device_id *matches,
                                         const struct device_node *node)
{
    const struct of_device_id *match;
    unsigned long flags;

    spin_lock_irqsave(&devtree_lock, flags);
    match = __of_match_node(matches, node);
    spin_unlock_irqrestore(&devtree_lock, flags);
    return match;
}

int of_device_is_compatible(const struct device_node *device, const char *compat)
{
    unsigned long flags;
    int res;

    spin_lock_irqsave(&devtree_lock, flags);
    res = __of_device_is_compatible(device, compat, NULL, NULL);
    spin_unlock_irqrestore(&devtree_lock, flags);
    return res;
}

struct device_node *__of_find_all_nodes(struct device_node *prev)
{
    struct device_node *np;
    if (!prev)
    {
        np = of_get_root_node();
    }
    else if (prev->child)
    {
        np = prev->child;
    }
    else
    {
        /* Walk back up looking for a sibling, or the end of the structure */
        np = prev;
        while (np->parent && !np->sibling)
            np = np->parent;
        np = np->sibling; /* Might be null at the end of the tree */
    }
    return np;
}

struct device_node *of_find_all_nodes(struct device_node *prev)
{
    struct device_node *np;
    unsigned long flags;

    spin_lock_irqsave(&devtree_lock, flags);
    np = __of_find_all_nodes(prev);
    of_node_get(np);
    of_node_put(prev);
    spin_unlock_irqrestore(&devtree_lock, flags);
    return np;
}

struct device_node *of_find_compatible_node(struct device_node *from, const char *type,
                                            const char *compatible)
{
    struct device_node *np;
    unsigned long flags;

    spin_lock_irqsave(&devtree_lock, flags);
    for_each_of_allnodes_from(from, np) if (__of_device_is_compatible(np, compatible, type, NULL) &&
                                            of_node_get(np)) break;
    of_node_put(from);
    spin_unlock_irqrestore(&devtree_lock, flags);
    return np;
}

struct device_node *of_find_node_with_property(struct device_node *from, const char *prop_name)
{
    struct device_node *np;
    struct property *pp;
    unsigned long flags;

    spin_lock_irqsave(&devtree_lock, flags);
    for_each_of_allnodes_from(from, np)
    {
        for (pp = np->property; pp; pp = pp->next)
        {
            if (strcmp(pp->name, prop_name) == 0)
            {
                of_node_get(np);
                goto out;
            }
        }
    }
out:
    of_node_put(from);
    spin_unlock_irqrestore(&devtree_lock, flags);
    return np;
}

int of_phandle_iterator_init(struct of_phandle_iterator *it, const struct device_node *np,
                             const char *list_name, const char *cells_name, int cell_count)
{
    const fdt32_t *list;
    int size;

    memset(it, 0, sizeof(*it));

    /*
     * one of cell_count or cells_name must be provided to determine the
     * argument length.
     */
    if (cell_count < 0 && !cells_name)
        return -EINVAL;

    list = of_get_property(np, list_name, &size);
    if (!list)
        return -ENOENT;

    it->cells_name = cells_name;
    it->cell_count = cell_count;
    it->parent = np;
    it->list_end = list + size / sizeof(*list);
    it->phandle_end = list;
    it->cur = list;

    return 0;
}

struct device_node *of_find_node_by_phandle(phandle handle)
{
    struct device_node *np = NULL;
    unsigned long flags;
    uint32_t handle_hash;

    if (!handle)
        return NULL;

    handle_hash = of_phandle_cache_hash(handle);

    spin_lock_irqsave(&devtree_lock, flags);

    if (phandle_cache[handle_hash] && handle == phandle_cache[handle_hash]->phandle)
        np = phandle_cache[handle_hash];

    if (!np)
    {
        for_each_of_allnodes(np)
			if (np->phandle == handle/* &&
			    !of_node_check_flag(np, OF_DETACHED)*/)
        {
            phandle_cache[handle_hash] = np;
            break;
        }
    }

    of_node_get(np);
    spin_unlock_irqrestore(&devtree_lock, flags);
    return np;
}

int of_phandle_iterator_args(struct of_phandle_iterator *it, uint32_t *args, int size)
{
    int i, count;

    count = it->cur_count;

    if (size < count)
        count = size;

    for (i = 0; i < count; i++)
        args[i] = fdt32_to_cpu(*it->cur++);

    return count;
}

int of_phandle_iterator_next(struct of_phandle_iterator *it)
{
    uint32_t count = 0;

    if (it->node)
    {
        of_node_put(it->node);
        it->node = NULL;
    }

    if (!it->cur || it->phandle_end >= it->list_end)
        return -ENOENT;

    it->cur = it->phandle_end;

    /* If phandle is 0, then it is an empty entry with no arguments. */
    it->phandle = fdt32_to_cpu(*it->cur++);

    if (it->phandle)
    {
        /*
         * Find the provider node and parse the #*-cells property to
         * determine the argument length.
         */
        it->node = of_find_node_by_phandle(it->phandle);

        if (it->cells_name)
        {
            if (!it->node)
            {
                printf("%pOF: could not find phandle %d\n", it->parent, it->phandle);
                goto err;
            }

            if (of_property_read_u32(it->node, it->cells_name, &count))
            {
                /*
                 * If both cell_count and cells_name is given,
                 * fall back to cell_count in absence
                 * of the cells_name property
                 */
                if (it->cell_count >= 0)
                {
                    count = it->cell_count;
                }
                else
                {
                    printf("%pOF: could not get %s for %pOF\n", it->parent, it->cells_name,
                           it->node);
                    goto err;
                }
            }
        }
        else
        {
            count = it->cell_count;
        }

        /*
         * Make sure that the arguments actually fit in the remaining
         * property data length
         */
        if (it->cur + count > it->list_end)
        {
            if (it->cells_name)
                printf("%pOF: %s = %d found %td\n", it->parent, it->cells_name, count,
                       it->list_end - it->cur);
            else
                printf("%pOF: phandle %s needs %d, found %td\n", it->parent,
                       of_node_full_name(it->node), count, it->list_end - it->cur);
            goto err;
        }
    }

    it->phandle_end = it->cur + count;
    it->cur_count = count;

    return 0;

err:
    if (it->node)
    {
        of_node_put(it->node);
        it->node = NULL;
    }

    return -EINVAL;
}

uint32_t ofnode_read_u32_default(const struct device_node *np, const char *propname,
                                 uint32_t default_value)
{
    uint32_t value = default_value;
    int ret;
    ret = of_property_read_u32(np, propname, &value);
    if (ret < 0)
    {
        value = default_value;
    }
    return value;
}

int of_device_get_resource_reg(struct device *dev, phys_addr_t *reg_addr, size_t *reg_size)
{
    int ret;
    int i;
    uint32_t address_cells;
    uint32_t size_cells;
    uint32_t valuetable[4];
    uint64_t value;
    /* 先获取全局的 cell 大小 */
    of_property_read_u32(of_get_root_node(), "#address-cells", &address_cells);
    of_property_read_u32(of_get_root_node(), "#size-cells", &size_cells);

    /* 尝试获取node的cell大小*/
    of_property_read_u32(dev->of_node, "#address-cells", &address_cells);
    of_property_read_u32(dev->of_node, "#size-cells", &size_cells);

    if (size_cells == 0)
    {
        size_cells = 1;
    }
    ret = of_property_read_u32_array(dev->of_node, "reg", valuetable, address_cells + size_cells);

    if (ret < 0 || address_cells > 2 || size_cells > 2)
    {
        return -ENOENT;
    }
    value = 0;
    for (i = 0; i < address_cells; i++)
    {
        value <<= 32;
        value |= valuetable[i];
    }
    *reg_addr = value;

    value = 0;
    for (; i < (address_cells + size_cells); i++)
    {
        value <<= 32;
        value |= valuetable[i];
    }
    *reg_size = (size_t)value;

    return 0;
}

int of_device_get_resource_regs(struct device *dev, devaddr_region_t *region, size_t region_count)
{
    int ret;
    int i;
    int count;
    uint32_t address_cells;
    uint32_t size_cells;
    uint32_t *valuetable;
    uint32_t *value_addr;
    uint64_t value;
    /* 先获取全局的 cell 大小 */
    of_property_read_u32(of_get_root_node(), "#address-cells", &address_cells);
    of_property_read_u32(of_get_root_node(), "#size-cells", &size_cells);

    /* 尝试获取node的cell大小*/
    of_property_read_u32(dev->of_node, "#address-cells", &address_cells);
    of_property_read_u32(dev->of_node, "#size-cells", &size_cells);

    /* 获取reg 以u32为单位的个数 */
    count = of_property_count_elems_of_size(dev->of_node, "reg", address_cells * size_cells * 4);
    if (count < 0)
        return -1;

    value_addr = calloc(1, address_cells * size_cells * 4 * count);

    /* 全部取出 */
    ret = of_property_read_u32_array(dev->of_node, "reg", value_addr,
                                     address_cells * size_cells * count);

    if (ret < 0 || address_cells > 2 || size_cells > 2)
    {
        free(value_addr);
        return -ENOENT;
    }

    count = count < region_count ? count : region_count;

    for (valuetable = value_addr; count > 0; count--)
    {
        for (value = 0, i = 0; i < address_cells; i++)
        {
            value <<= 32;
            value |= valuetable[i];
        }
        region->paddr = value;

        for (value = 0; i < (address_cells + size_cells); i++)
        {
            value <<= 32;
            value |= valuetable[i];
        }
        region->size = (size_t)value;
        region->vaddr = (virt_addr_t)ioremap(region->paddr, region->size);

        region++;
        valuetable += i;
    }

    free(value_addr);

    return 0;
}

int of_device_get_resource_pic_irq(struct device *dev, struct ttos_pic **pic, int *irq_no,
                                   int irq_no_count, int *irq_count)
{
    int ret = 0;
    int i;
    struct device_node *interrupt_parent;
    struct device *pic_dev;
    uint32_t interrupt_cells;
    uint32_t interrupt_num;
    uint32_t *valuetable;

    /* 尝试获取node的cell大小*/
    interrupt_parent = of_parse_phandle(dev->of_node, "interrupt-parent", 0);

    if (interrupt_parent == NULL)
    {
        /* 先获取root的中断控制器 */
        interrupt_parent = of_parse_phandle(of_get_root_node(), "interrupt-parent", 0);

        if (interrupt_parent == NULL)
        {
            return -ENOENT;
        }
    }

    pic_dev = (struct device *)interrupt_parent->device;
    *pic = (struct ttos_pic *)pic_dev->priv;

    ret = of_property_read_u32(interrupt_parent, "#interrupt-cells", &interrupt_cells);
    if (ret < 0)
        return -ENOENT;

    interrupt_num = of_property_count_u32_elems(dev->of_node, "interrupts");
    interrupt_num /= interrupt_cells;

    irq_no_count = irq_no_count <= interrupt_num ? irq_no_count : interrupt_num;

    valuetable = malloc(sizeof(uint32_t) * irq_no_count * interrupt_cells);

    ret = of_property_read_u32_array(dev->of_node, "interrupts", valuetable,
                                     irq_no_count * interrupt_cells);

    for (i = 0; i < irq_no_count; i++)
    {
        irq_no[i] =
            ttos_pic_irq_parse((struct ttos_pic *)pic_dev->priv, valuetable + interrupt_cells * i);
    }

    if (irq_count)
        *irq_count = irq_no_count;

    free(valuetable);

    return ret;
}
