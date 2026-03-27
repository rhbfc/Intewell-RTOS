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

#include <driver/of.h>
#include <errno.h>
#include <system/types.h>
/**
 * of_count_phandle_with_args() - Find the number of phandles references in a property
 * @np:		pointer to a device tree node containing a list
 * @list_name:	property name that contains a list
 * @cells_name:	property name that specifies phandles' arguments count
 *
 * Return: The number of phandle + argument tuples within a property. It
 * is a typical pattern to encode a list of phandle and variable
 * arguments into a single property. The number of arguments is encoded
 * by a property in the phandle-target node. For example, a gpios
 * property would contain a list of GPIO specifies consisting of a
 * phandle and 1 or more arguments. The number of arguments are
 * determined by the #gpio-cells property in the node pointed to by the
 * phandle.
 */
int of_count_phandle_with_args(const struct device_node *np, const char *list_name,
                const char *cells_name)
{
    struct of_phandle_iterator it;
    int rc, cur_index = 0;

    /*
     * If cells_name is NULL we assume a cell count of 0. This makes
     * counting the phandles trivial as each 32bit word in the list is a
     * phandle and no arguments are to consider. So we don't iterate through
     * the list but just use the length to determine the phandle count.
     */
    if (!cells_name) {
        const __be32 *list;
        int size;

        list = of_get_property(np, list_name, &size);
        if (!list)
            return -ENOENT;

        return size / sizeof(*list);
    }

    rc = of_phandle_iterator_init(&it, np, list_name, cells_name, -1);
    if (rc)
        return rc;

    while ((rc = of_phandle_iterator_next(&it)) == 0)
        cur_index += 1;

    if (rc != -ENOENT)
        return rc;

    return cur_index;
}

int __of_parse_phandle_with_args(const struct device_node *np,
                 const char *list_name,
                 const char *cells_name,
                 int cell_count, int index,
                 struct of_phandle_args *out_args)
{
    struct of_phandle_iterator it;
    int rc, cur_index = 0;

    if (index < 0)
        return -EINVAL;

    /* Loop over the phandles until all the requested entry is found */
    of_for_each_phandle(&it, rc, np, list_name, cells_name, cell_count)
    {
/*
 * All of the error cases bail out of the loop, so at
 * this point, the parsing is successful. If the requested
 * index matches, then fill the out_args structure and return,
 * or return -ENOENT for an empty entry.
 */
        rc = -ENOENT;
        if (cur_index == index) {
            if (!it.phandle)
                goto err;

            if (out_args) {
                int c;

                c = of_phandle_iterator_args(&it,
                                 out_args->args,
                                 MAX_PHANDLE_ARGS);
                out_args->np = it.node;
                out_args->args_count = c;
            }
            else {
                of_node_put(it.node);
            }

            /* Found it! return success */
            return 0;
        }

        cur_index++;
    }

    /*
     * Unlock node before returning result; will be one of:
     * -ENOENT : index is for empty phandle
     * -EINVAL : parsing error on data
     */

err:
    of_node_put(it.node);
    return rc;
}