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

#include <atomic.h>
#include <driver/class/chardev.h>
#include <driver/device.h>
#include <driver/of.h>
#include <errno.h>
#include <stdio.h>

#ifdef __x86_64__
/* x86 的前四个COM 必须是fixed否则会对应出错 */
static uint64_t g_serial_minor_pool = 0xf;
#else
static uint64_t g_serial_minor_pool = 0;
#endif

static void set_minor(int minor)
{
    g_serial_minor_pool |= (1 << minor);
}

static int get_free_minor(void)
{
    for (int i = 0; i < 64; i++)
    {
        if (!(g_serial_minor_pool & (1 << i)))
        {
            set_minor(i);
            return i;
        }
    }
    return -1;
}

int serial_register(struct device *dev)
{
    char *path;
    const char *eupath;
    struct char_class_type *class = (struct char_class_type *)dev->class;
    int ret;
    if (!dev->is_fixed_minor)
        dev->minor = get_free_minor();
    asprintf(&path, "/dev/ttyS%d", dev->minor);
    if (path == NULL)
    {
        return -ENOMEM;
    }

    eupath = earlycon_uart_of_path();
#ifdef __x86_64__
    if (eupath && !strcmp(eupath, path))
#else
    if (eupath && !strcmp(eupath, dev->of_node->full_name))
#endif
    {
        class->isconsole = true;
    }
    else
    {
        class->isconsole = false;
    }

    ret = char_register(path, dev);

    if (class->isconsole)
    {
        symlink(path, "/dev/console");
    }
    free(path);
    return ret;
}
