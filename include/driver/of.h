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

#ifndef __OF_H__
#define __OF_H__

#include <libfdt.h>

typedef uint32_t phandle;
typedef uint32_t ihandle;

struct property
{
    struct property *next;
    struct device_node *parent;
    const char *name;
    size_t value_len;
    uint8_t *value;
};

struct device_node
{
    const char *name;
    const char *type;
    const char *full_name;
    phandle phandle;
    struct device_node *parent;
    struct device_node *child;
    struct device_node *sibling;
    struct property *property;
    void *data;
    void *device;
};

#define MAX_PHANDLE_ARGS 16
struct of_phandle_args
{
    struct device_node *np;
    int args_count;
    uint32_t args[MAX_PHANDLE_ARGS];
};

struct of_phandle_iterator
{
    /* Common iterator information */
    const char *cells_name;
    int cell_count;
    const struct device_node *parent;

    /* List size information */
    const fdt32_t *list_end;
    const fdt32_t *phandle_end;

    /* Current position state */
    const fdt32_t *cur;
    uint32_t cur_count;
    phandle phandle;
    struct device_node *node;
};

struct of_device_id
{
    char name[32];
    char type[32];
    char compatible[128];
    const void *data;
};

void of_core_init(void);

void *fdt_root_get(void);
struct device_node *of_get_root_node(void);

struct device_node *of_get_next_child(const struct device_node *node, struct device_node *prev);

struct property *of_find_property(const struct device_node *np, const char *name, int *lenp);

const void *of_get_property(const struct device_node *np, const char *name, int *lenp);

static inline const char *of_node_get_device_type(const struct device_node *np)
{
    return of_get_property(np, "device_type", NULL);
}

bool of_device_is_available(const struct device_node *device);

static inline bool of_node_is_type(const struct device_node *np, const char *type)
{
    const char *match = of_node_get_device_type(np);

    return np && match && type && !strcmp(match, type);
}

static inline const char *of_node_full_name(const struct device_node *np)
{
    return np ? np->full_name : "<no-node>";
}

int of_property_count_elems_of_size(const struct device_node *np, const char *propname,
                                    int elem_size);

static inline int of_property_count_u8_elems(const struct device_node *np, const char *propname)
{
    return of_property_count_elems_of_size(np, propname, sizeof(uint8_t));
}

static inline int of_property_count_u16_elems(const struct device_node *np, const char *propname)
{
    return of_property_count_elems_of_size(np, propname, sizeof(uint16_t));
}

static inline int of_property_count_u32_elems(const struct device_node *np, const char *propname)
{
    return of_property_count_elems_of_size(np, propname, sizeof(uint32_t));
}

static inline int of_property_count_u64_elems(const struct device_node *np, const char *propname)
{
    return of_property_count_elems_of_size(np, propname, sizeof(uint64_t));
}

int of_property_read_string(const struct device_node *np, const char *propname,
                            const char **out_string);

int of_property_match_string(const struct device_node *np, const char *propname,
                             const char *string);
const char *of_prop_next_string(struct property *prop, const char *cur);

int of_property_read_string_helper(const struct device_node *np, const char *propname,
                                   const char **out_strs, size_t sz, int skip);

static inline int of_property_read_string_array(const struct device_node *np, const char *propname,
                                                const char **out_strs, size_t sz)
{
    return of_property_read_string_helper(np, propname, out_strs, sz, 0);
}

static inline int of_property_count_strings(const struct device_node *np, const char *propname)
{
    return of_property_read_string_helper(np, propname, NULL, 0, 0);
}

static inline int of_property_read_string_index(const struct device_node *np, const char *propname,
                                                int index, const char **output)
{
    int rc = of_property_read_string_helper(np, propname, output, 1, index);
    return rc < 0 ? rc : 0;
}

static inline bool of_property_read_bool(const struct device_node *np, const char *propname)
{
    struct property *prop = of_find_property(np, propname, NULL);

    return prop ? true : false;
}

int of_property_read_variable_u8_array(const struct device_node *np, const char *propname,
                                       uint8_t *out_values, size_t sz_min, size_t sz_max);

int of_property_read_variable_u16_array(const struct device_node *np, const char *propname,
                                        uint16_t *out_values, size_t sz_min, size_t sz_max);

int of_property_read_variable_u32_array(const struct device_node *np, const char *propname,
                                        uint32_t *out_values, size_t sz_min, size_t sz_max);

int of_property_read_variable_u64_array(const struct device_node *np, const char *propname,
                                        uint64_t *out_values, size_t sz_min, size_t sz_max);
static inline int of_property_read_u8_array(const struct device_node *np, const char *propname,
                                            uint8_t *out_values, size_t sz)
{
    int ret = of_property_read_variable_u8_array(np, propname, out_values, sz, 0);
    if (ret >= 0)
        return 0;
    else
        return ret;
}

static inline int of_property_read_u16_array(const struct device_node *np, const char *propname,
                                             uint16_t *out_values, size_t sz)
{
    int ret = of_property_read_variable_u16_array(np, propname, out_values, sz, 0);
    if (ret >= 0)
        return 0;
    else
        return ret;
}

static inline int of_property_read_u32_array(const struct device_node *np, const char *propname,
                                             uint32_t *out_values, size_t sz)
{
    int ret = of_property_read_variable_u32_array(np, propname, out_values, sz, 0);
    if (ret >= 0)
        return 0;
    else
        return ret;
}

static inline int of_property_read_u64_array(const struct device_node *np, const char *propname,
                                             uint64_t *out_values, size_t sz)
{
    int ret = of_property_read_variable_u64_array(np, propname, out_values, sz, 0);
    if (ret >= 0)
        return 0;
    else
        return ret;
}

static inline int of_property_read_u8(const struct device_node *np, const char *propname,
                                      uint8_t *out_value)
{
    return of_property_read_u8_array(np, propname, out_value, 1);
}

static inline int of_property_read_u16(const struct device_node *np, const char *propname,
                                       uint16_t *out_value)
{
    return of_property_read_u16_array(np, propname, out_value, 1);
}

static inline int of_property_read_u32(const struct device_node *np, const char *propname,
                                       uint32_t *out_value)
{
    return of_property_read_u32_array(np, propname, out_value, 1);
}

static inline int of_property_read_s32(const struct device_node *np, const char *propname,
                                       int32_t *out_value)
{
    return of_property_read_u32(np, propname, (uint32_t *)out_value);
}

int of_property_read_u64(const struct device_node *np, const char *propname, uint64_t *out_value);

/* Helper to read a big number; size is in cells (not bytes) */
static inline uint64_t of_read_number(const fdt32_t *cell, int size)
{
    uint64_t r = 0;
    for (; size--; cell++)
        r = (r << 32) | fdt32_to_cpu(*cell);
    return r;
}

/* Like of_read_number, but we want an unsigned long result */
static inline unsigned long of_read_ulong(const fdt32_t *cell, int size)
{
    /* toss away upper bits if unsigned long is smaller than u64 */
    return of_read_number(cell, size);
}

const fdt32_t *of_prop_next_u32(struct property *prop, const fdt32_t *cur, uint32_t *pu);

#define of_property_for_each_string(np, propname, prop, s)                                         \
    for (prop = of_find_property(np, propname, NULL), s = of_prop_next_string(prop, NULL); s;      \
         s = of_prop_next_string(prop, s))

const struct of_device_id *of_match_node(const struct of_device_id *matches,
                                         const struct device_node *node);

int of_device_is_compatible(const struct device_node *device, const char *compat);

static inline struct device_node *of_node_get(struct device_node *node)
{
    return node;
}

static inline void of_node_put(struct device_node *node) {}

struct device_node *__of_find_all_nodes(struct device_node *prev);
struct device_node *of_find_all_nodes(struct device_node *prev);

#define for_each_of_allnodes_from(from, dn)                                                        \
    for (dn = __of_find_all_nodes(from); dn; dn = __of_find_all_nodes(dn))
#define for_each_of_allnodes(dn) for_each_of_allnodes_from(NULL, dn)

struct device_node *of_find_compatible_node(struct device_node *from, const char *type,
                                            const char *compatible);

struct device_node *of_find_node_with_property(struct device_node *from, const char *prop_name);

int __of_parse_phandle_with_args(const struct device_node *np, const char *list_name,
                                 const char *cells_name, int cell_count, int index,
                                 struct of_phandle_args *out_args);

static inline struct device_node *of_parse_phandle(const struct device_node *np,
                                                   const char *phandle_name, int index)
{
    struct of_phandle_args args;

    if (__of_parse_phandle_with_args(np, phandle_name, NULL, 0, index, &args))
        return NULL;

    return args.np;
}

int of_phandle_iterator_init(struct of_phandle_iterator *it, const struct device_node *np,
                             const char *list_name, const char *cells_name, int cell_count);

int of_phandle_iterator_next(struct of_phandle_iterator *it);
int of_phandle_iterator_args(struct of_phandle_iterator *it, uint32_t *args, int size);

struct device_node *of_find_node_by_phandle(phandle handle);

#define of_for_each_phandle(it, err, np, ln, cn, cc)                                               \
    for (of_phandle_iterator_init((it), (np), (ln), (cn), (cc)),                                   \
         err = of_phandle_iterator_next(it);                                                       \
         err == 0; err = of_phandle_iterator_next(it))

#define for_each_child_of_node(parent, child)                                                      \
    for (child = of_get_next_child(parent, NULL); child != NULL;                                   \
         child = of_get_next_child(parent, child))

extern int of_count_phandle_with_args(const struct device_node *np, const char *list_name,
                                      const char *cells_name);

extern int __of_parse_phandle_with_args(const struct device_node *np, const char *list_name,
                                        const char *cells_name, int cell_count, int index,
                                        struct of_phandle_args *out_args);

/**
 * of_parse_phandle_with_args() - Find a node pointed by phandle in a list
 * @np:		pointer to a device tree node containing a list
 * @list_name:	property name that contains a list
 * @cells_name:	property name that specifies phandles' arguments count
 * @index:	index of a phandle to parse out
 * @out_args:	optional pointer to output arguments structure (will be filled)
 *
 * This function is useful to parse lists of phandles and their arguments.
 * Returns 0 on success and fills out_args, on error returns appropriate
 * errno value.
 *
 * Caller is responsible to call of_node_put() on the returned out_args->np
 * pointer.
 *
 * Example::
 *
 *  phandle1: node1 {
 *	#list-cells = <2>;
 *  };
 *
 *  phandle2: node2 {
 *	#list-cells = <1>;
 *  };
 *
 *  node3 {
 *	list = <&phandle1 1 2 &phandle2 3>;
 *  };
 *
 * To get a device_node of the ``node2`` node you may call this:
 * of_parse_phandle_with_args(node3, "list", "#list-cells", 1, &args);
 */
static inline int of_parse_phandle_with_args(const struct device_node *np, const char *list_name,
                                             const char *cells_name, int index,
                                             struct of_phandle_args *out_args)
{
    int cell_count = -1;

    /* If cells_name is NULL we assume a cell count of 0 */
    if (!cells_name)
        cell_count = 0;

    return __of_parse_phandle_with_args(np, list_name, cells_name, cell_count, index, out_args);
}

/**
 * of_phandle_args_equal() - Compare two of_phandle_args
 * @a1:		First of_phandle_args to compare
 * @a2:		Second of_phandle_args to compare
 *
 * Return: True if a1 and a2 are the same (same node pointer, same phandle
 * args), false otherwise.
 */
static inline bool of_phandle_args_equal(const struct of_phandle_args *a1,
                                         const struct of_phandle_args *a2)
{
    return a1->np == a2->np && a1->args_count == a2->args_count &&
           !memcmp(a1->args, a2->args, sizeof(a1->args[0]) * a1->args_count);
}

uint32_t ofnode_read_u32_default(const struct device_node *np, const char *propname,
                                 uint32_t default_value);

#endif /* __OF_H__ */
